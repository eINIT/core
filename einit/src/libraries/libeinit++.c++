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


/* our primary einit class */

Einit::Einit() {
 this->listening = false;

 this->servicesRaw = NULL;
 this->modulesRaw = NULL;
 this->modesRaw = NULL;

 einit_connect();
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

bool Einit::powerDown() {
 einit_power_down();

 return true;
}

bool Einit::powerReset() {
 einit_power_reset();

 return true;
}

void Einit::updateData() {
 if (this->servicesRaw) {
  struct stree *services = einit_get_all_services();
  struct stree *cur = services;

  while (cur) {
   map<string, EinitService *>::iterator it = this->services.find ((string)cur->key);
   if (it != this->services.end()) {
    it->second->update ((struct einit_service *)cur->value);
   }

   cur = streenext (cur);
  }
 }

 if (this->modulesRaw) {
  struct stree *modules = einit_get_all_modules();
  struct stree *cur = modules;

  while (cur) {
   map<string, EinitModule *>::iterator it = this->modules.find ((string)cur->key);
   if (it != this->modules.end()) {
    it->second->update ((struct einit_module *)cur->value);
   }

   cur = streenext (cur);
  }
 }

 if (this->modesRaw) {
  struct stree *modes = einit_get_all_modes();
  struct stree *cur = modes;

  while (cur) {
   map<string, EinitMode *>::iterator it = this->modes.find ((string)cur->key);
   if (it != this->modes.end()) {
    it->second->update ((struct einit_mode_summary *)cur->value);
   }

   cur = streenext (cur);
  }
 }
}


/* generic offspring */

EinitOffspring::EinitOffspring (Einit *e) {
 this->main = e;
}


/* services */

EinitService::EinitService (Einit *e, struct einit_service *t) : EinitOffspring (e) {
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

bool EinitService::update(struct einit_service *s) {
}


/* modules */

EinitModule::EinitModule (Einit *e, struct einit_module *t) : EinitOffspring (e) {
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

bool EinitModule::update(struct einit_module *s) {
}


/* modes */

EinitMode::EinitMode (Einit *e, struct einit_mode_summary *t) : EinitOffspring (e) {
}

EinitMode::~EinitMode () {
}

bool EinitMode::switchTo() {
 einit_switch_mode (this->id.c_str());

 return true;
}

bool update(struct einit_mode_summary *) {
}
