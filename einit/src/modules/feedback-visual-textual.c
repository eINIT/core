/*
 *  feedback-visual-textual.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/03/2006.
 *  Renamed from vis-text.c on 11/10/2006.
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
#include <einit/module-logic.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef LINUX
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
/* okay, i think i found the proper file now */
#include <asm/ioctls.h>
#include <linux/vt.h>
#endif

#ifdef POSIXREGEX
#include <regex.h>
#include <dirent.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif


int einit_feedback_visual_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_feedback_visual_provides[] = {"feedback-textual", NULL};
const struct smodule einit_feedback_visual_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_feedback,
 .name      = "visual/text-based feedback module",
 .rid       = "feedback-visual-textual",
 .si        = {
  .provides = einit_feedback_visual_provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_feedback_visual_configure
};

module_register(einit_feedback_visual_self);

#endif

uint32_t shutdownfailuretimeout = 10;
char enableansicodes = 1;

struct feedback_textual_command {
 struct lmodule *module;
 enum einit_module_status status;
 char *message;
 char *statusline;
 uint32_t seqid;
 uint32_t warnings;
 time_t ctime;
};

struct message_log {
 uint32_t seqid;
 char *message;
};

struct feedback_textual_module_status {
 struct lmodule *module;
 enum einit_module_status laststatus;
 struct message_log **log;
 uint32_t warnings;
 time_t lastchange;
};

struct feedback_textual_command **feedback_textual_commandQ = NULL;
struct feedback_textual_module_status **feedback_textual_modules = NULL;

pthread_mutex_t
 feedback_textual_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t feedback_textual_commandQ_cond = PTHREAD_COND_INITIALIZER;

char *feedback_textual_statusline = "\e[0;0H[ \e[31m....\e[0m ] \e[34minitialising\e[0m\e[0K\n";

void feedback_textual_queue_comand (struct lmodule *module, enum einit_module_status status, char *message, uint32_t seqid, time_t ctime, char *statusline, uint32_t warnings) {
 struct feedback_textual_command tnc;
 memset (&tnc, 0, sizeof (struct feedback_textual_command));

 tnc.module = module;
 tnc.statusline = statusline;
 tnc.status = status;
 tnc.seqid = seqid;
 if (message)
  tnc.message = estrdup (message);
 tnc.ctime = ctime;
 tnc.warnings = warnings;

 emutex_lock (&feedback_textual_commandQ_mutex);

 feedback_textual_commandQ = (struct feedback_textual_command **)setadd ((void **)feedback_textual_commandQ, (void *)(&tnc), sizeof (struct feedback_textual_command));

 emutex_unlock (&feedback_textual_commandQ_mutex);

 pthread_cond_broadcast (&feedback_textual_commandQ_cond);
}

pthread_t feedback_textual_thread;
char einit_feedback_visual_textual_worker_thread_running = 0,
     einit_feedback_visual_textual_worker_thread_keep_running = 1;

signed int feedback_log_sort (struct message_log *st1, struct message_log *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->seqid - st1->seqid);
}

signed int feedback_time_sort (struct feedback_textual_module_status *st1, struct feedback_textual_module_status *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->lastchange - st1->lastchange);
}

void feedback_process_textual_noansi(struct feedback_textual_module_status *st) {
 char statuscode[] = { '(', '-', '-', '-', '-', ')', 0 };
 char *rid = (st->module->module && st->module->module->rid) ? st->module->module->rid : "no idea";
 char *name = (st->module->module && st->module->module->name) ? st->module->module->name : "no name";

 if (st->module->status == status_idle) {
  statuscode[1] = 'I';
 } else {
  if (st->module->status & status_enabled) {
   statuscode[2] = 'E';
  }
  if (st->module->status & status_disabled) {
   statuscode[3] = 'D';
  }
  if (st->module->status & status_working) {
   statuscode[4] = 'w';
  }

  if (st->module->status & status_failed) {
   statuscode[0] = '!';
   statuscode[5] = '!';
  }
 }

 if (st->log) {
  uint32_t y = 0;

  for (; st->log[y]; y++) ;

  if (y != 0) {
   y--;
   eprintf (stdout, "%s %s (%s): %s\n", statuscode, name, rid, st->log[y]->message);
  }
 } else {
  eprintf (stdout, "%s %s (%s)\n", statuscode, name, rid);
 }
}

