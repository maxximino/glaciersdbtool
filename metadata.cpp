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
#include "metadata.h"

void metadata::parseMeta()
{
	boost::iostreams::stream<boost::iostreams::file_descriptor_source> metain(4,boost::iostreams::file_descriptor_flags::close_handle);
	string line;
	while(!metain.eof()) {
		getline(metain,line);
		boost::trim(line);
		if(line.length() == 0) {continue;}
		int pos = line.find("=");
		if(pos==string::npos) {
			throw MetadataException() << boost::error_info<string,string>((format("Invalid metadata line: %s") % line).str());
		}
		string key, value;
		key=line.substr(0,pos);
		value=line.substr(pos+1);
		boost::trim(key);
		boost::trim(value);
		if(key.length() == 0) {
			throw MetadataException() << boost::error_info<string,string>((format("Invalid metadata line: %s") % line).str());
		}
		metadati.insert(std::pair<string,string>(key,value));
	}

}
void metadata::putMeta(boost::property_tree::ptree inp)
{
	metadati.clear();
for(pair<string,boost::property_tree::ptree> it : inp) {
		metadati.insert(pair<string,string>(it.first,it.second.data()));
	}
}

boost::property_tree::ptree metadata::getMeta()
{
	boost::property_tree::ptree pt;
for(pair<string,string> m : metadati) {
		pt.put(m.first,m.second);
	}
	return pt;
}


void metadata::deleteFile(string archiveid)
{
	shared_ptr<AWSRequest> req = AWSRequest::getSimpleDBRequest(*cfg);
	req->addQueryStringParameter("Action","DeleteAttributes");
	req->addQueryStringParameter("ItemName",archiveid);
	req->execute_and_verify(200);
}

void metadata::insertFile(string archiveid, map<string,string>& filedata,bool updateOnly)
{
	shared_ptr<AWSRequest> req = AWSRequest::getSimpleDBRequest(*cfg);
	req->addQueryStringParameter("Action","PutAttributes");
	req->addQueryStringParameter("ItemName",archiveid);
	int i = 0;
for(pair<string,string> p : metadati) {
		string value;
		if(filedata.count(p.second)>0) {
			value=filedata[p.second];
		} else {
			if(updateOnly) {continue;}
			else {
				value=p.second;
			}
		}
		if(value.length() == 0) {continue;}
		req->addQueryStringParameter((format("Attribute.%i.Name")% ++i).str(),p.first);
		req->addQueryStringParameter((format("Attribute.%i.Value")% i).str(),value);
	}
	req->execute_and_verify(200);
}
void metadata::updateMeta(string archiveid, map<string,string>& filedata)
{
	insertFile(archiveid,filedata,true);
}
