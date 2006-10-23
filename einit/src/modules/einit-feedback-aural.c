/*
 *  einit-feedback-aural.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/09/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <pthread.h>
#include <einit/pexec.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char *provides[] = {"feedback-aural", NULL};
char *requires[] = {"audio", "mount/critical", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_FEEDBACK,
	.options	= 0,
	.name		= "aural/tts feedback module",
	.rid		= "einit-feedback-aural",
	.provides	= provides,
	.requires	= requires,
	.notwith	= NULL
};

struct lmodule *self_l = NULL;

void feedback_event_handler(struct einit_event *);
void synthesize (char *);
char *synthesizer;
int sev_threshold = 2;

int examine_configuration (struct lmodule *irr) {
 int pr = 0;

 if (!cfg_getstring("configuration-feedback-aural-tts-synthesizer-command", NULL)) {
  fputs (" * configuration variable \"configuration-feedback-aural-tts-synthesizer-command\" not found.\n", stderr);
  pr++;
 }
 if (!cfg_getnode("configuration-feedback-aural-tts-vocalising-threshold", NULL)) {
  fputs (" * configuration variable \"configuration-feedback-aural-tts-vocalising-threshold\" not found.\n", stderr);
  pr++;
 }

 return pr;
}

int configure (struct lmodule *this) {
 pexec_configure (this);

 struct cfgnode *node;

 synthesizer = cfg_getstring ("configuration-feedback-aural-tts-synthesizer-command", NULL);

 if (node = cfg_getnode ("configuration-feedback-aural-tts-vocalising-threshold", NULL))
  sev_threshold = node->value;

 self_l = this;
}

int cleanup (struct lmodule *this) {
 pexec_cleanup(this);
}

int enable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&self_l->imutex);
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

 char phrase[2048], hostname[128];
 phrase[0] = 0;

 if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    if (gethostname (hostname, 128)) strcpy (hostname, "localhost");
    hostname[127] = 0;
    snprintf (phrase, 2048, "Host \"%s\" now switching to mode \"%s\".", hostname, newmode);
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
    snprintf (phrase, 2048, "New mode \"%s\" is now in effect.", currentmode);
    break;
  }
 } else if (ev->type == EVE_FEEDBACK_NOTICE) {
  if (synthesizer && ev->string && (ev->flag < sev_threshold)) {
   char *tx;
   strtrim (ev->string);

   if (!(tx = strrchr (ev->string, ':'))) tx = ev->string;
   else tx ++;

   if (tx) strncat (phrase, tx, 2047);
  }
 }

 if (phrase[0]) synthesize (phrase);

 pthread_mutex_unlock (&self_l->imutex);
 return;
}

/* BUG: using popen/pclose might interfere with the scheduler's zombie-auto-reaping code,
        if we're running this on a system with a buggy pthreads implementation */
void synthesize (char *string) {
 FILE *px = popen (synthesizer, "w");

 if (px) {
  fputs (string, px);

  if (pclose (px) == -1)
   perror ("tts: pclose");
 } else
  perror ("tts: popen");
}
