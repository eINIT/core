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

const struct smodule einit_feedback_visual_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_feedback,
 .name      = "visual/text-based feedback module",
 .rid       = "einit-feedback-visual-textual",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_feedback_visual_configure
};

module_register(einit_feedback_visual_self);

#endif

char *feedback_textual_statusline = NULL;

uint32_t shutdownfailuretimeout = 10;
char enableansicodes = 1, suppress_messages = 0, suppress_status_notices = 0;

pthread_t feedback_textual_thread;
char einit_feedback_visual_textual_worker_thread_running = 0;
//     einit_feedback_visual_textual_worker_thread_keep_running = 1;

int feedback_textual_switch_progress = 0;

pthread_mutex_t feedback_textual_main_mutex = PTHREAD_MUTEX_INITIALIZER;

enum feedback_textual_commands {
 ftc_module_update,
 ftc_register_fd,
 ftc_unregister_fd
};

struct feedback_textual_command {
 enum feedback_textual_commands command;
 union {
  struct {
   struct lmodule *module;
   enum einit_module_status status;
   char *message;
   char *statusline;
   uint32_t warnings;
   time_t ctime;
  };

  struct {
   FILE *fd;
   enum einit_ipc_options fd_options;
  };
 };
 uint32_t seqid;
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
 uint32_t seqid;
};

struct feedback_stream {
 FILE *stream;
 uint32_t seqid;
 uint32_t last_seqid;
 enum einit_ipc_options options;

 uint32_t width;
 uint32_t erase_lines;
};

struct feedback_textual_command **feedback_textual_commandQ = NULL;
struct feedback_textual_module_status **feedback_textual_modules = NULL;
struct feedback_stream **feedback_streams;

pthread_mutex_t
 feedback_textual_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
// feedback_textual_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_streams_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_all_done_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t
// feedback_textual_commandQ_cond = PTHREAD_COND_INITIALIZER,
 feedback_textual_all_done_cond = PTHREAD_COND_INITIALIZER;

extern int einit_have_feedback;
char feedback_textual_allowed = 1;
char einit_feedback_visual_boot_done_switch = 0;

void feedback_textual_enable();
void *einit_feedback_visual_textual_worker_thread (void *);

void feedback_textual_queue_fd_command (enum feedback_textual_commands command, FILE *fd, enum einit_ipc_options fd_options, uint32_t seqid) {
 struct feedback_textual_command tnc;
 memset (&tnc, 0, sizeof (struct feedback_textual_command));

 tnc.command = command;

 tnc.fd = fd;
 tnc.fd_options = fd_options;
 tnc.seqid = seqid;

 emutex_lock (&feedback_textual_commandQ_mutex);

 feedback_textual_commandQ = (struct feedback_textual_command **)setadd ((void **)feedback_textual_commandQ, (void *)(&tnc), sizeof (struct feedback_textual_command));

 emutex_unlock (&feedback_textual_commandQ_mutex);

// pthread_cond_broadcast (&feedback_textual_commandQ_cond);
 if (!einit_feedback_visual_textual_worker_thread_running) {
  einit_feedback_visual_textual_worker_thread_running = 1;
  ethread_create (&feedback_textual_thread, NULL, einit_feedback_visual_textual_worker_thread, NULL);
 }
}

void feedback_textual_queue_update (struct lmodule *module, enum einit_module_status status, char *message, uint32_t seqid, time_t ctime, char *statusline, uint32_t warnings) {
 struct feedback_textual_command tnc;

 if (!einit_have_feedback && feedback_textual_allowed && (einit_quietness < 3)) {
  if (message) {
   eprintf (stderr, " > %s\n", message);
  }
  if (statusline) {
   eprintf (stderr, " s %s\n", statusline);
  }
  if (module && (module->status & status_failed) && module->module) {
   eprintf (stderr, " f %s (%s)\n", module->module->name ? module->module->name : "?", module->module->name ? module->module->name : "?");
  }
  if (module && (module->status & status_enabled) && module->module) {
   eprintf (stderr, " e %s (%s)\n", module->module->name ? module->module->name : "?", module->module->name ? module->module->name : "?");
  }
  if (module && (module->status & status_working) && module->module) {
   eprintf (stderr, " w %s (%s)\n", module->module->name ? module->module->name : "?", module->module->name ? module->module->name : "?");
  }
  fflush(stderr);
 }

 memset (&tnc, 0, sizeof (struct feedback_textual_command));

 tnc.command = ftc_module_update;

 tnc.module = module;
 tnc.statusline = statusline;
 tnc.status = status;
 tnc.seqid = seqid;
 if (message) {
  tnc.message = estrdup (message);
  strtrim (tnc.message);
 }
 tnc.ctime = ctime;
 tnc.warnings = warnings;

 emutex_lock (&feedback_textual_commandQ_mutex);

 feedback_textual_commandQ = (struct feedback_textual_command **)setadd ((void **)feedback_textual_commandQ, (void *)(&tnc), sizeof (struct feedback_textual_command));

 emutex_unlock (&feedback_textual_commandQ_mutex);

// pthread_cond_broadcast (&feedback_textual_commandQ_cond);
 if (!einit_feedback_visual_textual_worker_thread_running) {
  einit_feedback_visual_textual_worker_thread_running = 1;
  ethread_create (&feedback_textual_thread, NULL, einit_feedback_visual_textual_worker_thread, NULL);
 }
}

