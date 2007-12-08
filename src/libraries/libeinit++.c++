/*
 *  libeinit++.c++
 *  einit
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

#include <einit/einit++.h>
#include <map>
using std::pair;

/* our primary einit class */

Einit::Einit() {
 this->listening = false;

 this->servicesRaw = NULL;
 this->modulesRaw = NULL;
 this->modesRaw = NULL;

 einit_connect(NULL, NULL);
}

Einit::~Einit() {
 einit_disconnect();
}

void Einit::listen (enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
 if (!this->listening) {
  einit_receive_events();
  this->listening = true;
 }

 einit_remote_event_listen (type, handler);
}

void Einit::ignore (enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
 einit_remote_event_ignore (type, handler);
}

EinitService *Einit::getService (string s) {
 map<string, EinitService *>::iterator it = this->services.find (s);
 if (it != this->services.end()) {
  return it->second;
 } else {
  if (!this->servicesRaw) {
   this->servicesRaw = einit_get_all_services();
  } if (!this->servicesRaw)
   return NULL;

  struct stree *res = streefind (this->servicesRaw, s.c_str(), tree_find_first);

  if (res) {
   struct einit_service *s = (struct einit_service *)res->value;
   EinitService *r = new EinitService (this, s);

   return r;
  } else {
   return NULL;
  }
 }
}

EinitModule *Einit::getModule (string s) {
 map<string, EinitModule *>::iterator it = this->modules.find (s);
 if (it != this->modules.end()) {
  return it->second;
 } else {
  if (!this->modulesRaw) {
   this->modulesRaw = einit_get_all_modules();
  } if (!this->modulesRaw)
   return NULL;

  struct stree *res = streefind (this->modulesRaw, s.c_str(), tree_find_first);

  if (res) {
   struct einit_module *s = (struct einit_module *)res->value;
   EinitModule *r = new EinitModule (this, s);

   return r;
  } else {
   return NULL;
  }
 }
}

EinitMode *Einit::getMode (string s) {
 map<string, EinitMode *>::iterator it = this->modes.find (s);
 if (it != this->modes.end()) {
  return it->second;
 } else {
  if (!this->modesRaw) {
   this->modesRaw = einit_get_all_modes();
  } if (!this->modesRaw)
   return NULL;

  struct stree *res = streefind (this->modesRaw, s.c_str(), tree_find_first);

  if (res) {
   struct einit_mode_summary *s = (struct einit_mode_summary *)res->value;
   EinitMode *r = new EinitMode (this, s);

   return r;
  } else {
   return NULL;
  }
 }
}

map<string, EinitModule *> Einit::getAllModules() {
 if (!this->modulesRaw) {
  this->modulesRaw = einit_get_all_modules();
 }
 struct stree *cur = streelinear_prepare(this->modulesRaw);

 while (cur) {
  this->getModule(cur->key);

  cur = streenext (cur);
 }

 return this->modules;
}

map<string, EinitService *> Einit::getAllServices() {
 if (!this->servicesRaw) {
  this->servicesRaw = einit_get_all_services();
 }
 struct stree *cur = streelinear_prepare(this->servicesRaw);

 while (cur) {
  this->getService(cur->key);

  cur = streenext (cur);
 }

 return this->services;
}

map<string, EinitMode *> Einit::getAllModes() {
 if (!this->modesRaw) {
  this->modesRaw = einit_get_all_modes();
 }
 struct stree *cur = streelinear_prepare(this->modesRaw);

 while (cur) {
  this->getMode(cur->key);

  cur = streenext (cur);
 }

 return this->modes;
}


bool Einit::powerDown() {
 einit_power_down();

 return true;
}

bool Einit::powerReset() {
 einit_power_reset();

 return true;
}

