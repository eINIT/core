/*
 *  einit-module.cc
 *  einit
 *
 *  Created by Magnus Deininger on 09/03/2006.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
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

#include <einit++/module.h>

void einitModule::setModule (struct lmodule *newmodule) {
 this->module = newmodule;
}

int einitModule::enable (void *p, struct einit_event *status) {
 return STATUS_FAIL;
}
int einitModule::disable (void *p, struct einit_event *status) {
 return STATUS_FAIL;
}
int einitModule::reset (void *p, struct einit_event *status) {
 return STATUS_FAIL;
}
int einitModule::reload (void *p, struct einit_event *status) {
 return STATUS_FAIL;
}

int einitModule::configure (struct lmodule *thismodule) {
 return 0;
}
int einitModule::cleanup (struct lmodule *thismodule) {
 return 0;
}

int einitModule::scanmodules (struct lmodule *list) {
 return -1;
}

extern "C" {

 int enable (void *p, struct einit_event *status) {
  return module->enable (p, status);
 }
 int disable (void *p, struct einit_event *status) {
  return module->disable (p, status);
 }
 int reset (void *p, struct einit_event *status) {
  return module->reset (p, status);
 }
 int reload (void *p, struct einit_event *status) {
  return module->reload (p, status);
 }

 int configure (struct lmodule *thismodule) {
  module->setModule (thismodule);

  return module->configure(thismodule);
 }
 int cleanup (struct lmodule *thismodule) {
  return module->cleanup(thismodule);;
 }

 int scanmodules (struct lmodule *list) {
  return module->scanmodules (list);
 }

}