void feedback_process_textual_ansi(struct feedback_textual_module_status *st) {
 char *name = (st->module->module && st->module->module->name) ? st->module->module->name : "no name";

 char *wmarker = "";
 char *emarker = "";
 char *rmarker = " ";
 char *status = "\e[31mwtf?\e[0m";
 char wmbuffer[BUFFERSIZE];
 char do_details = 0;

 if (st->module->status == status_idle) {
  status = "\e[31midle\e[0m";
 } else {
  if (st->module->status & status_enabled) {
   status = "\e[32menab\e[0m";
  }
  if (st->module->status & status_disabled) {
   status = "\e[33mdisa\e[0m";
  }
  if (st->module->status & status_working) {
   status = "\e[31m....\e[0m";
  }

  if (st->module->status & status_failed) {
   emarker = " \e[31m(failed)\e[0m";
   do_details = 1;
  }
 }

 if (st->warnings > 1) {
  wmarker = wmbuffer;
  esprintf (wmbuffer, BUFFERSIZE, "\e[36m(%i warnings)\e[0m ", st->warnings);
  do_details = 1;
 } else if (st->warnings == 1) {
  wmarker = "\e[36m(1 warning)\e[0m ";
  do_details = 1;
 }

 if (st->log) {
  uint32_t y = 0;

  for (; st->log[y]; y++) ;

  if (do_details && (y > 1)) {
   eprintf (stdout, "%s[ %s ]%s %s%s; messages:\e[0K\n", rmarker, status, emarker, wmarker, name);

   y = 0;
   for (; st->log[y]; y++)
    eprintf (stdout, "  \e[37m*\e[0m %s\e[0K\n", st->log[y]->message);
  } else if (y != 0) {
   y--;

   eprintf (stdout, "%s[ %s ]%s %s%s: %s\e[0K\n", rmarker, status, emarker, wmarker, name, st->log[y]->message);
  } else {
   eprintf (stdout, "%s[ %s ]%s %s%s\e[0K\n", rmarker, status, emarker, wmarker, name);
  }
 } else {
  eprintf (stdout, "%s[ %s ]%s %s%s\e[0K\n", rmarker, status, emarker, wmarker, name);
 }
}

void feedback_textual_update_screen () {
 emutex_lock (&feedback_textual_modules_mutex);

 if (enableansicodes) {
  eputs (feedback_textual_statusline, stdout);
 } else
  eputs ("\n", stdout);

 if (feedback_textual_modules) {
  uint32_t i = 0;

/*  setsort ((void **)feedback_textual_modules, 0, (signed int(*)(const void *, const void*))feedback_time_sort);*/

  if (enableansicodes) {
   for (; feedback_textual_modules[i]; i++)
    feedback_process_textual_ansi(feedback_textual_modules[i]);
  }
  else {
   for (; feedback_textual_modules[i]; i++)
    feedback_process_textual_noansi(feedback_textual_modules[i]);
  }
 }

 emutex_unlock (&feedback_textual_modules_mutex);
}

void feedback_textual_update_module (struct lmodule *module, time_t ctime, uint32_t seqid, char *message, uint32_t warnings) {
 char create_module = 1;
 char do_update = 0;

 emutex_lock (&feedback_textual_modules_mutex);

 if (feedback_textual_modules) {
  uint32_t i = 0;

  for (; feedback_textual_modules[i]; i++) {
   if (feedback_textual_modules[i]->module == module) {
    create_module = 0;

    if (feedback_textual_modules[i]->lastchange <= ctime) {
     feedback_textual_modules[i]->lastchange = ctime;
     do_update = 1;
    }
    if (message) {
     do_update = 1;
     struct message_log ne = {
      .seqid = seqid,
      .message = message
     };

     feedback_textual_modules[i]->log = (struct message_log **)setadd ((void **)(feedback_textual_modules[i]->log), &ne, sizeof (struct message_log));

     if (feedback_textual_modules[i]->log) {
      setsort ((void **)feedback_textual_modules[i]->log, 0, (signed int(*)(const void *, const void*))feedback_log_sort);
     }
    }

    if (warnings > feedback_textual_modules[i]->warnings) {
     feedback_textual_modules[i]->warnings = warnings;
    }
   }
  }
 }

 if (create_module) {
  struct feedback_textual_module_status nm;

  memset (&nm, 0, sizeof (struct feedback_textual_module_status));

  nm.module = module;
  nm.lastchange = ctime;

  nm.warnings = warnings;

  if (message) {
   struct message_log ne = {
    .seqid = seqid,
    .message = message
   };

   nm.log = (struct message_log **)setadd (NULL, &ne, sizeof (struct message_log));
  }

  feedback_textual_modules = (struct feedback_textual_module_status **)setadd ((void **)feedback_textual_modules, &nm, sizeof (struct feedback_textual_module_status));

  do_update = 1;
 }

 emutex_unlock (&feedback_textual_modules_mutex);

 if (do_update) {
  feedback_textual_update_screen ();
 }
}

