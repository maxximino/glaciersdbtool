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
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <boost/date_time/posix_time/ptime.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <map>
#include <memory>
#include "config.h"
#include "cryptoutils.h"

using namespace std;
using boost::format;
string URIencode(string in);
size_t curl_readfunction( void* ptr, size_t size, size_t nmemb, void* userdata);
size_t curl_writefunction( char* ptr, size_t size, size_t nmemb, void* userdata);
size_t curl_headerfunction( void* ptr, size_t size, size_t nmemb, void* userdata);
string FormatTS(boost::posix_time::ptime time,string format);
class AWSRequest
{

	public:
		AWSRequest():querystring(),reqheaders(),respheaders() {
			host="";
			method="";
			uri="/";
			accesskey="";
			secretkey="";
			payload=NULL;
			payloadsize=0;
			payloadsent=0;
			already_signed=false;
			outstream=NULL;
		}
		void setHost(string h) {
			host=boost::algorithm::to_lower_copy(h);
		}
		void setMethod(string m) {
			method=boost::algorithm::to_upper_copy(m);
		}
		void setURI(string u) {
			uri=u;
		}
		void addQueryStringParameter(string key,string value) {
			querystring.insert(std::pair<string,string>(key,value));
		}
		void addReqHeader(string key, string value) {
			reqheaders.insert(std::pair<string,string>(key,value));
		}
		void setPayload(void* ptr,unsigned long size) {
			payload=ptr;
			payloadsize=size;
		}
		void setAWSAccessKey(string acc) {
			accesskey=acc;
		}
		void setAWSSecretKey(string sec) {
			secretkey=sec;
		}
		void setAWSRegion(string r) {
			region=r;
		}
		void setAWSService(string s) {
			service=s;
		}
		void setOutputStream(ostream* o) {
			outstream=o;
		}
		void execute();
		void cleanup();
		void print_as_wget();
		static shared_ptr<AWSRequest> getGlacierRequest(Config& cfg);
		static shared_ptr<AWSRequest> getSimpleDBRequest(Config& cfg);
		void execute_and_verify(int expectedStatusCode);
		long getHttpResponseCode() {
			return http_status_code;
		}
		map<string,string> getResponseHeaders() {
			return respheaders;
		}
	protected:
		ostream* outstream;
		string host;
		string method;
		string uri;
		string accesskey;
		string secretkey;
		string region;
		string service;
		map<string,string> querystring; //map  with string as key is sorted by default per ascii code of the key. I *need* this behaviour per AWS specifications.
		map<string,string> reqheaders;
		map<string,string> respheaders;
		long http_status_code;
		void* payload;
		unsigned long payloadsent;
		unsigned long payloadsize;
		bool already_signed;
		virtual void sign()=0;
		string buildUrl();
		string buildQS();
		friend size_t curl_writefunction( char* ptr, size_t size, size_t nmemb, void* userdata);
		friend size_t curl_readfunction( void* ptr, size_t size, size_t nmemb, void* userdata);
		friend size_t curl_headerfunction( void* ptr, size_t size, size_t nmemb, void* userdata);
};

class AWSRequestV4: public AWSRequest
{

	public:
		AWSRequestV4():AWSRequest() {}
	protected:
		bitset<256> deriveSigningKey(boost::posix_time::ptime date);
		string getSignedHeaders();
		string canonicalheaders();
		virtual void sign();
};

class AWSRequestV2: public AWSRequest
{

	public:
		AWSRequestV2():AWSRequest() {}
	protected:
		virtual void sign();
};