void Einit::update() {
 if (this->servicesRaw) {
  struct stree *services = einit_get_all_services();
  struct stree *cur = streelinear_prepare(services);

  while (cur) {
   map<string, EinitService *>::iterator it = this->services.find ((string)cur->key);
   if (it != this->services.end()) {
    it->second->update ((struct einit_service *)cur->value);
   }

   cur = streenext (cur);
  }

  servicestree_free (this->servicesRaw);
  this->servicesRaw = services;
 }

 if (this->modulesRaw) {
  struct stree *modules = einit_get_all_modules();
  struct stree *cur = streelinear_prepare(modules);

  while (cur) {
   map<string, EinitModule *>::iterator it = this->modules.find ((string)cur->key);
   if (it != this->modules.end()) {
    it->second->update ((struct einit_module *)cur->value);
   }

   cur = streenext (cur);
  }

  modulestree_free (this->modulesRaw);
  this->modulesRaw = modules;
 }

 if (this->modesRaw) {
  struct stree *modes = einit_get_all_modes();
  struct stree *cur = streelinear_prepare(modes);

  while (cur) {
   map<string, EinitMode *>::iterator it = this->modes.find ((string)cur->key);
   if (it != this->modes.end()) {
    it->second->update ((struct einit_mode_summary *)cur->value);
   }

   cur = streenext (cur);
  }

  modestree_free (this->modesRaw);
  this->modesRaw = modes;
 }
}


/* generic offspring */

EinitOffspring::EinitOffspring (Einit *e) {
 this->main = e;
}

EinitOffspring::~EinitOffspring () {
}

/* services */

EinitService::EinitService (Einit *e, struct einit_service *t) : EinitOffspring (e) {
 this->update (t);
}

EinitService::~EinitService () {
}

bool EinitService::enable() {
 einit_service_enable (this->id.c_str());

 return true;
}

bool EinitService::disable() {
 einit_service_disable (this->id.c_str());

 return true;
}

bool EinitService::call(string s) {
 einit_service_call (this->id.c_str(), s.c_str());

 return true;
}

vector<string> EinitService::getCommonFunctions() {
 vector<string> rv = this->getAllFunctions();

 return rv;
}

vector<string> EinitService::getAllFunctions() {
 vector<string> rv;
 map<string,EinitModule *>::iterator it;

 for ( it=this->modules.begin() ; it != this->modules.end(); it++ ) {
  if (it->second->functions) {
   uint32_t i = 0;
   for ( ; it->second->functions[i]; i++ ) {
    bool have = false;
    string toadd = it->second->functions[i];

    for (i=0; i<rv.size(); i++) if (rv.at(i) == toadd) have = true;

    if (!have) {
     rv.push_back (toadd);
    }
   }
  }
 }

 return rv;
}


