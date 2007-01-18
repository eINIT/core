/*
 *  einit-feedback-visual-fbsplash.c
 *  einit
 *
 *  Created by Magnus Deininger on 18/01/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <einit-modules/exec.h>

#include <sys/types.h>
#include <sys/stat.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char *provides[] = {"feedback-visual", "feedback-graphical", NULL};
const struct smodule self = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= EINIT_MOD_FEEDBACK,
 .options	= 0,
 .name		= "visual/fbsplash-based feedback module",
 .rid		= "einit-feedback-visual-fbsplash",
 .si           = {
  .provides = provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

struct lmodule *self_l = NULL;

void feedback_event_handler(struct einit_event *);

char *splash_util = "/sbin/splash_util";

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getstring("configuration-feedback-visual-fbsplash-splash-util", NULL)) {
   fdputs (" * configuration variable \"configuration-feedback-visual-fbsplash-splash-util\" not found.\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *this) {
 exec_configure (this);
 char *s = NULL;

 struct cfgnode *node;

 if (s = cfg_getstring("configuration-feedback-visual-fbsplash-splash-util", NULL))
  splash_util = s;

 self_l = this;
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int enable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&self_l->imutex);

 return STATUS_FAIL;

 struct stat st;
 if (stat (splash_util, &st)) return STATUS_FAIL;

 event_listen (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 pthread_mutex_unlock (&self_l->imutex);
 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&self_l->imutex);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 pthread_mutex_unlock (&self_l->imutex);
 return STATUS_OK;
}

void feedback_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&self_l->imutex);


 pthread_mutex_unlock (&self_l->imutex);
 return;
}
