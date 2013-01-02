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
#include "uploadjob.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
using boost::format;
UploadJob::UploadJob(Config* _cfg,string resfile,bool _resume,bool _archiveboundary):archiveTreeHash(),sdb(_cfg),archiveHash(),status(),streamTreeHash(),streamHash()
{
	cfg=_cfg;
	partwritten=0;
	lastarchiveid="";
	sentParts=0;
	archiveBytes=0;
	resumefile=resfile;
	archiveboundary=_archiveboundary;
	if(_resume) { startResume(); }
	else { startUpload(); }
};
void UploadJob::startUpload()
{
	resume=false;
	sdb.parseMeta();
	partsize = cfg->getChunkSize()*1024*1024;
	partbuffer=malloc(partsize);
	InitiateMultipartUpload();
	status.add<string>("archives","");
	status.put("completed",0);
	status.add_child("runningConfig",cfg->getRunningConfig());
	status.add_child("metadata",sdb.getMeta());
	updateStatus();

}
void UploadJob::startResume()
{
	resume=true;
	boost::property_tree::info_parser::read_info(resumefile,status);
	sdb.putMeta(status.get_child("metadata"));
	multipartuploadid=status.get<string>("multipartuploadid");
	lastarchiveid=status.get<string>("lastarchiveid");
	if(status.get<int>("completed")>0) {
		cerr << "Cannot resume a completed upload." << endl;
		exit(4);
	}
	cfg->putRunningConfig(status.get_child("runningConfig"));
	partsize = cfg->getChunkSize()*1024*1024;
	partbuffer=malloc(partsize);


	updateStatus();

}
void UploadJob::updateStatus()
{
	if(resumefile.length()==0) {return;}
	if(resume) {
		size_t maxBytes=status.get<size_t>("streamBytes");
		if( maxBytes > streamBytes) {
			//wait...
		} else if(maxBytes == streamBytes) {
			//checkpoint!
			if(sentParts != status.get<size_t>("sentParts") ||
			   sentArchives != status.get<size_t>("sentArchives") ||
			   archiveBytes != status.get<size_t>("archiveBytes") ||
			   streamHash.asHexString() != status.get<string>("streamHash") ||
			   streamTreeHash.asHexString() != status.get<string>("streamTreeHash")) {
				cerr << "State corrupt. Are you sure that the input stream is exacly the same?" << endl;
				exit(6);
			} else {
				resume = 0; //fine resume :) si fa sul serio!
			}

		} else { //maxBytes < streamBytes
			cerr << "Something REALLY wrong."<< endl;
			exit(5);
		}
	} else {
		status.put("multipartuploadid",multipartuploadid);
		status.put("lastarchiveid",lastarchiveid);
		status.put("sentParts",sentParts);
		status.put("sentArchives",sentArchives);
		status.put("archiveBytes",archiveBytes);
		status.put("streamHash",streamHash.asHexString());
		status.put("streamBytes",streamBytes);
		status.put("streamTreeHash",streamTreeHash.asHexString());
		boost::property_tree::info_parser::write_info(resumefile,status);
	}
}