bool EinitService::update(struct einit_service *s) {
 this->id = s->name;

 this->provided = (s->status & service_provided) ? true : false;

 if (s->group && s->group->seq && s->group->services) {
  uint32_t i = 0;

  this->groupType = s->group->seq;

  for (; s->group->services[i]; i++) {
   EinitService *sp = this->main->getService (s->group->services[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->group.find (s->group->services[i]);
    if (it != this->group.end()) {
     this->group.erase (it);
    }

    this->group.insert(pair<string, EinitService *>(s->group->services[i], sp));
   }
  }
 }

 if (s->modules) {
  struct stree *cur = streelinear_prepare(s->modules);

  while (cur) {
   EinitModule *sp = this->main->getModule (cur->key);

   if (sp) {
    map<string, EinitModule *>::iterator it = this->modules.find (cur->key);
    if (it != this->modules.end()) {
     this->modules.erase (it);
    }

    this->modules.insert(pair<string, EinitModule *>(cur->key, sp));
   }

   cur = streenext (cur);
  }
 }

 if (s->used_in_mode) {
  uint32_t i = 0;

  for (; s->used_in_mode[i]; i++) {
   EinitMode *sp = this->main->getMode (s->used_in_mode[i]);

   if (sp) {
    map<string, EinitMode *>::iterator it = this->modes.find (s->used_in_mode[i]);
    if (it != this->modes.end()) {
     this->modes.erase (it);
    }

    this->modes.insert(pair<string, EinitMode *>(s->used_in_mode[i], sp));
   }
  }
 }

 return true;
}


/* modules */

EinitModule::EinitModule (Einit *e, struct einit_module *t) : EinitOffspring (e) {
 this->update (t);
}

EinitModule::~EinitModule () {
}

bool EinitModule::enable() {
 einit_module_id_enable (this->id.c_str());

 return true;
}

bool EinitModule::disable() {
 einit_module_id_disable (this->id.c_str());

 return true;
}

bool EinitModule::call(string s) {
 einit_module_id_call (this->id.c_str(), s.c_str());

 return true;
}

vector<string> EinitModule::getAllFunctions() {
 vector<string> rv;

 if (this->functions) {
  uint32_t i = 0;
  for ( ; this->functions[i]; i++ ) {
   rv.push_back (this->functions[i]);
  }
 }

 return rv;
}

bool EinitModule::update(struct einit_module *s) {
 this->id = s->id;
 this->name = s->name ? s->name : "unknown";

 this->enabled = (s->status & status_enabled) ? true : false;
 this->idle = (s->status == status_idle) ? true : false;
 this->error = (s->status & status_failed) ? true : false;

 if (s->requires) {
  uint32_t i = 0;

  for (; s->requires[i]; i++) {
   EinitService *sp = this->main->getService (s->requires[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->requires.find (s->requires[i]);
    if (it != this->requires.end()) {
     this->requires.erase (it);
    }

    this->requires.insert(pair<string, EinitService *>(s->requires[i], sp));
   }
  }
 }

 if (s->provides) {
  uint32_t i = 0;

  for (; s->provides[i]; i++) {
   EinitService *sp = this->main->getService (s->provides[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->provides.find (s->provides[i]);
    if (it != this->provides.end()) {
     this->provides.erase (it);
    }

    this->provides.insert(pair<string, EinitService *>(s->provides[i], sp));
   }
  }
 }

 this->after = s->after ? (char **)setdup ((const void **)s->after, SET_TYPE_STRING) : NULL;
 this->before = s->before ? (char **)setdup ((const void **)s->before, SET_TYPE_STRING) : NULL;
 this->functions = s->functions ? (char **)setdup ((const void **)s->functions, SET_TYPE_STRING) : NULL;

 return true;
}


/* modes */

EinitMode::EinitMode (Einit *e, struct einit_mode_summary *t) : EinitOffspring (e) {
 this->update (t);
}

EinitMode::~EinitMode () {
}

bool EinitMode::switchTo() {
 einit_switch_mode (this->id.c_str());

 return true;
}

bool EinitMode::update(struct einit_mode_summary *s) {
 this->id = s->id;

 if (s->services) {
  uint32_t i = 0;

  for (; s->services[i]; i++) {
   EinitService *sp = this->main->getService (s->services[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->services.find (s->services[i]);
    if (it != this->services.end()) {
     this->services.erase (it);
    }

    this->services.insert(pair<string, EinitService *>(s->services[i], sp));
   }
  }
 }

 if (s->critical) {
  uint32_t i = 0;

  for (; s->critical[i]; i++) {
   EinitService *sp = this->main->getService (s->critical[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->critical.find (s->critical[i]);
    if (it != this->critical.end()) {
     this->critical.erase (it);
    }

    this->critical.insert(pair<string, EinitService *>(s->critical[i], sp));
   }
  }
 }

 if (s->disable) {
  uint32_t i = 0;

  for (; s->disable[i]; i++) {
   EinitService *sp = this->main->getService (s->disable[i]);

   if (sp) {
    map<string, EinitService *>::iterator it = this->disable.find (s->disable[i]);
    if (it != this->disable.end()) {
     this->disable.erase (it);
    }

    this->disable.insert(pair<string, EinitService *>(s->disable[i], sp));
   }
  }
 }

 if (s->base) {
  uint32_t i = 0;

  for (; s->base[i]; i++) {
   EinitMode *sp = this->main->getMode (s->base[i]);

   if (sp) {
    map<string, EinitMode *>::iterator it = this->base.find (s->base[i]);
    if (it != this->base.end()) {
     this->base.erase (it);
    }

    this->base.insert(pair<string, EinitMode *>(s->base[i], sp));
   }
  }
 }

 return true;
}
