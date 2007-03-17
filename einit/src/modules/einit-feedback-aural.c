/*
 *  einit-feedback-aural.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/09/2006.
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
#include <einit/bitch.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_feedback_aural_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

char *provides[] = {"feedback-aural", NULL};
char *requires[] = {"audio", "mount/critical", NULL};
const struct smodule _einit_feedback_aural_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "aural/tts feedback module",
 .rid       = "einit-feedback-aural",
 .si        = {
  .provides = provides,
  .requires = requires,
  .after    = NULL,
  .before   = NULL
 },
.configure = _einit_feedback_aural_configure
};

module_register(_einit_feedback_aural_self);

#endif

void feedback_event_handler(struct einit_event *);
void synthesize (char *);
char *synthesizer;
int sev_threshold = 2;

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && strmatch(ev->set[0], "examine") && strmatch(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-feedback-aural-tts-synthesizer-command", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-aural-tts-synthesizer-command\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-aural-tts-vocalising-threshold", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-aural-tts-vocalising-threshold\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int _einit_feedback_aural_cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 return 0;
}

int _einit_feedback_aural_enable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_listen (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

int _einit_feedback_aural_disable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

void feedback_event_handler(struct einit_event *ev) {
 emutex_lock (&thismodule->imutex);

 char phrase[BUFFERSIZE], hostname[BUFFERSIZE];
 phrase[0] = 0;

 if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    if (gethostname (hostname, BUFFERSIZE)) strcpy (hostname, "localhost");
    hostname[BUFFERSIZE] = 0;
    esprintf (phrase, BUFFERSIZE, "Host \"%s\" now switching to mode \"%s\".", hostname, (cmode && cmode->id) ? cmode->id : "unknown");
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
    esprintf (phrase, BUFFERSIZE, "New mode \"%s\" is now in effect.", (amode && amode->id) ? amode->id : "unknown");
    break;
  }
 } else if (ev->type == EVE_FEEDBACK_NOTICE) {
  if (synthesizer && ev->string && (ev->flag < sev_threshold)) {
   char *tx;
   strtrim (ev->string);

   if (!(tx = strrchr (ev->string, ':'))) tx = ev->string;
   else tx ++;

   if (tx) strncat (phrase, tx, BUFFERSIZE);
  }
 }

 if (phrase[0]) synthesize (phrase);

 emutex_unlock (&thismodule->imutex);
 return;
}

/* BUG: using popen/pclose might interfere with the scheduler's zombie-auto-reaping code,
        if we're running this on a system with a buggy pthreads implementation */
void synthesize (char *string) {
 FILE *px = popen (synthesizer, "w");

 if (px) {
  eputs (string, px);

  if (pclose (px) == -1)
   perror ("tts: pclose");
 } else
  perror ("tts: popen");
}

int _einit_feedback_aural_configure (struct lmodule *r) {
 module_init (r);
 exec_configure (r);

 r->cleanup = _einit_feedback_aural_cleanup;
 r->enable = _einit_feedback_aural_enable;
 r->disable = _einit_feedback_aural_disable;

 struct cfgnode *node;

 synthesizer = cfg_getstring ("configuration-feedback-aural-tts-synthesizer-command", NULL);

 if ((node = cfg_getnode ("configuration-feedback-aural-tts-vocalising-threshold", NULL)))
  sev_threshold = node->value;

 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 return 0;
}
