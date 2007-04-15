/*
 *  feedback-aural.c
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

int einit_feedback_aural_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char *einit_feedback_aural_provides[] = {"feedback-aural", NULL};
char *einit_feedback_aural_requires[] = {"audio", "mount-critical", NULL};
const struct smodule einit_feedback_aural_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "aural/tts feedback module",
 .rid       = "feedback-aural",
 .si        = {
  .provides = einit_feedback_aural_provides,
  .requires = einit_feedback_aural_requires,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_feedback_aural_configure
};

module_register(einit_feedback_aural_self);

#endif

void einit_feedback_aural_feedback_event_handler(struct einit_event *);
void synthesize (char *);
char *synthesizer;
int sev_threshold = 2;

void einit_feedback_aural_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  if (!cfg_getnode("configuration-feedback-aural-tts-synthesizer-command", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-aural-tts-synthesizer-command\" not found.\n", ev->output);
   ev->ipc_return++;
  }
  if (!cfg_getnode("configuration-feedback-aural-tts-vocalising-threshold", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-aural-tts-vocalising-threshold\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }
}

int einit_feedback_aural_cleanup (struct lmodule *this) {
 exec_cleanup(this);
 event_ignore (EVENT_SUBSYSTEM_IPC, einit_feedback_aural_ipc_event_handler);

 return 0;
}

int einit_feedback_aural_enable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_listen (EVENT_SUBSYSTEM_FEEDBACK, einit_feedback_aural_feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

int einit_feedback_aural_disable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, einit_feedback_aural_feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

void einit_feedback_aural_feedback_event_handler(struct einit_event *ev) {
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

int einit_feedback_aural_configure (struct lmodule *r) {
 module_init (r);
 exec_configure (r);

 r->cleanup = einit_feedback_aural_cleanup;
 r->enable = einit_feedback_aural_enable;
 r->disable = einit_feedback_aural_disable;

 struct cfgnode *node;

 synthesizer = cfg_getstring ("configuration-feedback-aural-tts-synthesizer-command", NULL);

 if ((node = cfg_getnode ("configuration-feedback-aural-tts-vocalising-threshold", NULL)))
  sev_threshold = node->value;

 event_listen (EVENT_SUBSYSTEM_IPC, einit_feedback_aural_ipc_event_handler);

 return 0;
}
