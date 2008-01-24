/*
 *  log-syslog.c
 *  einit
 *
 *  Split from log.c on 2007/12/24.
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

int einit_log_syslog_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_log_syslog_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_feedback,
 .name      = "eINIT Core Log Module (Syslog)",
 .rid       = "einit-log-syslog",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_log_syslog_configure
};

module_register(einit_log_syslog_self);

#endif

struct log_syslog_entry {
 char *message;
 unsigned char severity;
};

char have_syslog = 0;

struct log_syslog_entry **logbuffer = NULL;
pthread_mutex_t
 logmutex = PTHREAD_MUTEX_INITIALIZER,
 log_syslog_flushmutex = PTHREAD_MUTEX_INITIALIZER;
char dolog = 1, log_syslog_notices_to_stderr = 1;

int einit_log_syslog_in_switch = 0;

void einit_log_syslog_ipc_event_handler(struct einit_event *);
#if 0
signed int logsort (struct log_syslog_entry *, struct log_syslog_entry *);
#endif

char flush_log_syslog_buffer_to_syslog() {
 if (!logbuffer) return 1;

 if (have_syslog) {
  if (pthread_mutex_trylock(&logmutex)) return -1;

  while (logbuffer && logbuffer[0]) {

   char *slmessage = logbuffer[0]->message;
   char sev = logbuffer[0]->severity;

   efree (logbuffer[0]);
   logbuffer = (struct log_syslog_entry **)setdel ((void **)logbuffer, (void *)logbuffer[0]);

   pthread_mutex_unlock(&logmutex);

   if (slmessage) {
    if (sev < 3)
     syslog (LOG_CRIT, slmessage);
    else
     syslog (LOG_NOTICE, slmessage);

    efree (slmessage);
   }

   if (pthread_mutex_trylock(&logmutex)) return -1;
  }

  pthread_mutex_unlock(&logmutex);

  return 0;
 }

 return 1;
}

void flush_log_syslog_buffer_free() {
 if (!logbuffer) return;

 if (pthread_mutex_trylock(&logmutex)) return;

 while (logbuffer && logbuffer[0]) {

  char *slmessage = logbuffer[0]->message;

  logbuffer = (struct log_syslog_entry **)setdel ((void **)logbuffer, (void *)logbuffer[0]);

  pthread_mutex_unlock(&logmutex);

  if (slmessage) {
   efree (slmessage);
  }

  if (pthread_mutex_trylock(&logmutex)) return;
 }

 pthread_mutex_unlock(&logmutex);

 return;
}

char have_flush_thread = 0;

void flush_log_syslog_buffer_thread() {
 if (have_flush_thread) return;
 else
  have_flush_thread = 1;

 emutex_lock (&log_syslog_flushmutex);

 if (have_syslog) { flush_log_syslog_buffer_to_syslog(); }

 emutex_unlock (&log_syslog_flushmutex);

 have_flush_thread = 0;
}

void flush_log_syslog_buffer() {
 if (!have_flush_thread) {
  ethread_spawn_detached_run ((void *(*)(void *))flush_log_syslog_buffer_thread, NULL);
 }
}

#if 0
signed int logsort (struct log_syslog_entry *st1, struct log_syslog_entry *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->seqid - st1->seqid);
}
#endif

void log_syslog_notice_thread (struct log_syslog_entry *ne) {
 emutex_lock (&log_syslog_flushmutex);

 if (ne->severity < 3)
  syslog (LOG_CRIT, ne->message);
 else
  syslog (LOG_NOTICE, ne->message);

 emutex_unlock (&log_syslog_flushmutex);

 efree (ne->message);
 efree (ne);
}

void log_syslog_notice (unsigned char level, char *message) {
 struct log_syslog_entry *ne = emalloc (sizeof (struct log_syslog_entry));
 ne->message = estrdup (message),
 ne->severity = level;

 if (!have_syslog) {
  pthread_mutex_lock(&logmutex);
  logbuffer = (struct log_syslog_entry **)set_noa_add((void **)logbuffer, ne);
  pthread_mutex_unlock(&logmutex);
 } else {
  ethread_spawn_detached_run ((void *(*)(void *))log_syslog_notice_thread, ne);
 }
}

void einit_log_syslog_ipc_event_handler (struct einit_event *ev) {
 if (ev->argv && ev->argv[0] && ev->argv[1] && strmatch (ev->argv[0], "flush") && strmatch (ev->argv[1], "log")) {
  flush_log_syslog_buffer();

  ev->implemented = 1;
 }
}

void einit_log_syslog_einit_event_handler_service_enabled (struct einit_event *ev) {
 if (!dolog) return;

 if (ev->string && strmatch (ev->string, "logger")) {
  emutex_lock (&log_syslog_flushmutex);

  openlog ("einit", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

  emutex_unlock (&log_syslog_flushmutex);

//    fprintf (stderr, "opened syslog connection\n");

  have_syslog = 1;

  flush_log_syslog_buffer();
 }
}

void einit_log_syslog_einit_event_handler_service_disabled (struct einit_event *ev) {
 if (!dolog || !have_syslog) return;

 if (ev->string && strmatch (ev->string, "logger")) {
  have_syslog = 0;

  emutex_lock (&log_syslog_flushmutex);

  closelog();

  emutex_unlock (&log_syslog_flushmutex);

  flush_log_syslog_buffer();
 }
}

void einit_log_syslog_einit_event_handler_mode_switching (struct einit_event *ev) {
 if (!dolog) return;

  char logentry[BUFFERSIZE];
  einit_log_syslog_in_switch++;

  esprintf (logentry, BUFFERSIZE, "Now switching to mode \"%s\".", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

  log_syslog_notice(4, estrdup(logentry));
}

void einit_log_syslog_einit_event_handler_mode_switch_done (struct einit_event *ev) {
 if (!dolog) return;

 char logentry[BUFFERSIZE];
 einit_log_syslog_in_switch--;

 esprintf (logentry, BUFFERSIZE, "Mode \"%s\" is now in effect.", (ev->para && ((struct cfgnode *)(ev->para))->id) ? ((struct cfgnode *)(ev->para))->id : "unknown");

 log_syslog_notice(1, estrdup(logentry));

 if (!einit_log_syslog_in_switch && !have_syslog) { /* no more switches... and still no syslog */
  flush_log_syslog_buffer_free(); /* clear buffer */
 }
}

