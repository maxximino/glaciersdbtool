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
#include <iostream>
#include <string>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <tclap/CmdLine.h>
#include <tclap/UnlabeledValueArg.h>
#include "config.h"
#include "uploadjob.h"
#include "cryptoutils.h"
#include "awsrequest.h"
#include "boost/property_tree/json_parser.hpp"
#include "boost/property_tree/info_parser.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "boost/property_tree/ptree.hpp"
using namespace std;
class CommandLineException: public boost::exception {};


void upload(Config& cfg, TCLAP::ValueArg<string>& resumefile,bool resume,TCLAP::SwitchArg& archiveboundary)
{
	UploadJob j = UploadJob(&cfg,resumefile.getValue(),resume,archiveboundary.getValue());
	char* buf = (char*)malloc(512*1024);
	while(!cin.eof()) {
		cin.read(buf,512*1024);
		j.addData(buf,cin.gcount());
	}
	free(buf);
	j.finishStream();
}
void startdownload(Config& cfg, TCLAP::ValueArg<string>& archiveid)
{

	boost::property_tree::ptree pt;
	if(!archiveid.isSet()) {
		throw CommandLineException() << boost::error_info<string,string>("Please specify the archive id." );

	}
	pt.put("Type","archive-retrieval");
	pt.put("ArchiveId",archiveid.getValue());
	auto req = AWSRequest::getGlacierRequest(cfg);
	stringstream ss("");
	req->setMethod("POST");
	req->setURI((format("/-/vaults/%s/jobs") % cfg.getVault()).str());
	boost::property_tree::write_json(ss,pt);
	int datalen = ss.str().length();
	char* buf = (char*)malloc(datalen);
	memcpy(buf,ss.str().c_str(),datalen);
	req->addReqHeader("Content-Length",(format("%u")% datalen).str());
	req->setPayload(buf,datalen);
	req->execute_and_verify(202);
	free(buf);
	map<string,string> hs = req->getResponseHeaders();
	cout << hs["x-amz-job-id"];
}

void getdownload(Config& cfg, TCLAP::ValueArg<string>& jobid)
{
	if(!jobid.isSet()) {
		throw CommandLineException() << boost::error_info<string,string>("Please specify the job id." );

	}
	auto req = AWSRequest::getGlacierRequest(cfg);
	req->setMethod("GET");
	req->setURI((format("/-/vaults/%s/jobs/%s/output") % cfg.getVault()% jobid.getValue()).str());
	req->print_as_wget();
}

void deleteArchive(Config& cfg, TCLAP::ValueArg<string>& archiveid)
{
	if(!archiveid.isSet()) {
		throw CommandLineException() << boost::error_info<string,string>("Please specify the archive id." );

	}
	auto req = AWSRequest::getGlacierRequest(cfg);
	req->setMethod("DELETE");
	req->setURI((format("/-/vaults/%s/archives/%s") % cfg.getVault()% archiveid.getValue()).str());
	req->execute_and_verify(204);
	metadata sdb(&cfg);
	sdb.deleteFile(archiveid.getValue());
	cout << "Deleted." <<endl;
}

void listJobs(Config& cfg)
{
	auto req = AWSRequest::getGlacierRequest(cfg);
	req->setMethod("GET");
	req->setURI((format("/-/vaults/%s/jobs") % cfg.getVault()).str());
	stringstream os("");
	req->setOutputStream(&os);
	req->execute();
	//dovrei implementare le richieste successive, cioè l'uso dei marker, se ci sono piu di 1000 jobs ora tronca la lista.
	boost::property_tree::ptree pt;
	boost::property_tree::ptree dest_pt;
	boost::property_tree::read_json(os,pt);
	boost::property_tree::ptree jl = pt.get_child("JobList");
	for(boost::property_tree::ptree::iterator it = jl.begin(); it!= jl.end(); ++it) {
		if(it->first.length() == 0) {
			dest_pt.add_child("Job",it->second);
		}
	}
	boost::property_tree::ptree dest_pt_root;
	dest_pt_root.add_child("JobList",dest_pt);
	boost::property_tree::write_xml(cout,dest_pt_root);
}

