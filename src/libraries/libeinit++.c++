/*
 *  libeinit++.c++
 *  einit
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Modifications by nikolavp
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
  //We first connect
  einit_connect(NULL, NULL);
  //Starting to populate the maps
  struct stree *serviceRaw = einit_get_all_services();
  struct stree *resService = streelinear_prepare(serviceRaw);
  while (resService) {
    services[resService->key] = new EinitService(this, (struct einit_service *)resService->value);
    resService = streenext(resService);
  }
  servicestree_free(serviceRaw);
  struct stree *modulesRaw = einit_get_all_modules();
  struct stree *resModule = streelinear_prepare(modulesRaw);
  while (resModule) {
    modules[resModule->key] = new EinitModule(this, (struct einit_module *)resModule->value);
    resModule = streenext(resModule);
  }
  modulestree_free(modulesRaw);
  struct stree *modesRaw = einit_get_all_modes();
  struct stree *resMode = streelinear_prepare(modesRaw);
  while (resMode) {
    modes[resMode->key] = new EinitMode(this, (struct einit_mode_summary *)resMode->value);
    resMode = streenext(resMode);
  }
  modestree_free(modesRaw);
  this->listening = false;
}
 
Einit::~Einit() {
  //Start to delete the maps from the constructor :(
  for (servicesMap::iterator sIt = services.begin(); sIt != services.end(); sIt++) {
    delete sIt->second;
  }
  for (modulesMap::iterator mIt = modules.begin(); mIt != modules.end(); mIt++) {
    delete mIt->second;
  }
  for (modesMap::iterator It = modes.begin(); It != modes.end(); It++) {
    delete It->second;
  }
  einit_disconnect();
}
//I haven't touched those two methods below, but it seems they should work...
void Einit::listen(enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
  if (!this->listening) {
    einit_receive_events();
    this->listening = true;
  }
 
  einit_remote_event_listen(type, handler);
}
 
void Einit::ignore(enum einit_event_subsystems type, void (* handler)(struct einit_remote_event *)) {
  einit_remote_event_ignore(type, handler);
}
 
EinitModule *Einit::getModule(const string& id) const {
  modulesMap::const_iterator it = this->modules.find(id);
  if (it != this->modules.end()) {
    return it->second;
  } else {
    return NULL;
  }
}
 
EinitService *Einit::getService(const string& id) const {
  servicesMap::const_iterator it = this->services.find(id);
  if (it != this->services.end()) {
    return it->second;
  } else {
    return NULL;
  }
}
 
EinitMode *Einit::getMode(const string& id) const {
  modesMap::const_iterator it = this->modes.find(id);
  if (it != this->modes.end()) {
    return it->second;
  } else {
    return NULL;
  }
}
 
modulesMap Einit::getAllModules() const {
  return modules;
}
 
servicesMap Einit::getAllServices() const {
  return services;
}
 
modesMap Einit::getAllModes() const {
  return modes;
}
 
//Don't touch those they should work
bool Einit::powerDown() {
  einit_power_down();
 
  return true;
}
 
bool Einit::powerReset() {
  einit_power_reset();
 
  return true;
}
 
//Does this update works ?:)
void Einit::update() {
  struct stree *servicesRaw = einit_get_all_services();
  struct stree *resServices = streelinear_prepare(servicesRaw);
 
  while (resServices) {
    servicesMap::iterator it = this->services.find((string)resServices->key);
    if (it != this->services.end()) {
      it->second->update((struct einit_service *)resServices->value);
    }
 
    resServices = streenext (resServices);
 
  }
  servicestree_free(servicesRaw);
 
  struct stree *modulesRaw = einit_get_all_modules();
  struct stree *resModules = streelinear_prepare(modulesRaw);
 
  while (resModules) {
    modulesMap::iterator it = this->modules.find((string)resModules->key);
    if (it != this->modules.end()) {
      it->second->update((struct einit_module *)resModules->value);
    }
 
    resModules = streenext (resModules);
  }
 
  modulestree_free(modulesRaw);
 
  struct stree *modesRaw = einit_get_all_modes();
  struct stree *resModes = streelinear_prepare(modesRaw);
 
  while (resModes) {
    modesMap::iterator it = this->modes.find((string)resModes->key);
    if (it != this->modes.end()) {
      it->second->update((struct einit_mode_summary *)resModes->value);
    }
 
    resModes = streenext (resModes);
  }
 
  modestree_free(modesRaw);
}
 
/* generic offspring */
EinitOffspring::EinitOffspring(Einit *e) {
  this->main = e;
}
 
EinitOffspring::~EinitOffspring() {
}
 
/* services */
 
EinitService::EinitService(Einit *e, struct einit_service *t) :
  EinitOffspring(e) {
  this->update(t);
}
 
EinitService::~EinitService() {
}
 
bool EinitService::enable() {
  einit_service_enable(this->id.c_str());
 
  return true;
}
 
bool EinitService::disable() {
  einit_service_disable(this->id.c_str());
 
  return true;
}
 
bool EinitService::call(string s) {
  einit_service_call(this->id.c_str(), s.c_str());
 
  return true;
}
 
vector<string> EinitService::getCommonFunctions() {
  vector<string> rv = this->getAllFunctions();
 
  return rv;
}
 
