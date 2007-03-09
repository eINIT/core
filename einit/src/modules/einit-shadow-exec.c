/*
 *  einit-shadow-exec.c
 *  einit
 *
 *  Created by Magnus Deininger on 09/03/2007.
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

#define _MODULE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include <einit/module.h>
#include <einit/config.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/bitch.h>
#include <einit-modules/exec.h>

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
 .name      = "Shadow Module Support",
 .rid       = "einit-shadow-exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

struct cfgnode *ecmode = NULL;
struct stree *shadows = NULL;

pthread_mutex_t shadow_mutex = PTHREAD_MUTEX_INITIALIZER;

void update_shadows() {
 emutex_lock(&shadow_mutex);

 if (ecmode != cmode) {
  char *tmp = cfg_getstring("shadows", NULL);

  if (shadows) {
   streefree (shadows);
   shadows = NULL;
  }

  ecmode = cmode;
 }

 emutex_unlock(&shadow_mutex);
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_SERVICE_UPDATE) {
  if (ecmode != cmode) {
   update_shadows();
  }

  if (shadows) {
  }
 }
}

int configure (struct lmodule *this) {
 exec_configure(this);

 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 return 0;
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 exec_cleanup(this);

 return 0;
}