void feedback_textual_wait_for_commandQ_to_finish() {
 while (1) {
  char ret = 0;
  emutex_lock (&feedback_textual_commandQ_mutex);
  ret = (feedback_textual_commandQ == NULL);
  emutex_unlock (&feedback_textual_commandQ_mutex);

  if (ret || !einit_feedback_visual_textual_worker_thread_running) return;
/*  else {
   pthread_cond_broadcast (&feedback_textual_commandQ_cond);
  }*/

  emutex_lock (&feedback_textual_all_done_cond_mutex);
  int e;
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts))
   bitch (bitch_stdio, errno, "gettime failed!");

  ts.tv_sec += 1; /* max wait before re-evaluate */

  e = pthread_cond_timedwait (&feedback_textual_all_done_cond, &feedback_textual_all_done_cond_mutex, &ts);
#elif defined(DARWIN)
  struct timespec ts;
  struct timeval tv;

  gettimeofday (&tv, NULL);

  ts.tv_sec = tv.tv_sec + 1; /* max wait before re-evaluate */

  e = pthread_cond_timedwait (&feedback_textual_all_done_cond, &feedback_textual_all_done_cond_mutex, &ts);
#else
  notice (2, "warning: un-timed lock.");
  e = pthread_cond_wait (&feedback_textual_all_done_cond, &feedback_textual_all_done_cond_mutex);
#endif
  emutex_unlock (&feedback_textual_all_done_cond_mutex);

#ifdef DEBUG
  notice (12, "feedback_textual_wait_for_commandQ_to_finish(): re-evaluation");
#endif
 }

 return;
}

signed int feedback_log_sort (struct message_log *st1, struct message_log *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

#if 0
 return (st2->seqid - st1->seqid);
#else
 if (st2->seqid > st1->seqid)
  return -1;
 return 1;
#endif
}

signed int feedback_time_sort (struct feedback_textual_module_status *st1, struct feedback_textual_module_status *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->lastchange - st1->lastchange);
}

signed int feedback_name_sort (struct feedback_textual_module_status *st1, struct feedback_textual_module_status *st2) {
 if (!st1 || !st1->module || !st1->module->module || !st1->module->module->name) return -1;
 if (!st2 || !st2->module || !st2->module->module || !st2->module->module->name) return 1;

// return (st2->lastchange - st1->lastchange);
 return strcmp (st1->module->module->name, st2->module->module->name) * -1;
}

