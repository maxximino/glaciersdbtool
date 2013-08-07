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
#include <unistd.h>
#include "awsrequest.h"
class WebResponseException: public boost::exception {};

#include <boost/algorithm/string/trim.hpp>
string URIencode(string in)
{
	string ret;
	CURL* handle = curl_easy_init();
	char* encoded = curl_easy_escape(handle,in.c_str(),in.length());
	ret = string(encoded);
	curl_free(encoded);
	curl_easy_cleanup(handle);
	return ret;
}

string FormatTS(boost::posix_time::ptime time,string format)
{
	stringstream ss("");
	boost::posix_time::time_facet* facet = new boost::posix_time::time_facet(format.c_str());
	ss.imbue(locale(ss.getloc(), facet));
	ss << time;
	return ss.str();
}
size_t curl_readfunction( void* ptr, size_t size, size_t nmemb, void* userdata)
{
	size_t maxsize = size*nmemb;
	AWSRequest* req =(AWSRequest*)userdata;
	size_t remaining = req->payloadsize - req->payloadsent;
	size_t tocopy=std::min<size_t>(maxsize,remaining);
	if(tocopy > 0 ) {
		memcpy(ptr,(char*)req->payload+req->payloadsent,tocopy);
		req->payloadsent += tocopy;
	}
	return tocopy;
}
size_t curl_writefunction( char* ptr, size_t size, size_t nmemb, void* userdata)
{
	size_t s = size*nmemb;
	AWSRequest* req =(AWSRequest*)userdata;
	if(req->outstream!= NULL) {
		req->outstream->write(ptr,s);
	}
	return s;
}
size_t curl_headerfunction( void* ptr, size_t size, size_t nmemb, void* userdata)
{
	using namespace boost::algorithm;
	size_t s = size*nmemb;
	AWSRequest* req =(AWSRequest*)userdata;
	string header=trim_copy(string((char*)ptr,s));
	if(header.length() == 0) {return s;}
	int pos= header.find(':',0);
	if(pos==string::npos) {
		req->respheaders.insert(std::pair<string,string>(header,""));
	} else {
		req->respheaders.insert(std::pair<string,string>(trim_copy(header.substr(0,pos)),trim_copy(header.substr(pos+1))));
	}
	return s;
}
shared_ptr<AWSRequest> AWSRequest::getGlacierRequest(Config& cfg)
{
	auto req = shared_ptr<AWSRequest>(new AWSRequestV4());
	req->setAWSAccessKey(cfg.getAWSAccessKey());
	req->setAWSSecretKey(cfg.getAWSSecretKey());
	req->setAWSRegion(cfg.getRegion());
	req->setAWSService("glacier");
	req->setHost((format("glacier.%s.amazonaws.com") % cfg.getRegion()).str());
	req->addReqHeader("x-amz-glacier-version","2012-06-01");
	return shared_ptr<AWSRequest>(req);
}
shared_ptr<AWSRequest> AWSRequest::getSimpleDBRequest(Config& cfg)
{
	auto req = shared_ptr<AWSRequest>(new AWSRequestV2());
	req->setAWSAccessKey(cfg.getAWSAccessKey());
	req->setAWSSecretKey(cfg.getAWSSecretKey());
	req->setHost("sdb.amazonaws.com");
	req->setMethod("GET");
	req->setURI("/");
	req->addQueryStringParameter("DomainName",cfg.getSDBDomain());
	req->addQueryStringParameter("Version","2009-04-15");
	return shared_ptr<AWSRequest>(req);
}

void AWSRequest::print_as_wget()
{
	sign();
	cout << "wget " << buildUrl() << " ";
	if(method =="GET") {

	} else {
		cerr << "UNIMPLEMENTED: " << method << " AS WGET COMMAND";
	}
for(std::pair<string,string> p: reqheaders) {
		if(p.first.length() < 2) {continue;}
		cout << "--header '" << (format("%s: %s") % p.first % p.second ).str() << "' ";
	}
	cout << endl;
}
void AWSRequest::cleanup(){
   payloadsent = 0;
   respheaders.clear();
}
void AWSRequest::execute()
{
	sign();
	CURL* handle = curl_easy_init();
	struct curl_slist* headerlist=NULL;
	curl_easy_setopt(handle,CURLOPT_URL,buildUrl().c_str());
	if(method =="POST") {
		curl_easy_setopt(handle,CURLOPT_POST,1);
#define REMOVE_HEADER(x) if(reqheaders.count(x)==0){ reqheaders.insert(std::pair<string,string>(x,"")); }
		REMOVE_HEADER("Expect");
		REMOVE_HEADER("Accept");
		REMOVE_HEADER("Content-Type");
		REMOVE_HEADER("Content-Length");

	} else if(method=="PUT") {
		curl_easy_setopt(handle,CURLOPT_UPLOAD,1);
		REMOVE_HEADER("Content-Length");
	} else if(method=="GET") {

	} else if(method=="DELETE") {
		curl_easy_setopt(handle,CURLOPT_NOBODY,1);
		curl_easy_setopt(handle,CURLOPT_CUSTOMREQUEST,"DELETE");

	}
#undef REMOVE_HEADER
for(std::pair<string,string> p: reqheaders) {
		if(p.first.length() < 2) {continue;}
		headerlist=curl_slist_append(headerlist,(format("%s: %s") % p.first % p.second ).str().c_str());
	}

	curl_easy_setopt(handle,CURLOPT_HTTPHEADER,headerlist);
	curl_easy_setopt(handle,CURLOPT_WRITEFUNCTION,curl_writefunction);
	curl_easy_setopt(handle,CURLOPT_WRITEDATA,this);
	curl_easy_setopt(handle,CURLOPT_READFUNCTION,curl_readfunction);
	curl_easy_setopt(handle,CURLOPT_READDATA,this);
	curl_easy_setopt(handle,CURLOPT_HEADERFUNCTION,curl_headerfunction);
	curl_easy_setopt(handle,CURLOPT_HEADERDATA,this);
	curl_easy_perform(handle);
	curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&http_status_code);
	curl_slist_free_all(headerlist);
	curl_easy_cleanup(handle);
}
void AWSRequest::execute_and_verify(int expectedStatusCode)
{
	unsigned long timeo = 1;
	const unsigned long maxtimeo = 65; //Slightly less than 40 seconds of total sleeping time between retries.
	stringstream out("");
	do{
	  if(timeo!=1){
	    usleep(300000*timeo); //300ms * timeo
	    cerr << "Retrying failed request:" << buildUrl() << endl;
	  }
	  cleanup();
	  out.str("");
	  setOutputStream(&out);
	  execute();
	  timeo <<=1;
	}
	while((getHttpResponseCode() != expectedStatusCode) && (timeo < maxtimeo));
	if(getHttpResponseCode() != expectedStatusCode) {
		stringstream err("Headers:");
		err << endl;
		for(pair<string,string>p : getResponseHeaders()) {
			err << p.first << ":" << p.second << endl;
		}
		err << endl << endl << out.str();
		throw WebResponseException() << boost::error_info<string,string>(err.str());
	}
}