vector<string> EinitService::getAllFunctions() {
  vector<string> rv;
 
  for (unsigned int i = 0; i < this->modules.size(); i++) {
    EinitModule *m = this->main->getModule(this->modules[i]);
 
    for (unsigned int y = 0; y < m->functions.size(); y++) {
      bool have = false;
 
      for (unsigned int z = 0; z < rv.size(); z++) {
        if (rv[z] == m->functions[y])
          have = true;
      }
 
      if (!have)
        rv.push_back(m->functions[y]);
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
 
    this->group.clear();
    for (; s->group->services[i]; i++) {
      this->group.push_back(s->group->services[i]);
    }
  }
 
  if (s->modules) {
    struct stree *cur = streelinear_prepare(s->modules);
 
    this->modules.clear();
 
    while (cur) {
      this->modules.push_back(cur->key);
      cur = streenext (cur);
    }
  }
 
  if (s->used_in_mode) {
    uint32_t i = 0;
 
    this->modes.clear();
    for (; s->used_in_mode[i]; i++) {
      this->modules.push_back(s->used_in_mode[i]);
    }
  }
 
  return true;
}
 
/* modules */
 
EinitModule::EinitModule(Einit *e, struct einit_module *t) :
  EinitOffspring(e) {
  this->update(t);
}
 
EinitModule::~EinitModule() {
}
 
bool EinitModule::enable() {
  einit_module_id_enable(this->id.c_str());
 
  return true;
}
 
bool EinitModule::disable() {
  einit_module_id_disable(this->id.c_str());
 
  return true;
}
 
bool EinitModule::call(string s) {
  einit_module_id_call(this->id.c_str(), s.c_str());
 
  return true;
}
 
vector<string> EinitModule::getAllFunctions() const {
  return functions;
}
 
bool EinitModule::update(struct einit_module *s) {
  this->id = s->id;
  this->name = s->name ? s->name : "unknown";
 
  this->enabled = (s->status & status_enabled) ? true : false;
  this->idle = (s->status == status_idle) ? true : false;
  this->error = (s->status & status_failed) ? true : false;
 
  this->requires.clear();
 
  if (s->requires) {
    uint32_t i = 0;
 
    for (; s->requires[i]; i++) {
      this->requires.push_back(s->requires[i]);
    }
  }
 
  this->provides.clear();
 
  if (s->provides) {
    uint32_t i = 0;
 
    for (; s->provides[i]; i++) {
      this->requires.push_back(s->provides[i]);
    }
  }
 
  this->after.clear();
 
  if (s->after) {
    uint32_t i = 0;
 
    for (; s->after[i]; i++) {
      this->after.push_back(s->after[i]);
    }
  }
 
  this->before.clear();
 
  if (s->before) {
    uint32_t i = 0;
 
    for (; s->before[i]; i++) {
      this->before.push_back(s->before[i]);
    }
  }
 
  this->functions.clear();
 
  if (s->functions) {
    uint32_t i = 0;
 
    for (; s->functions[i]; i++) {
      this->functions.push_back(s->functions[i]);
    }
  }
 
  return true;
}
 
/* modes */
 
EinitMode::EinitMode(Einit *e, struct einit_mode_summary *t) :
  EinitOffspring(e) {
  this->update(t);
}
 
EinitMode::~EinitMode() {
}
 
bool EinitMode::switchTo() {
  einit_switch_mode(this->id.c_str());
 
  return true;
}
 
bool EinitMode::update(struct einit_mode_summary *s) {
  this->id = s->id;
 
  if (s->services) {
    uint32_t i = 0;
 
    for (; s->services[i]; i++) {
      EinitService *sp = this->main->getService(s->services[i]);
 
      if (sp) {
        servicesMap::iterator it =
            this->services.find(s->services[i]);
        if (it != this->services.end()) {
          this->services.erase(it);
        }
 
        this->services.insert(pair<string, EinitService *>(s->services[i], sp));
      }
    }
  }
 
  if (s->critical) {
    uint32_t i = 0;
 
    for (; s->critical[i]; i++) {
      EinitService *sp = this->main->getService(s->critical[i]);
 
      if (sp) {
        servicesMap::iterator it =
            this->critical.find(s->critical[i]);
        if (it != this->critical.end()) {
          this->critical.erase(it);
        }
 
        this->critical.insert(pair<string, EinitService *>(s->critical[i], sp));
      }
    }
  }
 
  if (s->disable) {
    uint32_t i = 0;
 
    for (; s->disable[i]; i++) {
      EinitService *sp = this->main->getService(s->disable[i]);
 
      if (sp) {
        servicesMap::iterator it =
            this->disable.find(s->disable[i]);
        if (it != this->disable.end()) {
          this->disable.erase(it);
        }
 
        this->disable.insert(pair<string, EinitService *>(s->disable[i], sp));
      }
    }
  }
 
  if (s->base) {
    uint32_t i = 0;
 
    for (; s->base[i]; i++) {
      EinitMode *sp = this->main->getMode(s->base[i]);
 
      if (sp) {
        modesMap::iterator it = this->base.find(s->base[i]);
        if (it != this->base.end()) {
          this->base.erase(it);
        }
 
        this->base.insert(pair<string, EinitMode *>(s->base[i], sp));
      }
    }
  }
 
  return true;
}