void feedback_textual_update_streams () {
 uint32_t i = 0;

 for (; feedback_streams[i]; i++) {
  uint32_t y = 0;
  uint32_t hseq = feedback_streams[i]->last_seqid;

  if (feedback_streams[i]->options & einit_ipc_output_ansi) {
   while (feedback_streams[i]->erase_lines) {
    feedback_streams[i]->erase_lines--;

    eputs ("\e[F", feedback_streams[i]->stream);
   }
   eputs ("\e[2K", feedback_streams[i]->stream);
  } else
   eputs ("\r", feedback_streams[i]->stream);


  for (y = 0; feedback_textual_modules[y]; y++) {
   if (feedback_textual_modules[y]->seqid > feedback_streams[i]->last_seqid) {
    if (!suppress_messages) {
     char *emarker = "";
     char *wmarker = "";
//     char wmbuffer[BUFFERSIZE];
     char did_display = 0;

     if (!(feedback_textual_modules[y]->module->status & (status_enabled | status_disabled | status_failed | status_working))) {
      continue;
     }

/*     if (feedback_textual_modules[y]->module->status & status_failed) {
      emarker = " \e[31m(failed)\e[0m";
     }

     if (feedback_textual_modules[y]->warnings > 1) {
      wmarker = wmbuffer;
      esprintf (wmbuffer, BUFFERSIZE, " \e[36m(%i warnings)\e[0m", feedback_textual_modules[y]->warnings);
     } else if (feedback_textual_modules[y]->warnings == 1) {
      wmarker = " \e[36m(1 warning)\e[0m";
     }*/

     if (feedback_textual_modules[y]->log) {
      uint32_t x = 0;

      for (; feedback_textual_modules[y]->log[x]; x++) {
       if (feedback_textual_modules[y]->log[x]->seqid > feedback_streams[i]->last_seqid) {
        if (!did_display) {
         eprintf (feedback_streams[i]->stream, "  \e[34m>\e[m%s%s %s: %s\n", wmarker, emarker, feedback_textual_modules[y]->module->module->name, feedback_textual_modules[y]->log[x]->message);
         did_display = 1;
        } else {
         eprintf (feedback_streams[i]->stream, "  >> %s\n", feedback_textual_modules[y]->log[x]->message);
        }
       }
      }
     }
    }

    hseq = (feedback_textual_modules[y]->seqid > hseq) ? feedback_textual_modules[y]->seqid : hseq;

    if (!suppress_status_notices && !(feedback_textual_modules[y]->module->status & status_working) && (feedback_textual_modules[y]->laststatus != feedback_textual_modules[y]->module->status)) {
     if (feedback_textual_modules[y]->module->status & status_enabled) {

      if (feedback_textual_modules[y]->module->status & status_failed) {
       eprintf (feedback_streams[i]->stream, " [ \e[31mfail\e[0m ] %s: command failed, module is enabled\n", feedback_textual_modules[y]->module->module->name);
      } else if (!(feedback_textual_modules[y]->module->module->mode & einit_feedback_job)) {
       eprintf (feedback_streams[i]->stream, " [ \e[32menab\e[0m ] %s\n", feedback_textual_modules[y]->module->module->name);
      } else {
       eprintf (feedback_streams[i]->stream, " [ \e[32m OK \e[0m ] %s\n", feedback_textual_modules[y]->module->module->name);
      }

      feedback_textual_modules[y]->laststatus = feedback_textual_modules[y]->module->status;
     }

     if (feedback_textual_modules[y]->module->status & status_disabled) {

      if (feedback_textual_modules[y]->module->status & status_failed) {
       eprintf (feedback_streams[i]->stream, " [ \e[31mfail\e[0m ] %s: command failed, module is disabled\n", feedback_textual_modules[y]->module->module->name);
      } else if (!(feedback_textual_modules[y]->module->module->mode & einit_feedback_job)) {
       eprintf (feedback_streams[i]->stream, " [ \e[32mdisa\e[0m ] %s\n", feedback_textual_modules[y]->module->module->name);
      }

      feedback_textual_modules[y]->laststatus = feedback_textual_modules[y]->module->status;
     }
    }
   }
  }


  if (feedback_textual_statusline) {
   fputs (feedback_textual_statusline, feedback_streams[i]->stream);

   if (feedback_textual_statusline[0] && feedback_textual_statusline[1] && (feedback_textual_statusline[2] == '[') && feedback_textual_modules) {
    char errorheader = 0;
    uint32_t y = 0;

    for (; feedback_textual_modules[y]; y++) {
     if (feedback_textual_modules[y]->module && feedback_textual_modules[y]->module->module && ( (feedback_textual_modules[y]->module->status & status_failed))) {
      if (!errorheader) {
       eputs ("\e[31m >> WARNING: The following modules are tagged as FAILED:\e[0m\n", feedback_streams[i]->stream);

       errorheader = 1;
      }

      eprintf (feedback_streams[i]->stream, "   \e[31m*\e[0m %s (%s)\n", feedback_textual_modules[y]->module->module->name, feedback_textual_modules[y]->module->module->rid);
     }
    }
   }
  }

  fflush (feedback_streams[i]->stream);

/* display all workers: */
//  eputs ("working on:", feedback_streams[i]->stream);

  uint32_t used_width = 0;
  char started = 0;

  for (y = 0; feedback_textual_modules[y]; y++) {
   if (feedback_textual_modules[y]->module && feedback_textual_modules[y]->module->module &&
       (feedback_textual_modules[y]->module->status & status_working)) {
    if (!started) {
     started = 1;
     used_width += 10 + strlen (feedback_textual_modules[y]->module->module->rid);

     eprintf (feedback_streams[i]->stream, "[ %.3d%% || %s", feedback_textual_switch_progress, feedback_textual_modules[y]->module->module->rid);
    } else {
//     eprintf (feedback_streams[i]->stream, " | %s (%i)", feedback_textual_modules[y]->module->module->rid, feedback_textual_modules[y]->warnings);
     used_width += 3 + strlen (feedback_textual_modules[y]->module->module->rid);

     if (used_width > (feedback_streams[i]->width - 8)) {
      eputs (" | ++", feedback_streams[i]->stream);

      used_width -= strlen (feedback_textual_modules[y]->module->module->rid) -2;

      break;
     } else {
      eprintf (feedback_streams[i]->stream, " | %s", feedback_textual_modules[y]->module->module->rid);
     }
    }
   }
  }

  if (started) {
   while (used_width < (feedback_streams[i]->width -2)) {
    eputs (" ", feedback_streams[i]->stream);
    used_width++;
   }
   eputs (" ]", feedback_streams[i]->stream);
  }

  feedback_streams[i]->last_seqid = hseq;

  if (feedback_streams[i]->options & einit_ipc_output_ansi) {
   eputs ("\n", feedback_streams[i]->stream);
   feedback_streams[i]->erase_lines++;
  }

  fflush (feedback_streams[i]->stream);
 }

 if (feedback_textual_statusline) {
  feedback_textual_statusline = NULL;
 }
}

