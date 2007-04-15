/*
 *  log.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/03/2007.
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
#include <einit/bitch.h>
#include <einit/tree.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_log_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_log_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "eINIT Core Log Module",
 .rid       = "log",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_log_configure
};

module_register(einit_log_self);

#endif

struct log_entry {
 uint32_t seqid;
 time_t timestamp;

 char *message;
 unsigned char severity;
};

struct log_entry **logbuffer = NULL;
pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;
char dolog = 1, log_notices_to_stderr = 1;

void einit_log_feedback_event_handler(struct einit_event *);
void einit_log_ipc_event_handler(struct einit_event *);
void einit_log_event_event_handler(struct einit_event *);
signed int logsort (struct log_entry *, struct log_entry *);

void flush_log_buffer () {
 struct log_entry **slog = NULL;
 uint32_t i = 0;
 struct cfgnode *lognode = cfg_getnode("configuration-system-log", NULL);
 FILE *logfile;

 dolog = !lognode || lognode->flag;

 if (!logbuffer) return;

 if (dolog) {

  if (!lognode || !lognode->svalue || !(logfile = fopen (lognode->svalue, "a")))
   logfile = fopen ("/tmp/einit.log", "a");

  if (logfile) {
   emutex_lock(&logmutex);

   if (logbuffer) {
    slog = (struct log_entry **)setdup ((const void **)logbuffer, SET_NOALLOC);
    setsort ((void **)logbuffer, 0, (signed int(*)(const void *, const void*))logsort);

    for (; slog[i]; i++) {
     char timebuffer[50];
     char txbuffer[BUFFERSIZE];
     struct tm *tb = localtime (&(slog[i]->timestamp));

     strftime (timebuffer, 50, "%c", tb);

     if (slog[i]->message) {
      strtrim (slog[i]->message);
      esprintf (txbuffer, BUFFERSIZE, "%s; (event-seq=+%i, priority+%i) %s\n", timebuffer, slog[i]->seqid, slog[i]->severity, slog[i]->message);
     } else
      esprintf (txbuffer, BUFFERSIZE, "%s; (event-seq=+%i, priority+%i) <no message>\n", timebuffer, slog[i]->seqid, slog[i]->severity);

     eputs (txbuffer, logfile);
    }

    for (i = 0; logbuffer[i]; i++) {
     if (logbuffer[i]->message) free (logbuffer[i]->message);
    }

    free (logbuffer);
    logbuffer = NULL;
   }
   emutex_unlock(&logmutex);

   fclose (logfile);
  } else {
   bitch (bitch_stdio, errno, "could not open logfile for saving.");
  }
 } else {
  emutex_lock(&logmutex);
  for (; logbuffer[i]; i++) {
   if (logbuffer[i]->message) free (logbuffer[i]->message);
  }

  free (logbuffer);
  logbuffer = NULL;

  dolog = 0;
  emutex_unlock(&logmutex);
 }
}

signed int logsort (struct log_entry *st1, struct log_entry *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->seqid - st1->seqid);
}

void einit_log_ipc_event_handler (struct einit_event *ev) {
 if (ev->argv && ev->argv[0] && ev->argv[1] && strmatch (ev->argv[0], "flush") && strmatch (ev->argv[1], "log")) {
  flush_log_buffer();

  ev->implemented = 1;
 }
}

void einit_log_einit_event_handler(struct einit_event *ev) {
 if (!dolog) return;

 if (ev->type == einit_core_mode_switching) {
  char logentry[BUFFERSIZE];

  esprintf (logentry, BUFFERSIZE, "Now switching to mode \"%s\".", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

  struct log_entry ne = {
   .seqid = ev->seqid,
   .timestamp = ev->timestamp,
   .message = estrdup (logentry),
   .severity = 0
  };

  emutex_lock(&logmutex);
  logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
  emutex_unlock(&logmutex);
 } else if (ev->type == einit_core_mode_switch_done) {
  char logentry[BUFFERSIZE];

  esprintf (logentry, BUFFERSIZE, "Mode \"%s\" is now in effect.", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

  struct log_entry ne = {
   .seqid = ev->seqid,
   .timestamp = ev->timestamp,
   .message = estrdup (logentry),
   .severity = 0
  };

  emutex_lock(&logmutex);
  logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
  emutex_unlock(&logmutex);

  flush_log_buffer();
 }
}

void einit_log_feedback_event_handler(struct einit_event *ev) {
 if (!dolog) return;

 if ((ev->type == einit_feedback_unresolved_services) || (ev->type == einit_feedback_broken_services)) {
  char *tmp = set2str (' ', (const char **)ev->set);
  if (tmp) {
   char logentry[BUFFERSIZE];

   if (ev->type == einit_feedback_broken_services)
    esprintf (logentry, BUFFERSIZE, ev->set[1] ? "broken services: %s\n" : "broken service: %s\n", tmp);
   else
    esprintf (logentry, BUFFERSIZE, ev->set[1] ? "unresolved services: %s\n" : "unresolved service: %s\n", tmp);

   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (logentry),
    .severity = 0
   };

   emutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   emutex_unlock(&logmutex);

   free (tmp);
  }
 } else if (ev->type == einit_feedback_module_status) {
  if (ev->string) {
   char logentry[BUFFERSIZE];

   esprintf (logentry, BUFFERSIZE, "module \"%s\": %s",
             (ev->para && ((struct lmodule *)(ev->para))->module && ((struct lmodule *)(ev->para))->module->rid ? ((struct lmodule *)(ev->para))->module->rid : "unknown"), ev->string);

   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (logentry),
    .severity = 0
   };

   emutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   emutex_unlock(&logmutex);
  }

  if ((ev->status & STATUS_OK) || (ev->task & MOD_FEEDBACK_SHOW)){
   char logentry[BUFFERSIZE];
   char *action = "uknown";

   if ((ev->task & MOD_FEEDBACK_SHOW)) {
    if (ev->task & MOD_ENABLE) {
     action = "enabling";
    } else if (ev->task & MOD_DISABLE) {
     action = "disabling";
    } else if (ev->task & MOD_CUSTOM) {
     action = "custom";
    }
   } else {
    if (ev->task & MOD_ENABLE) {
     action = "enabled";
    } else if (ev->task & MOD_DISABLE) {
     action = "disabled";
    } else if (ev->task & MOD_CUSTOM) {
     action = "custom";
    }
   }

   if (ev->flag) {
    esprintf (logentry, BUFFERSIZE, "module \"%s\": %s (with %i warnings)",
             (ev->para && ((struct lmodule *)(ev->para))->module && ((struct lmodule *)(ev->para))->module->rid ? ((struct lmodule *)(ev->para))->module->rid : "unknown"), action, ev->flag);
   } else {
    esprintf (logentry, BUFFERSIZE, "module \"%s\": %s",
              (ev->para && ((struct lmodule *)(ev->para))->module && ((struct lmodule *)(ev->para))->module->rid ? ((struct lmodule *)(ev->para))->module->rid : "unknown"), action);
   }

   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (logentry),
    .severity = 0
   };

   emutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   emutex_unlock(&logmutex);
  }
 } else if ((ev->type == einit_feedback_notice) && ev->string) {
  strtrim (ev->string);

  struct log_entry ne = {
   .seqid = ev->seqid,
   .timestamp = ev->timestamp,
   .message = estrdup (ev->string),
   .severity = ev->flag
  };

  emutex_lock(&logmutex);
  logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
  emutex_unlock(&logmutex);

  if (ev->flag < 3) {
   eprintf (stderr, " ** %s\n", ev->string);
  } else if (ev->flag < 6) {
   eprintf (stderr, " >> %s\n", ev->string);
  }

  fflush (stderr);

  if (log_notices_to_stderr) {
  }
 }

 return;
}

int einit_log_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_ipc, einit_log_ipc_event_handler);
 event_ignore (einit_event_subsystem_feedback, einit_log_feedback_event_handler);
 event_ignore (einit_event_subsystem_core, einit_log_einit_event_handler);

 return 0;
}

int einit_log_configure (struct lmodule *r) {
 module_init (r);

 r->cleanup = einit_log_cleanup;

 event_listen (einit_event_subsystem_ipc, einit_log_ipc_event_handler);
 event_listen (einit_event_subsystem_feedback, einit_log_feedback_event_handler);
 event_listen (einit_event_subsystem_core, einit_log_einit_event_handler);

 return 0;
}
