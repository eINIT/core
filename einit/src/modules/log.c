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

#include <syslog.h>

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
 .mode      = einit_module_feedback,
 .name      = "eINIT Core Log Module",
 .rid       = "einit-log",
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

char have_syslog = 0;

struct log_entry **logbuffer = NULL;
pthread_mutex_t logmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_flushmutex = PTHREAD_MUTEX_INITIALIZER;
char dolog = 1, log_notices_to_stderr = 1;

int einit_log_in_switch = 0;

void einit_log_ipc_event_handler(struct einit_event *);
signed int logsort (struct log_entry *, struct log_entry *);

char flush_log_buffer_to_syslog() {
 if (!logbuffer) return 1;

 if (have_syslog) {
  if (pthread_mutex_trylock(&logmutex)) return -1;

  while (logbuffer && logbuffer[0]) {

   char *slmessage = logbuffer[0]->message;
//   char severity = logbuffer[0]->severity;

//   fprintf (stderr, "message: %s\n", slmessage);

   logbuffer = (struct log_entry **)setdel ((void **)logbuffer, (void *)logbuffer[0]);

   pthread_mutex_unlock(&logmutex);

   if (slmessage) {
    syslog(/*((severity <= 2) ? LOG_CRIT :
           ((severity <= 5) ? LOG_WARNING :
           ((severity <= 8) ? LOG_NOTICE :
      LOG_DEBUG)))*/ LOG_NOTICE,
    slmessage);
    free (slmessage);
   }

   if (pthread_mutex_trylock(&logmutex)) return -1;
  }

  pthread_mutex_unlock(&logmutex);

  return 0;
 }

 return 1;
}

void flush_log_buffer_free() {
 if (!logbuffer) return;

 if (pthread_mutex_trylock(&logmutex)) return;

 while (logbuffer && logbuffer[0]) {

  char *slmessage = logbuffer[0]->message;

  logbuffer = (struct log_entry **)setdel ((void **)logbuffer, (void *)logbuffer[0]);

  pthread_mutex_unlock(&logmutex);

  if (slmessage) {
   free (slmessage);
  }

  if (pthread_mutex_trylock(&logmutex)) return;
 }

 pthread_mutex_unlock(&logmutex);

 return;
}