void feedback_textual_update_screen () {
 emutex_lock (&feedback_textual_modules_mutex);

 emutex_lock (&feedback_textual_streams_mutex);
 if (feedback_streams && feedback_textual_modules) {
  feedback_textual_update_streams ();
 }
 emutex_unlock (&feedback_textual_streams_mutex);

 emutex_unlock (&feedback_textual_modules_mutex);
}

void feedback_textual_update_module (struct lmodule *module, time_t ctime, uint32_t seqid, char *message, uint32_t warnings) {
 char create_module = 1;

 emutex_lock (&feedback_textual_modules_mutex);

 if (feedback_textual_modules) {
  uint32_t i = 0;

  for (; feedback_textual_modules[i]; i++) {
   if (feedback_textual_modules[i]->module == module) {
    create_module = 0;

    if (feedback_textual_modules[i]->lastchange <= ctime) {
     feedback_textual_modules[i]->lastchange = ctime;
    }

    if (feedback_textual_modules[i]->seqid <= seqid) {
     feedback_textual_modules[i]->seqid = seqid;
    }

    if (message) {
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
    } if (!warnings)
     feedback_textual_modules[i]->warnings = 0;
   }
  }
 }

 if (create_module) {
  struct feedback_textual_module_status nm;

  memset (&nm, 0, sizeof (struct feedback_textual_module_status));

  nm.module = module;
  nm.lastchange = ctime;

  nm.warnings = warnings;
  nm.seqid = seqid;

  if (message) {
   struct message_log ne = {
    .seqid = seqid,
    .message = message
   };

   nm.log = (struct message_log **)setadd (NULL, &ne, sizeof (struct message_log));
  }

  feedback_textual_modules = (struct feedback_textual_module_status **)setadd ((void **)feedback_textual_modules, &nm, sizeof (struct feedback_textual_module_status));

  setsort ((void **)feedback_textual_modules, 0, (signed int(*)(const void *, const void*))feedback_name_sort);
 }

 emutex_unlock (&feedback_textual_modules_mutex);
}

