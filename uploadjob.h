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
#include "boost/format.hpp"
#include "boost/exception/all.hpp"
#include "config.h"
#include "awsrequest.h"
#include "metadata.h"

class uploadexception : public boost::exception {};
class UploadJob
{

	public:
		UploadJob(Config* _cfg,string resumefile,bool resume=false,bool _archiveboundary=false);
		void InitiateMultipartUpload();
		void addData(void* ptr, size_t len);

		void finishStream();
		~UploadJob();
	protected:
		metadata sdb;
		Config* cfg;
		boost::property_tree::ptree status;
		SHA256TreeHash archiveTreeHash;
		SHA256Hash archiveHash;
		SHA256TreeHash streamTreeHash;
		SHA256Hash streamHash;
		size_t partwritten=0;
		size_t partsize=0;
		void* partbuffer;
		size_t streamBytes=0;
		size_t archiveBytes=0;
		bool archiveboundary=false;
		string multipartuploadid="";
		string lastarchiveid="";
		string resumefile="";
		bool resume=false;
		long sentParts=0;
		long sentArchives=0;
		void sendPart();
		void finishArchive();
		void updateStatus();
		void startUpload();
		void startResume();
};



