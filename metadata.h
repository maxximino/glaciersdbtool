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
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include "config.h"
#include "awsrequest.h"
class MetadataException: public boost::exception {};
class metadata
{
	public:
		metadata(Config* _cfg):metadati() {
			cfg=_cfg;
		}
		void parseMeta();
		void updateMeta(string archiveid, map<string,string>& filedata);
		void insertFile(string archiveid, map<string,string>& filedata,bool updateOnly=false);
		void deleteFile(string archiveid);
		boost::property_tree::ptree getMeta();
		void putMeta(boost::property_tree::ptree inp);
	protected:
		Config* cfg;
		std::map<string,string> metadati;

};