void feedback_textual_process_command (struct feedback_textual_command *command) {
 if (command->statusline) {
  feedback_textual_statusline = command->statusline;

  feedback_textual_update_screen ();

  feedback_textual_statusline = NULL;
  efree (command->statusline);
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
 einit_have_feedback = 1;
 char cs = 0;

/* while (einit_feedback_visual_textual_worker_thread_keep_running) {*/
 rerun:

  while (feedback_textual_commandQ) {
   struct feedback_textual_command *command = NULL;

   cs++;

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
    switch (command->command) {
     case ftc_module_update:
      feedback_textual_process_command (command);

      efree (command);

      if (cs >= 1) {
       feedback_textual_update_screen ();
       cs = 0;
      }
      break;
     case ftc_register_fd:
      if (command->fd) {
       struct feedback_stream st;
       memset (&st, 0, sizeof (struct feedback_stream));

       st.stream = command->fd;
       st.options = command->fd_options;
       st.seqid = command->seqid;
       st.last_seqid = st.seqid;
       st.width = 80;
       st.erase_lines = 1;

       if (st.options & einit_ipc_output_ansi)
        eputs ("working on your request...\n", st.stream);

       emutex_lock (&feedback_textual_streams_mutex);
       feedback_streams = (struct feedback_stream **)setadd ((void **)feedback_streams, (void *)&st, sizeof (struct feedback_stream));
       emutex_unlock (&feedback_textual_streams_mutex);

       feedback_textual_update_screen ();
       cs = 0;
      }

      break;
     case ftc_unregister_fd:
      feedback_textual_update_screen ();
      cs = 0;

      emutex_lock (&feedback_textual_streams_mutex);
      repeat_unregister_fd:
      if (feedback_streams)  {
       uint32_t si = 0;

       for (; feedback_streams[si]; si++) {
        if (feedback_streams[si]->stream == command->fd) {
         feedback_streams = (struct feedback_stream **)setdel ((void **)feedback_streams, (void *)feedback_streams[si]);

         goto repeat_unregister_fd;
        }
       }
      }
      emutex_unlock (&feedback_textual_streams_mutex);

      break;
    }
   }
  }

//  if (cs)
  feedback_textual_update_screen ();

  pthread_cond_broadcast (&feedback_textual_all_done_cond);

/*  emutex_lock (&feedback_textual_commandQ_cond_mutex);
  pthread_cond_wait (&feedback_textual_commandQ_cond, &feedback_textual_commandQ_cond_mutex);
  emutex_unlock (&feedback_textual_commandQ_cond_mutex);*/
/* } */
 usleep (100);
 if (feedback_textual_commandQ) goto rerun;

 einit_feedback_visual_textual_worker_thread_running = 0;

 return NULL;
}

void einit_feedback_visual_feedback_event_handler_broken_services (struct einit_event *ev) {
 char *tmp = set2str (' ', (const char **)ev->set);
 char tmp2[BUFFERSIZE];

 if (tmp) {
  eprintf (stderr, ev->set[1] ? " >> broken services: %s\n" : " >> broken service: %s\n", tmp);

  esprintf (tmp2, BUFFERSIZE, "\e[31m ** BROKEN SERVICES:\e[0m %s\n", tmp);
  feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp2), 0);

  efree (tmp);
 }
}

void einit_feedback_visual_feedback_event_handler_unresolved_services (struct einit_event *ev) {
 char *tmp = set2str (' ', (const char **)ev->set);
 char tmp2[BUFFERSIZE];

 if (tmp) {
  eprintf (stderr, ev->set[1] ? " >> unresolved services: %s\n" : " >> unresolved service: %s\n", tmp);

  esprintf (tmp2, BUFFERSIZE, "\e[31m ** UNRESOLVED SERVICES:\e[0m %s\n", tmp);
  feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp2), 0);
 
  efree (tmp);
 }
}

void einit_feedback_visual_feedback_event_handler_module_status (struct einit_event *ev) {
 feedback_textual_queue_update (ev->module, ev->status, ev->string, ev->seqid, ev->timestamp, NULL, ev->flag);
}

void einit_feedback_visual_feedback_event_handler_register_fd (struct einit_event *ev) {
 feedback_textual_queue_fd_command (ftc_register_fd, ev->output, ev->ipc_options, ev->seqid);
}

void einit_feedback_visual_feedback_event_handler_unregister_fd(struct einit_event *ev) {
 char have_fd = 0;
 do {
  have_fd = 0;
  fflush (ev->output);

  feedback_textual_queue_fd_command (ftc_unregister_fd, ev->output, ev->ipc_options, ev->seqid);

  feedback_textual_wait_for_commandQ_to_finish();

  emutex_lock (&feedback_textual_streams_mutex);
  if (feedback_streams) {
   uint32_t si = 0;

   for (; feedback_streams[si]; si++) {
    if (feedback_streams[si]->stream == ev->output) {
     have_fd = 1;
     break;
    }
   }
  }
  emutex_unlock (&feedback_textual_streams_mutex);
 } while (have_fd);
}

