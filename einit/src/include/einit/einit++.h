/*
 *  einit.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#include <einit/einit.h>
#include <string>
#include <map>

using std::string;
using std::map;

class EinitService;
class EinitModule;
class EinitMode;

class Einit {
 public:
  Einit();
  ~Einit();

  void listen (enum einit_event_subsystems, void (*)(struct einit_remote_event *));
  void ignore (enum einit_event_subsystems, void (*)(struct einit_remote_event *));

  EinitModule *getModule (string);
  EinitService *getService (string);
  EinitMode *getMode (string);

  map<string, EinitModule *> getAllModules();
  map<string, EinitService *> getAllServices();
  map<string, EinitMode *> getAllModes();

  bool powerDown();
  bool powerReset();

  void update();

  map<string, EinitService *> services;
  map<string, EinitModule *> modules;
  map<string, EinitMode *> modes;

 private:
  bool listening;

  struct stree *servicesRaw;
  struct stree *modulesRaw;
  struct stree *modesRaw;
};

class EinitOffspring {
 public:
  EinitOffspring (Einit *);
  ~EinitOffspring ();

  Einit *main;

 private:
};

class EinitService : public EinitOffspring {
 public:
  EinitService (Einit *, struct einit_service *);
  ~EinitService ();

  bool enable();
  bool disable();
  bool call(string);

  bool update(struct einit_service *);

  string id;
  bool provided;

  map<string, EinitModule *> modules;
  map<string, EinitService *> group;
  char *groupType;
  map<string, EinitMode *> modes;

 private:
};

class EinitModule : public EinitOffspring {
 public:
  EinitModule (Einit *, struct einit_module *);
  ~EinitModule ();

  bool enable();
  bool disable();
  bool call(string);

  bool update(struct einit_module *);

  string id;
  string name;

  bool enabled;
  bool idle;
  bool error;

  map<string, EinitService *> provides;
  map<string, EinitService *> requires;

  char **functions;
  char **before;
  char **after;

 private:
};

class EinitMode : public EinitOffspring {
 public:
  EinitMode (Einit *, struct einit_mode_summary *);
  ~EinitMode ();

  bool switchTo();

  bool update(struct einit_mode_summary *);

  string id;

  map<string, EinitService *> services;
  map<string, EinitService *> critical;
  map<string, EinitService *> disable;
  map<string, EinitMode *> base;

 private:
};
