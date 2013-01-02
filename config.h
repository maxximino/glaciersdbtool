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
#include <string>
#include <tclap/CmdLine.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/exception/all.hpp>
using namespace std;
class ConfigException : public boost::exception {};

class Config
{
	public:
		void addArgs(TCLAP::CmdLine& cmd);
		void loadConfig();
		string getVault() {return vault;}
		string getRegion() {return aws_region;}
		string getAWSAccessKey() {return aws_access;}
		string getAWSSecretKey() {return aws_secret;}
		boost::property_tree::ptree& getRunningConfig();
		void putRunningConfig(boost::property_tree::ptree cfg);
		string getSDBDomain() {return sdbdomain;}
		int getChunkSize() {return transmission_chunk_size;}
		int getMaxArchiveSize() {return archive_max_size;}
		Config():
			tc_config("c", "config", "Path to configuration file", false, string(getenv("HOME")).append("/.glaciersdbtool_rc"), "path"),
			tc_vault("v", "vault", "Glacier's vault name", false, "", "string"),
			tc_sdbdomain("d", "sdbdomain", "SimpleDB domain", false, "", "string"),
			tc_aws_access("a", "awsaccesskey", "AWS Access key", false, "", "string"),
			tc_aws_secret("s", "awssecretkey", "AWS Secret key", false, "", "string"),
			tc_aws_region("r", "awsregion", "AWS region", false, "", "string"),
			tc_transmission_chunk("p", "partsize", "Maximum part size in MB (Amazon multi-part upload)", false, 0, "MB"),
			tc_archive_max("m", "maxsize", "Maximum Glacier archive size in MB (max 4096, default 2048)", false, 0, "MB") {
			configdata=NULL;
			configfile="";
			vault="";
			aws_region="";
			aws_access="";
			aws_secret="";
			sdbdomain="";
			transmission_chunk_size=0;
			archive_max_size=0;
		}
		~Config() {
			if(configdata!=NULL) {
				delete configdata;
			}
		}
	protected:
		string configfile;
		string vault;
		string aws_access;
		string aws_secret;
		string aws_region;
		string sdbdomain;
		int transmission_chunk_size;
		int archive_max_size;
		boost::property_tree::ptree* configdata;
		template <typename T>
		T getFromConfigFile(string key,T defaultvalue);
		template <typename T>
		T evaluateArg(TCLAP::ValueArg<T>& cmdarg,string configkey,T fakevalue, std::function<bool(T&)> validate, string errmsg);
	private:
		void loadConfigFile();
		TCLAP::ValueArg<string> tc_config;
		TCLAP::ValueArg<string>  tc_vault;
		TCLAP::ValueArg<string>  tc_sdbdomain;
		TCLAP::ValueArg<string>  tc_aws_access;
		TCLAP::ValueArg<string>  tc_aws_secret;
		TCLAP::ValueArg<string>  tc_aws_region;
		TCLAP::ValueArg<int>  tc_transmission_chunk;
		TCLAP::ValueArg<int>  tc_archive_max;

};