void einit_feedback_visual_einit_event_handler_service_update (struct einit_event *ev) {
 feedback_textual_queue_update (ev->module, ev->status, NULL, ev->seqid, ev->timestamp, NULL, ev->flag);
}

void einit_feedback_visual_einit_event_handler_mode_switching (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, " \e[34m**\e[0m \e[34mswitching to mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

 feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);
}

void einit_feedback_visual_einit_event_handler_mode_switch_done (struct einit_event *ev) {
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, " \e[32m**\e[0m \e[34mswitch complete: mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

 feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);

#ifdef LINUX
 if ((!einit_feedback_visual_boot_done_switch || strmatch (((struct cfgnode *)ev->para)->id, "default")) && !mod_service_is_provided ("displaymanager") && !mod_service_is_provided ("x11")  && !mod_service_is_provided ("xorg") && !mod_service_is_provided ("xdm") && !mod_service_is_provided ("slim") && !mod_service_is_provided ("gdm") && !mod_service_is_provided ("kdm") && !mod_service_is_provided ("entrance") && !mod_service_is_provided ("entranced")) {
  einit_feedback_visual_boot_done_switch = 1;
  char *new_vt = cfg_getstring ("configuration-feedback-visual-std-io/boot-done-chvt", NULL);

  if (new_vt) {
   int arg = (strtol (new_vt, (char **)NULL, 10) << 8) | 11;
   errno = 0;

   ioctl(0, TIOCLINUX, &arg);
   if (errno)
    perror ("einit-feedback-visual-textual: redirecting kernel messages");
  }
 }
#endif
}

/*
  -------- power event-handler -------------------------------------------------
 */
void einit_feedback_visual_power_event_handler(struct einit_event *ev) {
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
     eprintf (stdout, "\e[0m[ \e[31m%4.4i\e[0m ] \e[31mWarning: Errors occured while shutting down, waiting...\e[0m\n", c);
    } else {
     eprintf (stdout, "[ %4.4i ] Warning: Errors occured while shutting down, waiting...\n", c);
    }

    c -= 1 - sleep (1);
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

/*
  -------- function to enable and configure this module -----------------------
 */
void feedback_textual_enable() {
 emutex_lock (&feedback_textual_main_mutex);
 struct cfgnode *node = cfg_getnode ("configuration-feedback-textual", NULL);
 if (node && !node->flag) { /* node needs to exist and explicitly say 'no' to disable this module */
  feedback_textual_allowed = 0;
  emutex_unlock (&feedback_textual_main_mutex);
  return;
 }

 notice (3, "enabling textual feedback");

 if ((node = cfg_getnode ("configuration-feedback-visual-use-ansi-codes", NULL)))
  enableansicodes = node->flag;

 if (einit_quietness < 1) {
  node = cfg_getnode ("configuration-feedback-visual-suppress-messages", NULL);
  if (node)
   suppress_messages = node->flag;
 } else {
  suppress_messages = 1;
 }

 if (einit_quietness < 2) {
  node = cfg_getnode ("configuration-feedback-visual-suppress-status-notices", NULL);
  if (node)
   suppress_status_notices = node->flag;
 } else {
  suppress_status_notices = 1;
 }

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
     if ((tfd = open (filenode->arbattrs[i+1], O_WRONLY, 0))) {
      fcntl (tfd, F_SETFD, FD_CLOEXEC);
      ioctl (tfd, TIOCCONS, 0);
     }
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
     if ((tfd = open ("/dev/tty1", O_RDWR, 0))) {
      fcntl (tfd, F_SETFD, FD_CLOEXEC);
      ioctl (tfd, VT_ACTIVATE, vtn);
     }
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
  if (einit_quietness < 3) {
   eputs ("\e[2J\e[0;0H eINIT " EINIT_VERSION_LITERAL "\n", stdout);
  } else
   eputs ("\e[2J\e[0;0H\n", stdout);
 }

 /* register our default output feedback-stream */
 struct feedback_stream st;
 memset (&st, 0, sizeof (struct feedback_stream));

 st.stream = stdout;
 st.options = enableansicodes ? einit_ipc_output_ansi : 0;
 st.seqid = 0;
 st.last_seqid = 0;
 st.erase_lines = 1;

