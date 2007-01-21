/*
 *  einit-meta-service.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/12/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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

#define _MODULE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "External Services",
 .rid       = "einit-external",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

int enable (void *, struct einit_event *);
void einit_event_handler (struct einit_event *);

struct lmodule *this;

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("services-external", NULL)) {
   fdputs ("NOTICE: configuration variable \"services-external\" not found. (not a problem)\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *r) {
 char *p;

 this = r;

 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *irr) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
}

int enable (void *pa, struct einit_event *status) {
 char *p;
 if (!cfg_getstring ("services-external/provided", NULL)) {
  status->string = "no external services configured, not enabling";
  status->flag++;
  status_update (status);
  return STATUS_FAIL;
 }

 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 return STATUS_FAIL; // once enabled, this module cannot be disabled
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_CONFIGURATION_UPDATE) {
  char *p;
  if (p = cfg_getstring("services-external/provided", NULL)) {

   pthread_mutex_lock (&this->mutex);

   this->module->si.provides = str2set (':', p);

   pthread_mutex_unlock (&this->mutex);

   this = mod_update (this);

   mod (MOD_ENABLE, this);
  }
 }
}