void feedback_textual_process_command (struct feedback_textual_command *command) {
 if (command->statusline) {
  feedback_textual_statusline = command->statusline;
  feedback_textual_update_screen ();
 }

 if (command->module) {
  feedback_textual_update_module (command->module, command->ctime, command->seqid, command->message, command->warnings);
 }
}

signed int feedback_command_sort (struct feedback_textual_command *st1, struct feedback_textual_command *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->seqid - st1->seqid);
}

void *einit_feedback_visual_textual_worker_thread (void *irr) {
 einit_feedback_visual_textual_worker_thread_running = 1;

 while (einit_feedback_visual_textual_worker_thread_keep_running) {
  while (feedback_textual_commandQ) {
   struct feedback_textual_command *command = NULL;

   emutex_lock (&feedback_textual_commandQ_mutex);
   if (feedback_textual_commandQ) {
    setsort ((void **)feedback_textual_commandQ, 0, (signed int(*)(const void *, const void*))feedback_command_sort);

    if ((command = feedback_textual_commandQ[0])) {
     void *it = command;
     command = emalloc (sizeof (struct feedback_textual_command));
     memcpy (command, it, sizeof (struct feedback_textual_command));

     feedback_textual_commandQ = (struct feedback_textual_command **)setdel ((void **)feedback_textual_commandQ, it);
    }
   }
   emutex_unlock (&feedback_textual_commandQ_mutex);

   if (command) {
    feedback_textual_process_command (command);

    free (command);
   }
  }

  emutex_lock (&feedback_textual_commandQ_cond_mutex);
  pthread_cond_wait (&feedback_textual_commandQ_cond, &feedback_textual_commandQ_cond_mutex);
  emutex_unlock (&feedback_textual_commandQ_cond_mutex);
 }

 return NULL;
}

void einit_feedback_visual_feedback_event_handler(struct einit_event *ev) {
 if (ev->type == einit_feedback_broken_services) {
  char *tmp = set2str (' ', (const char **)ev->set);
  if (tmp) {
   eprintf (stderr, ev->set[1] ? " >> broken services: %s\n" : " >> broken service: %s\n", tmp);

   free (tmp);
  }
 } else if (ev->type == einit_feedback_unresolved_services) {
  char *tmp = set2str (' ', (const char **)ev->set);
  if (tmp) {
   eprintf (stderr, ev->set[1] ? " >> unresolved services: %s\n" : " >> unresolved service: %s\n", tmp);

   free (tmp);
  }
 } else if (ev->type == einit_feedback_module_status) {
  feedback_textual_queue_comand (ev->module, ev->status, ev->string, ev->seqid, ev->timestamp, NULL, ev->flag);
 }
}

void einit_feedback_visual_einit_event_handler(struct einit_event *ev) {
 if (ev->type == einit_core_service_update) {
  feedback_textual_queue_comand (ev->module, ev->status, NULL, ev->seqid, ev->timestamp, NULL, ev->flag);
 } else if (ev->type == einit_core_mode_switching) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

  feedback_textual_queue_comand (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);
 } else if (ev->type == einit_core_mode_switch_done) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "\e[0;0H[ \e[32mdone\e[0m ] \e[34mswitch complete: mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

  feedback_textual_queue_comand (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);
 }
}

/*
  -------- power event-handler -------------------------------------------------
 */
void einit_feedback_visual_power_event_handler(struct einit_event *ev) {
 struct cfgnode *n;

 if ((ev->type == einit_power_down_imminent) || (ev->type == einit_power_reset_imminent)) {
// shutdown imminent
  uint32_t c = shutdownfailuretimeout;
  char errors = 0;

  emutex_lock (&feedback_textual_modules_mutex);

   if (feedback_textual_modules) {
    uint32_t i = 0;
    for (; feedback_textual_modules[i]; i++) {
     if ((feedback_textual_modules[i])->warnings) {
      errors = 1;
      break;
     } else if (feedback_textual_modules[i]->module->status & status_failed) {
      errors = 1;
      break;
     }
    }
   }
  emutex_unlock (&feedback_textual_modules_mutex);

  if (errors)
   while (c) {
    if (enableansicodes) {
     eprintf (stdout, "\e[0;0H\e[0m[ \e[31m%4.4i\e[0m ] \e[31mWarning: Errors occured while shutting down, waiting...\e[0m\n", c);
    } else {
     eprintf (stdout, "[ %4.4i ] Warning: Errors occured while shutting down, waiting...\n", c);
    }

    c -= 1 - sleep (1);
   }

 }

 return;
}

void einit_feedback_visual_ipc_event_handler(struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "examine") && strmatch(ev->argv[1], "configuration")) {
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-use-ansi-codes\" not found.\n", ev->output);
   ev->ipc_return++;
  }
  if (!cfg_getnode("configuration-feedback-visual-std-io", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-std-io\" not found.\n", ev->output);
   ev->ipc_return++;
  }
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-shutdown-failure-timeout\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }
}