string AWSRequest::buildUrl()
{
	string qs = buildQS();
	if(qs.length() > 0 ) {
		return (format("http://%s%s?%s") % host % uri % buildQS()).str();
	} else {
		return (format("http://%s%s") % host % uri ).str();
	}
}
string AWSRequest::buildQS()
{
	stringstream ss;
	ss.str("");
for( std::pair<string,string> p : querystring) {
		if(ss.tellp() > 0) {
			ss << "&";
		}
		ss << URIencode(p.first) << "=" << URIencode(p.second);
	}
	return ss.str();
}
bitset<256> AWSRequestV4::deriveSigningKey(boost::posix_time::ptime date)
{
	string tmpstring=FormatTS(date,"%Y%m%d");
	bitset<256> tmp1 = HMAC256(string("AWS4").append(secretkey),tmpstring);
	bitset<256> tmp2= HMAC256(tmp1,region);
	tmp1=HMAC256(tmp2,service);
	tmpstring="aws4_request";
	tmp2=HMAC256(tmp1,tmpstring);
	return tmp2;
}
string AWSRequestV4::getSignedHeaders()
{
	stringstream ss("");
for(std::pair<string,string> p: reqheaders) {
		if(ss.tellp() > 0) {
			ss << ";";
		}
		ss << boost::algorithm::to_lower_copy(p.first);
	}
	return ss.str();
}
string AWSRequestV4::canonicalheaders()
{
	stringstream ss("");
for(std::pair<string,string> p: reqheaders) {
		ss << boost::algorithm::to_lower_copy(p.first) << ":" << p.second << "\n";
	}
	return ss.str();
}
void AWSRequestV4::sign()
{
	if(already_signed) return;
	already_signed=true;
	boost::posix_time::ptime pt = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
	addReqHeader("Host",host);
	addReqHeader("Date",FormatTS(pt,"%Y%m%dT%H%M%SZ"));
	stringstream canonicalrequest("");
	canonicalrequest << method << "\n";
	canonicalrequest << uri << "\n";
	canonicalrequest << buildQS() << "\n";
	canonicalrequest << canonicalheaders() << "\n";
	canonicalrequest << getSignedHeaders() << "\n";
	SHA256Hash hash;
	hash.addData(payload,payloadsize);
	canonicalrequest << hash.asHexString();
	//cout << "Canonical request: " <<canonicalrequest.str() <<endl;
	SHA256Hash hash2;
	hash2.addData(canonicalrequest.str().c_str(),canonicalrequest.str().length());
	string stringtosign = (format("AWS4-HMAC-SHA256\n%s\n%s/%s/%s/aws4_request\n%s") % FormatTS(pt,"%Y%m%dT%H%M%SZ") % FormatTS(pt,"%Y%m%d") % region % service % hash2.asHexString()).str();
	//cout << "signstring: " <<stringtosign<<endl;
	string signature = HMAC256_ashex(deriveSigningKey(pt),stringtosign);
	string autheader = (format("AWS4-HMAC-SHA256 Credential=%s/%s/%s/%s/aws4_request, SignedHeaders=%s, Signature=%s") % accesskey % FormatTS(pt,"%Y%m%d") % region % service % getSignedHeaders() % signature).str();
	addReqHeader("Authorization",autheader);
}
void AWSRequestV2::sign()
{
	if(already_signed) return;
	boost::posix_time::ptime pt = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
	addReqHeader("Host",host);
	addQueryStringParameter("AWSAccessKeyId",accesskey);
	addQueryStringParameter("Timestamp",FormatTS(pt,"%Y-%m-%dT%H:%M:%S.000Z"));
	addQueryStringParameter("SignatureVersion","2");
	addQueryStringParameter("SignatureMethod","HmacSHA256");
	stringstream canonicalrequest("");
	canonicalrequest << method << "\n";
	canonicalrequest << boost::to_lower_copy(host) << "\n";
	canonicalrequest << uri << "\n";
	canonicalrequest << buildQS();
	bitset<256> sign = HMAC256(secretkey,canonicalrequest.str());
	addQueryStringParameter("Signature",boost::trim_copy(base64((unsigned char*)&sign,32)));
}