#ifdef TIOCGWINSZ
 struct winsize size;

 if (!ioctl (STDOUT_FILENO, TIOCGWINSZ, &size)) {
  st.width = size.ws_col;
 } else {
  st.width = 80;
 }

#else
 st.width = 80;
#endif

 if (einit_quietness < 3) {
  uint32_t r = 0;
  for (; r < st.width; r++) {
   fputs ("#", stdout);
  }

  fputs ("\n\n", stdout);
 }

 if (einit_quietness < 3) {
  emutex_lock (&feedback_textual_streams_mutex);
  feedback_streams = (struct feedback_stream **)setadd ((void **)feedback_streams, (void *)&st, sizeof (struct feedback_stream));
  emutex_unlock (&feedback_textual_streams_mutex);
 }

 emutex_unlock (&feedback_textual_main_mutex);

// einit_feedback_visual_textual_worker_thread_keep_running = 1;
 ethread_create (&feedback_textual_thread, NULL, einit_feedback_visual_textual_worker_thread, NULL);
}

void einit_feedback_textual_feedback_switch_progress_handler (struct einit_event *ev) {
 feedback_textual_switch_progress = ev->integer;
}

int einit_feedback_visual_cleanup (struct lmodule *this) {
 event_ignore (einit_boot_devices_available, feedback_textual_enable);
 event_ignore (einit_ipc_request_generic, einit_feedback_visual_ipc_event_handler);
 event_ignore (einit_power_down_imminent, einit_feedback_visual_power_event_handler);
 event_ignore (einit_power_reset_imminent, einit_feedback_visual_power_event_handler);
 event_ignore (einit_feedback_broken_services, einit_feedback_visual_feedback_event_handler_broken_services);
 event_ignore (einit_feedback_unresolved_services, einit_feedback_visual_feedback_event_handler_unresolved_services);
 event_ignore (einit_feedback_module_status, einit_feedback_visual_feedback_event_handler_module_status);
 event_ignore (einit_feedback_register_fd, einit_feedback_visual_feedback_event_handler_register_fd);
 event_ignore (einit_feedback_unregister_fd, einit_feedback_visual_feedback_event_handler_unregister_fd);
 event_ignore (einit_core_service_update, einit_feedback_visual_einit_event_handler_service_update);
 event_ignore (einit_core_mode_switching, einit_feedback_visual_einit_event_handler_mode_switching);
 event_ignore (einit_core_mode_switch_done, einit_feedback_visual_einit_event_handler_mode_switch_done);
 event_ignore (einit_feedback_switch_progress, einit_feedback_textual_feedback_switch_progress_handler);

 return 0;
}

int einit_feedback_visual_configure (struct lmodule *irr) {
 module_init (irr);

 struct cfgnode *node = cfg_getnode ("configuration-feedback-textual", NULL);
 if (node && !node->flag) { /* node needs to exist and explicitly say 'no' to disable this module */
  return status_configure_failed | status_not_in_use;
 }

 irr->cleanup = einit_feedback_visual_cleanup;

 event_listen (einit_boot_devices_available, feedback_textual_enable);
 event_listen (einit_ipc_request_generic, einit_feedback_visual_ipc_event_handler);
 event_listen (einit_power_down_imminent, einit_feedback_visual_power_event_handler);
 event_listen (einit_power_reset_imminent, einit_feedback_visual_power_event_handler);
 event_listen (einit_feedback_broken_services, einit_feedback_visual_feedback_event_handler_broken_services);
 event_listen (einit_feedback_unresolved_services, einit_feedback_visual_feedback_event_handler_unresolved_services);
 event_listen (einit_feedback_module_status, einit_feedback_visual_feedback_event_handler_module_status);
 event_listen (einit_feedback_register_fd, einit_feedback_visual_feedback_event_handler_register_fd);
 event_listen (einit_feedback_unregister_fd, einit_feedback_visual_feedback_event_handler_unregister_fd);
 event_listen (einit_core_service_update, einit_feedback_visual_einit_event_handler_service_update);
 event_listen (einit_core_mode_switching, einit_feedback_visual_einit_event_handler_mode_switching);
 event_listen (einit_core_mode_switch_done, einit_feedback_visual_einit_event_handler_mode_switch_done);
 event_listen (einit_feedback_switch_progress, einit_feedback_textual_feedback_switch_progress_handler);

 return 0;
}