void requestInventory(Config& cfg)
{
	boost::property_tree::ptree pt;
	pt.put("Type","inventory-retrieval");
	auto req = AWSRequest::getGlacierRequest(cfg);
	stringstream ss("");
	req->setMethod("POST");
	req->setURI((format("/-/vaults/%s/jobs") % cfg.getVault()).str());
	boost::property_tree::write_json(ss,pt);
	int datalen = ss.str().length();
	char* buf = (char*)malloc(datalen);
	memcpy(buf,ss.str().c_str(),datalen);
	req->addReqHeader("Content-Length",(format("%u")% datalen).str());
	req->setPayload(buf,datalen);
	req->execute_and_verify(202);
	free(buf);
	map<string,string> hs = req->getResponseHeaders();
	cout << hs["x-amz-job-id"];
}

void readInventory(Config& cfg, TCLAP::ValueArg<string>& jobid)
{
	if(!jobid.isSet()) {
		throw CommandLineException() << boost::error_info<string,string>("Please specify the job id." );

	}
	auto req = AWSRequest::getGlacierRequest(cfg);
	req->setMethod("GET");
	req->setURI((format("/-/vaults/%s/jobs/%s/output") % cfg.getVault() % jobid.getValue()).str());
	stringstream os;
	req->setOutputStream(&os);
	req->execute();
	boost::property_tree::ptree pt;
	boost::property_tree::ptree dest_pt;
	boost::property_tree::read_json(os,pt);
	boost::property_tree::ptree jl = pt.get_child("ArchiveList");
	for(boost::property_tree::ptree::iterator it = jl.begin(); it!= jl.end(); ++it) {
		if(it->first.length() == 0) {
			dest_pt.add_child("Archive",it->second);
		}
	}
	boost::property_tree::ptree dest_pt_inv;
	boost::property_tree::ptree dest_pt_root;
	dest_pt_inv.add_child("VaultARN",pt.get_child("VaultARN"));
	dest_pt_inv.add_child("InventoryDate",pt.get_child("InventoryDate"));
	dest_pt_inv.add_child("ArchiveList",dest_pt);
	dest_pt_root.add_child("Inventory",dest_pt_inv);
	boost::property_tree::write_xml(cout,dest_pt_root);
}

int main(int argc, char** argv)
{
	TCLAP::CmdLine cmd ( "GlacierSDBTool", ' ', "1" );
	TCLAP::UnlabeledValueArg<string> mode("mode", "Mode", true, "nameString","upload,resumeupload,listjobs,request-inventory,read-inventory,delete,startdownload,getdownload");
	TCLAP::ValueArg<string> archiveid("i","archiveid","Archive ID",false,"archiveid","");
	TCLAP::ValueArg<string> jobid("j","jobid","Job ID",false,"jobid","");
	TCLAP::ValueArg<string> resumefile("r","resumefile","Path to resume file",false,"filename","");
	TCLAP::SwitchArg archiveboundary("b","archive-boundary","Resume on archive boundary instead of multipart upload part",false);
	Config cfg;
	cmd.add(mode);
	cmd.add(archiveid);
	cmd.add(jobid);
	cmd.add(resumefile);
	cmd.add(archiveboundary);
	cfg.addArgs(cmd);
	cmd.parse(argc,argv);
	try {
		cfg.loadConfig();
	} catch(boost::exception& exc) {
		cerr << "Configuration is not valid.Exiting."<< endl << "Details:" << boost::diagnostic_information(exc)<<endl;
		return 1;
	}
	try {
		if(mode.getValue()=="upload") { upload(cfg,resumefile,false,archiveboundary); }
		else if(mode.getValue()=="resumeupload") { upload(cfg,resumefile,true,archiveboundary); }
		else if(mode.getValue()=="startdownload") { startdownload(cfg,archiveid); }
		else if(mode.getValue()=="getdownload")  { getdownload(cfg,jobid); }
		else if(mode.getValue()=="delete") { deleteArchive(cfg,archiveid); }
		else if(mode.getValue()=="listjobs") { listJobs(cfg); }
		else if(mode.getValue()=="request-inventory") { requestInventory(cfg); }
		else if(mode.getValue()=="read-inventory") { readInventory(cfg,jobid); }
		else {
			cerr << "Invalid mode.";
			return 1;

		}
	} catch(boost::exception& exc) {
		cerr << "There was an error executing your request:" << endl << boost::diagnostic_information(exc)<<endl;
		return 1;
	}
	return 0;
}