void flush_log_buffer() {
 if (pthread_mutex_trylock (&log_flushmutex)) return;

 if (have_syslog) { flush_log_buffer_to_syslog(); }

 pthread_mutex_unlock (&log_flushmutex);
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

void einit_log_einit_event_handler_service_update (struct einit_event *ev) {
 if (!dolog) return;

  if (ev->status & status_enabled) {
   if (ev->module && ev->module->si && ev->module->si->provides && inset ((const void **)ev->module->si->provides, "logger", SET_TYPE_STRING)) {
    openlog ("einit", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

//    fprintf (stderr, "opened syslog connection\n");

    have_syslog = 1;

    flush_log_buffer();
   }
  } else if (!(ev->status & status_enabled)) {
   if (ev->module && ev->module->si && ev->module->si->provides && inset ((const void **)ev->module->si->provides,"logger", SET_TYPE_STRING)) {
    have_syslog = 0;

    closelog();

    flush_log_buffer();
   }
  }
}

void einit_log_einit_event_handler_mode_switching (struct einit_event *ev) {
 if (!dolog) return;

  char logentry[BUFFERSIZE];
  einit_log_in_switch++;

  esprintf (logentry, BUFFERSIZE, "Now switching to mode \"%s\".", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

  if (!have_syslog) {
   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (logentry),
    .severity = 0
   };

   pthread_mutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   pthread_mutex_unlock(&logmutex);
  } else {
   syslog(LOG_NOTICE, logentry);
  }

//  if (have_syslog) flush_log_buffer();
}

void einit_log_einit_event_handler_mode_switch_done (struct einit_event *ev) {
 if (!dolog) return;

 char logentry[BUFFERSIZE];
 einit_log_in_switch--;

 esprintf (logentry, BUFFERSIZE, "Mode \"%s\" is now in effect.", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

 if (!have_syslog) {
  struct log_entry ne = {
   .seqid = ev->seqid,
   .timestamp = ev->timestamp,
   .message = estrdup (logentry),
   .severity = 1
  };

  if (einit_quietness < 2)
   eprintf (stderr, " ** %s\n", logentry);

  pthread_mutex_lock(&logmutex);
  logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
  pthread_mutex_unlock(&logmutex);
  flush_log_buffer();
 } else {
  syslog(LOG_NOTICE, logentry);
 }

 if (!einit_log_in_switch && !have_syslog) { /* no more switches... and still no syslog */
  flush_log_buffer_free(); /* clear buffer */
 }
}

void einit_log_feedback_event_handler_unresolved_broken_services(struct einit_event *ev) {
 if (!dolog) return;

 char *tmp = set2str (' ', (const char **)ev->set);
 if (tmp) {
  char logentry[BUFFERSIZE];

  if (ev->type == einit_feedback_broken_services)
   esprintf (logentry, BUFFERSIZE, ev->set[1] ? "broken services: %s\n" : "broken service: %s\n", tmp);
  else
   esprintf (logentry, BUFFERSIZE, ev->set[1] ? "unresolved services: %s\n" : "unresolved service: %s\n", tmp);

  if (!have_syslog) {
   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (logentry),
    .severity = 0
   };

   pthread_mutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   pthread_mutex_unlock(&logmutex);
  } else {
   syslog(LOG_NOTICE, logentry);
  }

//  if (have_syslog) flush_log_buffer();

  free (tmp);
 }
}

void einit_log_feedback_event_handler_module_status (struct einit_event *ev) {
 if (!dolog) return;

  if (ev->string) {
   char logentry[BUFFERSIZE];
   esprintf (logentry, BUFFERSIZE, "module \"%s\": %s",
             (ev->para && ((struct lmodule *)(ev->para))->module && ((struct lmodule *)(ev->para))->module->rid ? ((struct lmodule *)(ev->para))->module->rid : "unknown"), ev->string);

   if (!have_syslog) {

    struct log_entry ne = {
     .seqid = ev->seqid,
     .timestamp = ev->timestamp,
     .message = estrdup (logentry),
     .severity = 0
    };

    pthread_mutex_lock(&logmutex);
    logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
    pthread_mutex_unlock(&logmutex);

   } else {
    syslog(LOG_NOTICE, logentry);
   }
  }

  if ((ev->status & status_ok) || (ev->task & einit_module_feedback_show)){
   char logentry[BUFFERSIZE];
   char *action = "unknown";

   if ((ev->task & einit_module_feedback_show)) {
    if (ev->task & einit_module_enable) {
     action = "enabling";
    } else if (ev->task & einit_module_disable) {
     action = "disabling";
    } else if (ev->task & einit_module_custom) {
     action = "custom";
    }
   } else {
    if (ev->task & einit_module_enable) {
     action = "enabled";
    } else if (ev->task & einit_module_disable) {
     action = "disabled";
    } else if (ev->task & einit_module_custom) {
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

   if (!have_syslog) {
    struct log_entry ne = {
     .seqid = ev->seqid,
     .timestamp = ev->timestamp,
     .message = estrdup (logentry),
     .severity = 0
    };

    pthread_mutex_lock(&logmutex);
    logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
    pthread_mutex_unlock(&logmutex);
   } else {
    syslog(LOG_NOTICE, logentry);
   }

//   if (have_syslog) flush_log_buffer();
  }
}

void einit_log_feedback_event_handler_notice (struct einit_event *ev) {
 if (!dolog) return;

 if (ev->string) {
  strtrim (ev->string);

  if (!have_syslog) {
   struct log_entry ne = {
    .seqid = ev->seqid,
    .timestamp = ev->timestamp,
    .message = estrdup (ev->string),
    .severity = ev->flag
   };

   pthread_mutex_lock(&logmutex);
   logbuffer = (struct log_entry **)setadd((void **)logbuffer, (void *)&ne, sizeof (struct log_entry));
   pthread_mutex_unlock(&logmutex);
  } else {
   syslog(/*((severity <= 2) ? LOG_CRIT :
          ((severity <= 5) ? LOG_WARNING :
          ((severity <= 8) ? LOG_NOTICE :
     LOG_DEBUG)))*/ LOG_NOTICE,
     ev->string);
  }

  if (einit_quietness < 2) {
   if (ev->flag < 3) {
    eprintf (stderr, " ** %s\n", ev->string);
   } else if (ev->flag < 6) {
    eprintf (stderr, " >> %s\n", ev->string);
   }
   fflush (stderr);
  }

//  if (have_syslog) flush_log_buffer();
 }

 return;
}

int einit_log_cleanup (struct lmodule *this) {
 event_ignore (einit_ipc_request_generic, einit_log_ipc_event_handler);
 event_ignore (einit_core_service_update, einit_log_einit_event_handler_service_update);
 event_ignore (einit_core_mode_switching, einit_log_einit_event_handler_mode_switching);
 event_ignore (einit_core_mode_switch_done, einit_log_einit_event_handler_mode_switch_done);
 event_ignore (einit_feedback_broken_services, einit_log_feedback_event_handler_unresolved_broken_services);
 event_ignore (einit_feedback_unresolved_services, einit_log_feedback_event_handler_unresolved_broken_services);
 event_ignore (einit_feedback_module_status, einit_log_feedback_event_handler_module_status);
 event_ignore (einit_feedback_notice, einit_log_feedback_event_handler_notice);

 return 0;
}

int einit_log_configure (struct lmodule *r) {
 module_init (r);

 r->cleanup = einit_log_cleanup;

 event_listen (einit_ipc_request_generic, einit_log_ipc_event_handler);
 event_listen (einit_core_service_update, einit_log_einit_event_handler_service_update);
 event_listen (einit_core_mode_switching, einit_log_einit_event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, einit_log_einit_event_handler_mode_switch_done);
 event_listen (einit_feedback_broken_services, einit_log_feedback_event_handler_unresolved_broken_services);
 event_listen (einit_feedback_unresolved_services, einit_log_feedback_event_handler_unresolved_broken_services);
 event_listen (einit_feedback_module_status, einit_log_feedback_event_handler_module_status);
 event_listen (einit_feedback_notice, einit_log_feedback_event_handler_notice);

 setlogmask (LOG_UPTO (LOG_INFO));

 return 0;
}
