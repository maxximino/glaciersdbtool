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
#include "config.h"
#include "boost/property_tree/ptree.hpp"
#include "boost/exception/all.hpp"
#include "boost/property_tree/info_parser.hpp"
void Config::addArgs(TCLAP::CmdLine& cmd)
{
	cmd.add(tc_config);
	cmd.add(tc_vault);
	cmd.add(tc_sdbdomain);
	cmd.add(tc_aws_access);
	cmd.add(tc_aws_secret);
	cmd.add(tc_transmission_chunk);
	cmd.add(tc_archive_max);
}
void Config::loadConfig()
{
	vault = evaluateArg<string>(tc_vault,"vault","",
	[](string& v) {return (v.length() > 0);},
	"Invalid Glacier's vault name.");
	aws_access = evaluateArg<string>(tc_aws_access,"awsaccess","",
	[](string& v) {return (v.length() > 0);},
	"Invalid AWS Access key.");
	aws_secret = evaluateArg<string>(tc_aws_secret,"awssecret","",
	[](string& v) {return (v.length() > 0);},
	"Invalid AWS Secret key.");
	sdbdomain = evaluateArg<string>(tc_sdbdomain,"sdbdomain","",
	[](string& v) {return (v.length() > 0);},
	"Invalid SimpleDB domain.");
	aws_region = evaluateArg<string>(tc_aws_region,"awsregion","",
	[](string& v) {return (v.length() > 0);},
	"Invalid AWS region.");
	archive_max_size = evaluateArg<int>(tc_archive_max,"max-archive-size",2048,
	[](int v) {return v > 0 && v < 4096;},
	"Invalid Glacier's max archive size.");
	transmission_chunk_size = evaluateArg<int>(tc_transmission_chunk,"part-size",8,
	[this](int v) {return v > 0 && v < ceil(archive_max_size/2) && ((v & (v - 1))==0);},
	"Invalid AWS Multi-part upload part size. Should be a power of two.");

}
template <typename T>
T Config::evaluateArg(TCLAP::ValueArg< T >& cmdarg,string configkey,T defaultvalue, std::function< bool(T&) > validate, string errmsg)
{
	T param;
	if(cmdarg.isSet()) {
		param=cmdarg.getValue();
	} else {
		param = this->getFromConfigFile<T>(configkey,defaultvalue);
	}
	if(!validate(param)) {
		throw ConfigException() << boost::error_info<string,string>(errmsg);
	}
	configdata->put<T>(configkey,param);
	return param;
}
void Config::putRunningConfig(boost::property_tree::ptree cfg)
{
	*configdata=cfg;
}

boost::property_tree::ptree& Config::getRunningConfig()
{
	return *configdata;
}

void Config::loadConfigFile()
{
	using namespace boost::property_tree;
	if(configdata != NULL) {return;}
	configdata = new ptree();
	read_info(tc_config.getValue(),*configdata);
}
template <typename T>
T Config::getFromConfigFile(string key,T defaultvalue)
{
	this->loadConfigFile();
	T tmp = configdata->get<T>(key,defaultvalue);
	return tmp;
}