void UploadJob::InitiateMultipartUpload()
{
	if(!resume) {
		auto req = AWSRequest::getGlacierRequest(*cfg);
		req->setMethod("POST");
		req->setURI((format("/-/vaults/%s/multipart-uploads") % cfg->getVault()).str());
		req->addReqHeader("x-amz-archive-description",(format("GlacierSDBTool-SDBDomain:%s") % cfg->getSDBDomain()).str());
		req->addReqHeader("x-amz-part-size",(format("%i") % partsize).str());
		req->execute_and_verify(201);
		multipartuploadid = req->getResponseHeaders()["x-amz-multipart-upload-id"];
	}
	sentParts = 0;
	archiveBytes=0;
	archiveHash.reset();
	archiveTreeHash.reset();
}
void UploadJob::addData(void* ptr, size_t len)
{
	char* myptr=(char*)ptr;
	archiveTreeHash.addData(ptr,len);
	archiveHash.addData(ptr,len);
	streamTreeHash.addData(ptr,len);
	streamHash.addData(ptr,len);
	while(len>0) {
		size_t availablespace=partsize - partwritten;
		size_t tobecopied = std::min<size_t>(len,availablespace);
		memcpy((char*)partbuffer+partwritten,myptr,tobecopied);
		myptr += tobecopied;
		partwritten+=tobecopied;
		len -=tobecopied;
		if(partwritten==partsize) {
			sendPart();
		}
	}

}
void UploadJob::finishArchive()
{
	string archiveid;
	if(!resume) {
		if(partwritten > 0) {
			sendPart();
		}
		auto req = AWSRequest::getGlacierRequest(*cfg);
		req->setMethod("POST");
		req->setURI((format("/-/vaults/%s/multipart-uploads/%s") % cfg->getVault() % multipartuploadid).str());
		req->addReqHeader("x-amz-archive-description",(format("GlacierSDBTool-SDBDomain:%s") % cfg->getSDBDomain()).str());
		req->addReqHeader("x-amz-archive-size",(format("%i") % archiveBytes).str());
		req->addReqHeader("x-amz-sha256-tree-hash",archiveTreeHash.asHexString());
		req->execute_and_verify(201);
		map<string,string> hs = req->getResponseHeaders();
		archiveid=hs["x-amz-archive-id"];
		map<string,string> fd;
		fd.insert(std::pair<string,string>("/*LINEAR_SHA256_OF_ENTIRE_STREAM*/",""));
		fd.insert(std::pair<string,string>("/*TREE_SHA256_OF_ENTIRE_STREAM*/",""));
		fd.insert(std::pair<string,string>("/*TOTAL_NUMBER_OF_ARCHIVES*/",""));
		fd.insert(std::pair<string,string>("/*STREAM_SIZE_BYTES*/",""));
		fd.insert(std::pair<string,string>("/*NEXT_ARCHIVE_ID*/",""));
		fd.insert(std::pair<string,string>("/*LINEAR_SHA256_FROM_START_OF_STREAM*/",streamHash.asHexString()));
		fd.insert(std::pair<string,string>("/*TREE_SHA256_FROM_START_OF_STREAM*/",streamTreeHash.asHexString()));
		fd.insert(std::pair<string,string>("/*ARCHIVE_NUMBER*/",(format("%lu")% sentArchives).str()));
		fd.insert(std::pair<string,string>("/*ARCHIVE_FIRST_BYTE_IN_STREAM*/",(format("%lu")% (streamBytes-archiveBytes)).str()));
		fd.insert(std::pair<string,string>("/*ARCHIVE_LAST_BYTE_IN_STREAM*/",(format("%lu")% (streamBytes-1)).str()));
		fd.insert(std::pair<string,string>("/*PREVIOUS_ARCHIVE_ID*/",lastarchiveid));
		fd.insert(std::pair<string,string>("/*LINEAR_SHA256_OF_ARCHIVE*/",archiveHash.asHexString()));
		fd.insert(std::pair<string,string>("/*TREE_SHA256_OF_ARCHIVE*/",archiveTreeHash.asHexString()));
		fd.insert(std::pair<string,string>("/*ARCHIVE_SIZE_BYTES*/",((format("%lu") % archiveBytes).str())));
		sdb.insertFile(archiveid,fd);
		cout << archiveid<<endl;
		if(lastarchiveid.length()>0) {
			fd.clear();
			fd.insert(std::pair<string,string>("/*NEXT_ARCHIVE_ID*/",archiveid));
			sdb.updateMeta(lastarchiveid,fd);
		}
		lastarchiveid=archiveid;
	}
	sentArchives++;
	boost::property_tree::ptree myarchive;
	myarchive.put("name",archiveid);
	myarchive.put("linear-sha256",archiveHash.asHexString());
	myarchive.put("tree-sha256",archiveTreeHash.asHexString());
	myarchive.put("order",sentArchives);
	myarchive.put("size",archiveBytes);
	myarchive.put("streamHash",streamHash.asHexString());
	myarchive.put("streamBytes",streamBytes);
	myarchive.put("streamTreeHash",streamTreeHash.asHexString());
	if(!resume) {
		status.get_child("archives").add_child((format("%lu")% sentArchives).str(),myarchive);
	} else {
		boost::property_tree::ptree& savedarchive = status.get_child("archives").get_child((format("%lu")%sentArchives).str());
		myarchive.put("name",savedarchive.get<string>("name"));
		cout << savedarchive.get<string>("name")<<endl;
		if(myarchive != savedarchive) {
			cerr <<" Archivio " << sentArchives << " diverso. 'azz...  :(" << endl;
			boost::property_tree::info_parser::write_info(cout,myarchive);
			cout << "---------------------" << endl;
			boost::property_tree::info_parser::write_info(cout,savedarchive);

			exit(7);
		}
		if(archiveboundary && (sentArchives==status.get<int>("sentArchives"))) {
			resume=false;
		}
	}
	InitiateMultipartUpload();
	updateStatus();
}

void UploadJob::finishStream()
{
	if(partwritten > 0) {
		sendPart();
	}
	if(sentParts > 0) {
		finishArchive();
	}
	map<string,string> fd;
	fd.insert(std::pair<string,string>("/*LINEAR_SHA256_OF_ENTIRE_STREAM*/",streamHash.asHexString()));
	fd.insert(std::pair<string,string>("/*TREE_SHA256_OF_ENTIRE_STREAM*/",streamTreeHash.asHexString()));
	fd.insert(std::pair<string,string>("/*TOTAL_NUMBER_OF_ARCHIVES*/",(format("%lu")% sentArchives).str()));
	fd.insert(std::pair<string,string>("/*STREAM_SIZE_BYTES*/",(format("%lu")% streamBytes).str()));
	sdb.updateMeta(lastarchiveid,fd);
	status.put("completed",1);
	updateStatus();
}
UploadJob::~UploadJob()
{
	free(partbuffer);
}

void UploadJob::sendPart()
{
	SHA256TreeHash partTreeHash;
	SHA256Hash partHash;
	partTreeHash.addData(partbuffer,partwritten);
	partHash.addData(partbuffer,partwritten);
	if(!resume) {
		auto req = AWSRequest::getGlacierRequest(*cfg);
		req->setMethod("PUT");
		req->setURI((format("/-/vaults/%s/multipart-uploads/%s") % cfg->getVault() % multipartuploadid).str());
		req->addReqHeader("Content-Range",(format("bytes %i-%i/*") % archiveBytes % (archiveBytes+partwritten-1)).str());
		req->addReqHeader("Content-Length",(format("%i") % partwritten).str());
		req->addReqHeader("x-amz-sha256-tree-hash",partTreeHash.asHexString());
		req->addReqHeader("x-amz-content-sha256",partHash.asHexString());
		req->setPayload(partbuffer,partwritten);
		req->execute_and_verify(204);

	}
	sentParts++;
	streamBytes += partwritten;
	archiveBytes += partwritten;
	partwritten=0;
	if(archiveBytes >= (cfg->getMaxArchiveSize() * 1024*1024)) {
		finishArchive();
	}
	updateStatus();
}

