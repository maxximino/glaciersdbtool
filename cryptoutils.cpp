/*
Copyright (C) 2012	Massimo Maggi

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include "cryptoutils.h"
#include <openssl/buffer.h>
#include <openssl/bio.h>

unsigned long Endian_QWord_Conversion(unsigned long dword)
{
	return	((dword>>56)&0x00000000000000FF) |
		((dword>>40)&0x000000000000FF00) |
		((dword>>24)&0x0000000000FF0000) |
		((dword>> 8)&0x00000000FF000000) |
		((dword<< 8)&0x000000FF00000000) |
		((dword<<24)&0x0000FF0000000000) |
		((dword<<40)&0x00FF000000000000) |
		((dword<<56)&0xFF00000000000000) ;
}
ostream& operator<<( ostream& out, const bitset<256> L )
{
	unsigned long c1;
	unsigned long c2;
	unsigned long c3;
	unsigned long c4;
	unsigned char* ptr=(unsigned char*)&L;
	memcpy(&c1,ptr,8);
	memcpy(&c2,ptr+8,8);
	memcpy(&c3,ptr+16,8);
	memcpy(&c4,ptr+24,8);
	out<< hex << std::setw(16) <<std::setfill('0')<< Endian_QWord_Conversion(c1);
	out<< hex << std::setw(16) <<std::setfill('0')<< Endian_QWord_Conversion(c2);
	out<< hex << std::setw(16) <<std::setfill('0')<< Endian_QWord_Conversion(c3);
	out<< hex << std::setw(16) <<std::setfill('0')<< Endian_QWord_Conversion(c4);
	return out;
}

bitset<256> HMAC256(const string& key,const  string& data)
{
	HMAC_CTX ctx;
	bitset<256> retval;
	unsigned int len = 32;
	HMAC_CTX_init(&ctx);
	HMAC_Init(&ctx,key.c_str(),key.length(),EVP_sha256());
	HMAC_Update(&ctx,(const unsigned char*)data.c_str(),data.length());
	HMAC_Final(&ctx,(unsigned char*)&retval,&len);
	HMAC_CTX_cleanup(&ctx);
	return retval;
}
bitset<256> HMAC256(const bitset<256>& key,const  string& data)
{
	HMAC_CTX ctx;
	bitset<256> retval;
	unsigned int len = 32;
	HMAC_CTX_init(&ctx);
	HMAC_Init(&ctx,&key,32,EVP_sha256());
	HMAC_Update(&ctx,(const unsigned char*)data.c_str(),data.length());
	HMAC_Final(&ctx,(unsigned char*)&retval,&len);
	HMAC_CTX_cleanup(&ctx);
	return retval;
}
string HMAC256_ashex(const bitset<256>& key,const  string& data)
{
	stringstream ss;
	ss.str("");
	bitset<256> val = HMAC256(key,data);
	ss << val;
	return ss.str();
}
//Credits to Carson McDonald
string base64(const unsigned char* input, int length)
{
	BIO* bmem, *b64;
	BUF_MEM* bptr;

	b64 = BIO_new(BIO_f_base64());
	bmem = BIO_new(BIO_s_mem());
	b64 = BIO_push(b64, bmem);
	BIO_write(b64, input, length);
	BIO_flush(b64);
	BIO_get_mem_ptr(b64, &bptr);

	char* buff = (char*)malloc(bptr->length+1);
	memcpy(buff, bptr->data, bptr->length);
	buff[bptr->length] = 0;
	string ret =string(buff,bptr->length);
	BIO_free_all(b64);
	free(buff);
	return ret;
}

string HashAlgorithm::asHexString()
{
	unsigned int size = 256;
	stringstream ss("");
	unsigned char* buffer = (unsigned char*)malloc(size);
	asBinary(buffer,&size);
	ss << std::hex << std::setfill('0');
	for(int i=0; i < size; ++i) {
		unsigned short s = buffer[i];
		ss << std::setw(2) << s;
	}
	free(buffer);
	return ss.str();
}
void OpenSSLHash::addData(const void* ptr, size_t size)
{
	if(EVP_DigestUpdate(ctx,ptr,size)!=1) {
		throw CryptoException();
	}
};
void OpenSSLHash::asBinary(void* ptr, unsigned int* size)
{
	EVP_MD_CTX* tempctx=EVP_MD_CTX_create();
	EVP_MD_CTX_copy_ex(tempctx,ctx);
	if(EVP_DigestFinal_ex(tempctx,(unsigned char*)ptr,size)!=1) {
		throw CryptoException();
	}
	EVP_MD_CTX_destroy(tempctx);
};

void OpenSSLHash::opensslhash_init(string hname)
{
	OpenSSL_add_all_algorithms();
	hashname=hname;
	md = EVP_get_digestbyname(hashname.c_str() );
	ctx=EVP_MD_CTX_create();
	if(EVP_DigestInit_ex(ctx,md,NULL)!=1) {
		throw CryptoException();
	}
};
OpenSSLHash::~OpenSSLHash()
{
	EVP_MD_CTX_destroy(ctx);
}

#define OneMB 1024*1024
SHA256TreeHash::SHA256TreeHash():mbhashes()
{
	remainder = (unsigned char*) malloc(1024*1024);
	remaindersize=0;
}
void SHA256TreeHash::reset()
{
	remaindersize=0;
	mbhashes.clear();
}

void SHA256TreeHash::addData(const void* ptr, size_t size)
{
	if(size==0) {return;}
	const unsigned char* myptr = (const unsigned char*)ptr;
	unsigned int ToBeCopied;
	while(size > 0 ) {
		ToBeCopied = std::min<size_t>(OneMB-remaindersize,size);
		memcpy(remainder+remaindersize,myptr,ToBeCopied);
		size -= ToBeCopied;
		remaindersize += ToBeCopied;
		myptr+=ToBeCopied;
		if(remaindersize==OneMB) {
			mbhashes.push_back(SHA256HashOnePart());
			remaindersize=0;
		}
	}

}
void SHA256TreeHash::asBinary(void* ptr, unsigned int* size)
{
	vector<bitset<256> >* tmp1 = new vector<bitset<256> >(mbhashes.cbegin(),mbhashes.cend());
	vector<bitset<256> >* tmp2 = new vector<bitset<256> >();
	if(remaindersize > 0 || mbhashes.size()==0) {
		tmp1->push_back(SHA256HashOnePart());
	}

	while(tmp1->size() > 1) {
		int idx;
		for(idx = 0; (idx+1) < tmp1->size(); idx+=2) {
			tmp2->push_back(SHA256HashDouble(tmp1->at(idx),tmp1->at(idx+1)));
		}
		if(tmp1->size()%2==1) {
			tmp2->push_back(tmp1->at(idx));
		}
		delete tmp1;
		tmp1=tmp2;
		tmp2 = new vector<bitset<256> >();
	}
	bitset<256> b = tmp1->at(0);
	memcpy(ptr,&b,32);
	*size=32;
	delete tmp1;
	delete tmp2;
}

SHA256TreeHash::~SHA256TreeHash()
{
	free(remainder);
}
bitset<256> SHA256TreeHash::SHA256HashOnePart()
{
	bitset<256> ret;
	SHA256Hash h;
	h.addData(remainder,remaindersize);
	h.asBinary(&ret,NULL);
	return ret;
}
bitset<256> SHA256TreeHash::SHA256HashDouble(bitset<256> p1,bitset<256> p2)
{
	bitset<256> ret;
	unsigned int s=32;
	SHA256Hash h;
	h.addData(&p1,32);
	h.addData(&p2,32);
	h.asBinary(&ret,&s);
	return ret;
}