int einit_feedback_visual_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_power, einit_feedback_visual_power_event_handler);
 event_ignore (einit_event_subsystem_core, einit_feedback_visual_einit_event_handler);
 event_ignore (einit_event_subsystem_feedback, einit_feedback_visual_feedback_event_handler);
 event_ignore (einit_event_subsystem_ipc, einit_feedback_visual_ipc_event_handler);

 return 0;
}

/*
  -------- function to enable and configure this module -----------------------
 */
int einit_feedback_visual_enable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);

 einit_feedback_visual_textual_worker_thread_keep_running = 1;
 ethread_create (&feedback_textual_thread, NULL, einit_feedback_visual_textual_worker_thread, NULL);

 struct cfgnode *node = cfg_getnode ("configuration-feedback-visual-use-ansi-codes", NULL);
 if (node)
  enableansicodes = node->flag;

 if ((node = cfg_getnode ("configuration-feedback-visual-shutdown-failure-timeout", NULL)))
  shutdownfailuretimeout = node->value;

 struct cfgnode *filenode = cfg_getnode ("configuration-feedback-visual-std-io", NULL);

 if (filenode && filenode->arbattrs) {
  uint32_t i = 0;
  FILE *tmp;
  struct stat st;

  for (; filenode->arbattrs[i]; i+=2) {
   errno = 0;

   if (filenode->arbattrs[i]) {
    if (strmatch (filenode->arbattrs[i], "stdio")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "r", stdin);
      if (!tmp)
       freopen ("/dev/null", "r+", stdin);

      tmp = freopen (filenode->arbattrs[i+1], "w", stdout);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "w", stdout);
     } else {
      perror ("einit-feedback-visual-textual: opening stdio");
     }
    } else if (strmatch (filenode->arbattrs[i], "stderr")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "a", stderr);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "a", stderr);
      if (tmp)
       eprintf (stderr, "\n%i: eINIT: visualiser einit-vis-text activated.\n", (int)time(NULL));
     } else {
      perror ("einit-feedback-visual-textual: opening stderr");
      enableansicodes = 0;
     }
    } else if (strmatch (filenode->arbattrs[i], "console")) {
#ifdef LINUX
     int tfd = 0;
     errno = 0;
     if ((tfd = open (filenode->arbattrs[i+1], O_WRONLY, 0)))
      ioctl (tfd, TIOCCONS, 0);
     if (errno)
      perror (filenode->arbattrs[i+1]);

#else
     eputs ("einit-tty: console redirection support currently only available on LINUX\n", stderr);
#endif
    } else if (strmatch (filenode->arbattrs[i], "kernel-vt")) {
#ifdef LINUX
     int arg = (strtol (filenode->arbattrs[i+1], (char **)NULL, 10) << 8) | 11;
     errno = 0;

     ioctl(0, TIOCLINUX, &arg);
     if (errno)
      perror ("einit-feedback-visual-textual: redirecting kernel messages");
#else
     eputs ("einit-feedback-visual-textual: kernel message redirection support currently only available on LINUX\n", stderr);
#endif
    } else if (strmatch (filenode->arbattrs[i], "activate-vt")) {
#ifdef LINUX
     uint32_t vtn = strtol (filenode->arbattrs[i+1], (char **)NULL, 10);
     int tfd = 0;
     errno = 0;
     if ((tfd = open ("/dev/tty1", O_RDWR, 0)))
      ioctl (tfd, VT_ACTIVATE, vtn);
     if (errno)
      perror ("einit-feedback-visual-textual: activate terminal");
     if (tfd > 0) close (tfd);
#else
     eputs ("einit-feedback-visual-textual: terminal activation support currently only available on LINUX\n", stderr);
#endif
    }
   }
  }
 }

 if (enableansicodes) {
  eputs ("\e[2J\e[0;0H", stdout);
 }

 emutex_unlock (&thismodule->imutex);
 return status_ok;
}

/*
  -------- function to disable this module ------------------------------------
 */
int einit_feedback_visual_disable (void *pa, struct einit_event *status) {
 return status_ok;
}

int einit_feedback_visual_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = einit_feedback_visual_cleanup;
 irr->enable  = einit_feedback_visual_enable;
 irr->disable = einit_feedback_visual_disable;

 event_listen (einit_event_subsystem_feedback, einit_feedback_visual_feedback_event_handler);
 event_listen (einit_event_subsystem_core, einit_feedback_visual_einit_event_handler);
 event_listen (einit_event_subsystem_power, einit_feedback_visual_power_event_handler);

 event_listen (einit_event_subsystem_ipc, einit_feedback_visual_ipc_event_handler);

 return 0;
}
