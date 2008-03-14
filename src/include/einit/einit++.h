/*
 *  einit++.h
 *  eINIT
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
 
/*!\file einit/einit++.h
 * \brief eINIT C++ Client Library
 * \author Magnus Deininger
 *
 * Bindings for libeinit in C++ (libeinit++). You'll very likely want to use this and not libeinit (einit/einit.h) for your client applications. It's also a lot cleaner and you don't need to worry about nearly as much.
 */
 
#include <einit/einit.h>
#include <string>
#include <map>
#include <vector>

using std::string;
using std::vector;

/*!\brief The Main eINIT Object
 *
 * This is the main object to manipulate eINIT with. You should only have one instance of this in your program.
 */
class Einit {
 private:
 
 
 
 public:
  Einit(); /*!<\brief Regular constructor, as a side-effect this'll initate the connection to eINIT. */
  ~Einit(); /*!<\brief Regular destructor, as a side-effect this'll terminate the connection to eINIT. */

	
/*!\brief Power Down the System
 *
 * Tell eINIT to initiate a system shutdown. You're likely to die soon after this, so better start cleaning up ASAP.
 */
  bool powerDown();

/*!\brief Reset the System
 *
 * Tell eINIT to initiate a system reboot. You're likely to die soon after this, so better start cleaning up ASAP.
 */
  bool powerReset();

/*!\brief Update current Information from the core.
 *
 * Update all the information we have from eINIT.
 */
  void update();


/*!\brief Connect to eINIT
 *
 * Connect to eINIT, via whatever Method is deemed appropriate. Use this before 
 * using any of the einit*_ipc*() functions.
*/ 
  bool connect(int *argc, char **argv);
  
  bool connectSpawn(int *argc, char **argv);
  
	char disconnect();
 	void switchMode (const string mode);
  
	void eventLoop(); 
	void replayEvents();
 	void serviceCall(const string service, const string action);
 	EinitModule makeModule(const string name);

};

/*!\brief The eINIT file system.
 * 
 *
 */
class EinitFilesystem {
	
	IxpClient *einit_ipc_9p_client = NULL;
	pid_t einit_ipc_9p_client_pid = 0;

	
	int readCallback (string *path, int (*callback)(string, size_t, void *), void *cdata);
	int readCallbackLimited (string *path, int (*callback)(string, size_t, void *), void *cdata, int fragments);

	
	public:
	int write(string *path, const string data);
	string* ls(string *path);
	string read(string *path);
	
};

class EinitModule {
	
	string rid;
	
	int enable  (void *, struct einit_event *);
	int disable (void *, struct einit_event *);
	int custom (void *, char *, struct einit_event *);
	int cleanup (EinitModule m);
	int scanmodules (EinitModule m);
	
	public:
	void call(const string rid, const string action);
  string getAttribute (const string rid, const string attribute);
  string getName (const string rid);
  vector<string> EinitModule::stringToVector(const string rid, const string attr);
  vector<string> getProvides (const string rid);
  vector<string> getRequires (const string rid);
  vector<string> getAfter (const string rid);
  vector<string> getBefore (const string rid);
  vector<string> getStatus (const string rid);
  vector<string> getOptions (const string rid);
   
}