void einit_log_syslog_feedback_event_handler_unresolved_broken_services(struct einit_event *ev) {
 if (!dolog) return;

 char *tmp = set2str (' ', (const char **)ev->set);
 if (tmp) {
  char logentry[BUFFERSIZE];

  if (ev->type == einit_feedback_broken_services)
   esprintf (logentry, BUFFERSIZE, ev->set[1] ? "broken services: %s\n" : "broken service: %s\n", tmp);
  else
   esprintf (logentry, BUFFERSIZE, ev->set[1] ? "unresolved services: %s\n" : "unresolved service: %s\n", tmp);

  log_syslog_notice(1, estrdup(logentry));

  efree (tmp);
 }
}

void einit_log_syslog_feedback_event_handler_module_status (struct einit_event *ev) {
 if (!dolog) return;

  if (ev->string) {
   char logentry[BUFFERSIZE];
   esprintf (logentry, BUFFERSIZE, "module \"%s\": %s",
             (ev->para && ((struct lmodule *)(ev->para))->module && ((struct lmodule *)(ev->para))->module->rid ? ((struct lmodule *)(ev->para))->module->rid : "unknown"), ev->string);

   log_syslog_notice(1, estrdup(logentry));
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

  log_syslog_notice(5, estrdup(logentry));
 }
}

void einit_log_syslog_feedback_event_handler_notice (struct einit_event *ev) {
 if (!dolog) return;

 if (ev->string) {
  strtrim (ev->string);

  log_syslog_notice(ev->flag, estrdup(ev->string));

  if (einit_quietness < 2) {
   if (ev->flag < 3) {
    eprintf (stderr, " ** %s\n", ev->string);
   } else if (ev->flag < 6) {
    eprintf (stderr, " >> %s\n", ev->string);
   }
   fflush (stderr);
  }
 }

 return;
}

int einit_log_syslog_cleanup (struct lmodule *this) {
 event_ignore (einit_ipc_request_generic, einit_log_syslog_ipc_event_handler);
 event_ignore (einit_core_service_enabled, einit_log_syslog_einit_event_handler_service_enabled);
 event_ignore (einit_core_mode_switching, einit_log_syslog_einit_event_handler_mode_switching);
 event_ignore (einit_core_mode_switch_done, einit_log_syslog_einit_event_handler_mode_switch_done);
 event_ignore (einit_feedback_broken_services, einit_log_syslog_feedback_event_handler_unresolved_broken_services);
 event_ignore (einit_feedback_unresolved_services, einit_log_syslog_feedback_event_handler_unresolved_broken_services);
 event_ignore (einit_feedback_module_status, einit_log_syslog_feedback_event_handler_module_status);
 event_ignore (einit_feedback_notice, einit_log_syslog_feedback_event_handler_notice);

 return 0;
}

int einit_log_syslog_configure (struct lmodule *r) {
 module_init (r);

 r->cleanup = einit_log_syslog_cleanup;

 event_listen (einit_ipc_request_generic, einit_log_syslog_ipc_event_handler);
 event_listen (einit_core_service_enabled, einit_log_syslog_einit_event_handler_service_enabled);
 event_listen (einit_core_service_disabled, einit_log_syslog_einit_event_handler_service_disabled);
 event_listen (einit_core_mode_switching, einit_log_syslog_einit_event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, einit_log_syslog_einit_event_handler_mode_switch_done);
 event_listen (einit_feedback_broken_services, einit_log_syslog_feedback_event_handler_unresolved_broken_services);
 event_listen (einit_feedback_unresolved_services, einit_log_syslog_feedback_event_handler_unresolved_broken_services);
 event_listen (einit_feedback_module_status, einit_log_syslog_feedback_event_handler_module_status);
 event_listen (einit_feedback_notice, einit_log_syslog_feedback_event_handler_notice);

 setlogmask (LOG_UPTO (LOG_INFO));

 return 0;
}
