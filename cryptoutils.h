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
#pragma once
#include <string.h>
#include <string>
#include <bitset>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <list>
#include <vector>
#include <iomanip>
#include <openssl/evp.h>
#include <boost/exception/exception.hpp>
#include <boost/concept_check.hpp>
#include <bits/ios_base.h>
using namespace std;
unsigned long Endian_QWord_Conversion(unsigned long dword);
string base64(const unsigned char* input, int length);
ostream& operator<<( ostream& out, const bitset<256> L );
class CryptoUtils
{
	public:
		CryptoUtils();
};

class HashAlgorithm
{
	public:
		virtual void addData(const void* ptr, size_t size)=0;
		virtual void asBinary(void* ptr, unsigned int* size)=0;
		virtual string asHexString();
		virtual void reset()=0;
	protected:

};
class CryptoException: public boost::exception {};
#include <openssl/hmac.h>

bitset<256> HMAC256(const string& key,const  string& data);
bitset<256> HMAC256(const bitset<256>& key,const  string& data);
string HMAC256_ashex(const bitset<256>& key,const  string& data);

class OpenSSLHash: public HashAlgorithm
{
	public:
		virtual void addData(const void* ptr, size_t size);
		virtual void asBinary(void* ptr, unsigned int* size);
		virtual void reset() {
			EVP_MD_CTX_destroy(ctx);
			opensslhash_init(hashname);
		}
	protected:
		OpenSSLHash() {};
		string hashname;
		virtual void opensslhash_init(string hname);
		~OpenSSLHash();
		const EVP_MD* md;
		EVP_MD_CTX* ctx;
};
class SHA256Hash: public OpenSSLHash
{
	public:
		SHA256Hash() {
			opensslhash_init("sha256");
		}

};
class SHA512Hash: public OpenSSLHash
{
	public:
		SHA512Hash() {
			opensslhash_init("sha512");
		}
};
class MD5Hash: public OpenSSLHash
{
	public:
		MD5Hash() {
			opensslhash_init("sha256");
		}
};
class SHA256TreeHash: public HashAlgorithm
{
	public:
		SHA256TreeHash();
		virtual void addData(const void* ptr, size_t size);
		virtual void asBinary(void* ptr, unsigned int* size);
		~SHA256TreeHash();
		virtual void reset();
	protected:
		list<bitset<256> > mbhashes;
		unsigned char* remainder;
		long remaindersize;

		bitset<256> SHA256HashOnePart();
		bitset<256> SHA256HashDouble(bitset<256> p1,bitset<256> p2);
};
