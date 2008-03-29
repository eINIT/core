/*
 *  libeinit++.c++
 *  einit
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Modifications by nikolavp
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */
 
/*
 Copyright (c) 2007-2008, Magnus Deininger
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 used to endorse or promote products derived from this software without
 specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <string>
#include <einit/einit++.h>

#include <ixp_local.h>

using std::pair;
using std::string;
using std::vector;

/* our primary einit class */

Einit::Einit() {
  einit_connect(NULL, NULL);
}

Einit::~Einit() {
  einit_disconnect();
}

bool Einit::connect(int *argc, char **argv) {
	einit_connect(argc, argv);
}

bool Einit::connectSpawn(int *argc, char **argv) {
	
}

bool Einit::disconnect() {
  einit_disconnect();
}

void Einit::update() {}

bool Einit::powerDown() {
  Einit::switchMode("power-down");

  return true;
}
 
bool Einit::powerReset() {
  Einit::switchMode("power-reset");

  return true;
}

void Einit::switchMode(const string mode) {
	string* path = new string[2];
	path[0] = mode;
	path[1] = "";
	
	EinitFilesystem::write(path, mode);
}

void Einit::eventLoop() {} 
void Einit::replayEvents() {}
void Einit::serviceCall(const string service, const string action) {}


EinitModule Einit::makeModule(const string rid) {
	return EinitModule(rid);
}



/*!\brief This class handles the modules of eINIT.
 *  
 * 
 */
EinitModule::EinitModule(const string name) {
	Einit *core = new Einit(); // handles the connection for us, provides info about the module somehow 
	EinitModule::rid = name;
}

void EinitModule::call(const string action) {
	string* path = new string[5];
	path[0] = "modules", path[1] = "all", path [2] = rid, path[3] = "status", path[4] = "";
	EinitFilesystem::write(path, action);
}

string EinitModule::getAttribute (const string attribute) {
	using namespace std;
	string* path = new string[5];
	
	path[0] = "modules", path[1] = "all", path [2] = rid, path[3] = attribute, path[4] = "";
	string tmp = EinitFilesystem::read (path);
	if (tmp != "") {
		const char* c = new char[tmp.size()];
		c = tmp.c_str();
		strtrim(((char* ) c));
		string rv(c);
		return rv;
	}
	else{
		return ""; 
	}
}

string EinitModule::getName () {
	using namespace std;
	string data = EinitModule::getAttribute("name");
 	if (data != "") {
 		const char* tmp = new char[data.size()];
 		tmp = data.c_str();
 		string rv(str_stabilise(tmp));
 	  return rv;
	}
	return "";
}

vector<string> EinitModule::stringToVector(const string attr) {
	string data = EinitModule::getAttribute(attr);
	
	if (data != "") {
	char **rv = str2set ('\n', data.c_str());
  char **nrv = set_str_dup_stable (rv);
  efree (rv);
  vector<string> retval;
  for(int i = 0;nrv[i];i++) {
  	string tmp(nrv[i]);
  	retval.push_back(tmp);
  }
  efree (nrv);
  return retval;
	}
	vector<string> rv;
	return rv;
}


vector<string> EinitModule::getProvides () {
	EinitModule::stringToVector("provides");
}

vector<string> EinitModule::getRequires () {
	EinitModule::stringToVector("requires");
}

vector<string> EinitModule::getBefore () {
	EinitModule::stringToVector("before");
}

vector<string> EinitModule::getAfter () {
	EinitModule::stringToVector("after");
}

vector<string> EinitModule::getStatus () {
	EinitModule::stringToVector("status");
}

vector<string> EinitModule::getOptions () {
	EinitModule::stringToVector("options");
}

/*these are supposed to just point to the enable/disable functions when I figure out how to get them from the core */

int EinitModule::enable(void *, struct einit_event *event) {
	
}

int EinitModule::disable(void *, struct einit_event *event) {
	
}

int EinitModule::custom(void *, char *, struct einit_event *event) {
	
}

int EinitModule::scanmodules() {
	
}

int EinitModule::cleanup() {
	
}


/*!\brief This class handles the file system of eINIT.
 *  
 * 
 */
		
int EinitFilesystem::readCallback (string *path, int (*callback)(string, size_t, void *), void *cdata) {}
int EinitFilesystem::readCallbackLimited (string *path, int (*callback)(string, size_t, void *), void *cdata, int fragments) {}

int EinitFilesystem::write(string *path, const string data){
		if (data == "") return 0;
		char **tmp = new char*[5];
		for (int i = 0; i < 5; i++) {
			tmp[i] = new char[path[i].size()];
			tmp[i] = ((char*) path[i].c_str());
		}
		einit_write (tmp, data.c_str());
		for (int i = 0; i < 5; i++) {
			delete[] tmp[i];
		}
		delete[] tmp;
		return 0;
}

vector<string> EinitFilesystem::ls(string *path){
		using namespace std;
		char** p = new char*[sizeof(path)/sizeof(path[0])];
		for(int i = 0; i < (sizeof(path)/sizeof(path[0])); i++) {
			p[i] = ((char*) path[i].c_str());
		}
		char** tmp = einit_ls(p);
		vector<string> rv;
		for (int i = 0; i < (sizeof(path)/sizeof(path[0])); i++) {
			string t(tmp[i]);
			rv.push_back(t);
			
			delete[] p[i];
			delete[] tmp[i];
		}
		delete[] p;
		delete[] tmp;
		
		return rv;
			
	}

string EinitFilesystem::read(string *path){
	char** tmp = new char*[5];
	for (int i = 0; i < 5; i++) {
			tmp[i] = new char[path[i].size()];
			tmp[i] = ((char*) path[i].c_str());
		}
		string rv(einit_read (tmp));
		for (int i = 0; i < 5; i++) {
			delete[] tmp[i];
		}
		delete[] tmp;
		return rv;
		
}
