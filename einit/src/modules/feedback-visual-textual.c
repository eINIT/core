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
 .rid       = "einit-feedback-visual-textual",
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
};

struct feedback_textual_command **feedback_textual_commandQ = NULL;
struct feedback_textual_module_status **feedback_textual_modules = NULL;
struct feedback_stream **feedback_streams;

pthread_mutex_t
 feedback_textual_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_streams_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_all_done_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
 pthread_cond_t
 feedback_textual_commandQ_cond = PTHREAD_COND_INITIALIZER,
 feedback_textual_all_done_cond = PTHREAD_COND_INITIALIZER;

char *feedback_textual_statusline = "[ \e[31m....\e[0m ] \e[34minitialising\e[0m\e[0K\n";

void *feedback_textual_io_handler_thread (void *irr) {
 int rchar;

 while (1) {
  rchar = fgetc(stdin);
  if (rchar == EOF) {
  }

  eprintf (stdout, "read character: %c", rchar);
 }

 return irr;
}

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

 pthread_cond_broadcast (&feedback_textual_commandQ_cond);
}

void feedback_textual_queue_update (struct lmodule *module, enum einit_module_status status, char *message, uint32_t seqid, time_t ctime, char *statusline, uint32_t warnings) {
 struct feedback_textual_command tnc;
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

 pthread_cond_broadcast (&feedback_textual_commandQ_cond);
}

void feedback_textual_wait_for_commandQ_to_finish() {
 while (1) {
  char ret = 0;
  emutex_lock (&feedback_textual_commandQ_mutex);
  ret = (feedback_textual_commandQ == NULL);
  emutex_unlock (&feedback_textual_commandQ_mutex);

  if (ret) return;
  else {
   pthread_cond_broadcast (&feedback_textual_commandQ_cond);
  }

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

  notice (12, "feedback_textual_wait_for_commandQ_to_finish(): re-evaluation");
 }

 return;
}

pthread_t feedback_textual_thread;
char einit_feedback_visual_textual_worker_thread_running = 0,
     einit_feedback_visual_textual_worker_thread_keep_running = 1;

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
  if (st->module->status & status_deferred) {
   statuscode[4] = 's';
  }

  if (st->module->status & status_failed) {
   statuscode[0] = '!';
   statuscode[5] = '!';
  }
 }

 if (st->log) {
/*  uint32_t y = 0;

  for (; st->log[y]; y++) ;

  if (y != 0) {
   y--;
   eprintf (stdout, "%s %s (%s): %s\n", statuscode, name, rid, st->log[y]->message);
  }*/
  if (st->log[0])
   eprintf (stdout, "%s %s (%s): %s\n", statuscode, name, rid, st->log[0]->message);
 } else {
  eprintf (stdout, "%s %s (%s)\n", statuscode, name, rid);
 }
}

void feedback_process_textual_ansi(struct feedback_textual_module_status *st) {
 char *defcode = "0";
 char *name = (st->module->module && st->module->module->name) ? st->module->module->name : "no name";

 char *wmarker = "";
 char *emarker = "";
 char *rmarker = " ";
 char *status = "<FIXME!> \e[31m----\e[0m";
 char wmbuffer[BUFFERSIZE];
 char embuffer[BUFFERSIZE];
 char stbuffer[BUFFERSIZE];
 char do_details = 0;

 if (st->module->status & status_working) {
  defcode = "30;1";
 } if ((st->module->status & status_disabled) || (st->module->status == status_idle)) {
  defcode = "36";
 }

 if (st->module->status == status_idle) {
  status = stbuffer;
  esprintf (stbuffer, BUFFERSIZE, "\e[31midle\e[%sm", defcode);
 } else {
  if (st->module->status & status_enabled) {
   status = stbuffer;
   esprintf (stbuffer, BUFFERSIZE, "\e[32menab\e[%sm", defcode);
  }
  if (st->module->status & status_disabled) {
   status = stbuffer;
   esprintf (stbuffer, BUFFERSIZE, "\e[33mdisa\e[%sm", defcode);
  }
  if (st->module->status & status_deferred) {
   status = stbuffer;
   esprintf (stbuffer, BUFFERSIZE, "\e[33mschd\e[%sm", defcode);
  }
  if (st->module->status & status_working) {
   status = stbuffer;
   esprintf (stbuffer, BUFFERSIZE, "\e[31m....\e[%sm", defcode);
  }

  if (st->module->status & status_failed) {
   esprintf (embuffer, BUFFERSIZE, " \e[31m(failed)\e[%sm", defcode);
   emarker = embuffer;
   do_details = 1;
  }
 }

 if (st->warnings > 1) {
  wmarker = wmbuffer;
  esprintf (wmbuffer, BUFFERSIZE, "\e[36m(%i warnings)\e[%sm ", st->warnings, defcode);
  do_details = 1;
 } else if (st->warnings == 1) {
  wmarker = wmbuffer;
  esprintf (wmbuffer, BUFFERSIZE, "\e[36m(one warning)\e[%sm ", defcode);
  do_details = 1;
 }

 if (st->log) {
  uint32_t y = 0;

  for (; st->log[y]; y++) ;

  if (do_details && (y > 1)) {
   eprintf (stdout, "\e[%sm%s[ %s ]%s %s%s; messages:\e[0m\e[0K\n", defcode, rmarker, status, emarker, wmarker, name);

   y = 0;
   for (; st->log[y] && y < 3; y++)
    eprintf (stdout, "\e[%sm  \e[37m*\e[0m %i: %s\e[0m\e[0K\n", defcode, st->log[y]->seqid, st->log[y]->message);
  } else if (y != 0) {
   y--;

   eprintf (stdout, "\e[%sm%s[ %s ]%s %s%s: %s\e[0m\e[0K\n", defcode, rmarker, status, emarker, wmarker, name, st->log[0]->message);
  } else {
   eprintf (stdout, "\e[%sm%s[ %s ]%s %s%s\e[0m\e[0K\n", defcode, rmarker, status, emarker, wmarker, name);
  }
 } else {
  eprintf (stdout, "\e[%sm%s[ %s ]%s %s%s\e[0m\e[0K\n", defcode, rmarker, status, emarker, wmarker, name);
 }
}

void feedback_textual_update_streams () {
 uint32_t i = 0;

 for (; feedback_streams[i]; i++) {
  uint32_t y = 0;
  uint32_t hseq = feedback_streams[i]->last_seqid;

  if (feedback_streams[i]->options & einit_ipc_output_ansi)
   eputs ("\e[F\e[2K", feedback_streams[i]->stream);
  else
   eputs ("\r", feedback_streams[i]->stream);

  for (y = 0; feedback_textual_modules[y]; y++) {
   if (feedback_textual_modules[y]->seqid > feedback_streams[i]->last_seqid) {
    char *status = "";
    char *emarker = "";
    char *wmarker = "";
    char wmbuffer[BUFFERSIZE];
    char did_display = 0;

    if (feedback_textual_modules[y]->module->status == status_idle) {
     status = "\e[31midle\e[0m";
    } else {
     if (feedback_textual_modules[y]->module->status & status_enabled) {
      status = "\e[32menab\e[0m";
     }
     if (feedback_textual_modules[y]->module->status & status_disabled) {
      status = "\e[33mdisa\e[0m";
     }
     if (feedback_textual_modules[y]->module->status & status_working) {
      status = "\e[31m....\e[0m";
     }

     if (feedback_textual_modules[y]->module->status & status_failed) {
      emarker = " \e[31m(failed)\e[0m";
     }
    }

    if (feedback_textual_modules[y]->warnings > 1) {
     wmarker = wmbuffer;
     esprintf (wmbuffer, BUFFERSIZE, "\e[36m(%i warnings)\e[0m ", feedback_textual_modules[y]->warnings);
    } else if (feedback_textual_modules[y]->warnings == 1) {
     wmarker = "\e[36m(1 warning)\e[0m ";
    }

    if (feedback_textual_modules[y]->log) {
     uint32_t x = 0;

     for (; feedback_textual_modules[y]->log[x]; x++) {
      if (feedback_textual_modules[y]->log[x]->seqid > feedback_streams[i]->last_seqid) {
       if (!did_display) {
        eprintf (feedback_streams[i]->stream, "  [ %s ] %s%s %s: %s\n", status, wmarker, emarker, feedback_textual_modules[y]->module->module->name, feedback_textual_modules[y]->log[x]->message);
        did_display = 1;
       } else {
        eprintf (feedback_streams[i]->stream, " >> %s\n", feedback_textual_modules[y]->log[x]->message);
       }
      }
     }
    }

    hseq = (feedback_textual_modules[y]->seqid > hseq) ? feedback_textual_modules[y]->seqid : hseq;

    if (!(feedback_textual_modules[y]->module->status & status_working)) {
     if (!did_display) {
      eprintf (feedback_streams[i]->stream, "  [ %s ] %s%s %s\n", status, wmarker, emarker, feedback_textual_modules[y]->module->module->name);
     }
    }
   }
  }

  fflush (feedback_streams[i]->stream);

/* display all workers: */
//  eputs ("working on:", feedback_streams[i]->stream);

  for (y = 0; feedback_textual_modules[y]; y++) {
   if (feedback_textual_modules[y]->module && feedback_textual_modules[y]->module->module &&
       (feedback_textual_modules[y]->module->status & status_working)) {
    eprintf (feedback_streams[i]->stream, "[ %s (%i) ]", feedback_textual_modules[y]->module->module->rid, feedback_textual_modules[y]->warnings);
   }
  }

  feedback_streams[i]->last_seqid = hseq;

  if (feedback_streams[i]->options & einit_ipc_output_ansi)
   eputs ("\n", feedback_streams[i]->stream);

  fflush (feedback_streams[i]->stream);
 }
}

void feedback_textual_update_screen () {
 emutex_lock (&feedback_textual_modules_mutex);

 if (enableansicodes) {
  eputs ("\e[0;0H\e[47;30m \e[34;1m[\e[30m Misc \e[34m]\e[90;22m    Network    Mountpoints\e[K\e[0m\n\n ", stdout);

  eputs (feedback_textual_statusline, stdout);
 } else
  eputs ("\n", stdout);

 if (feedback_textual_modules) {
  uint32_t i = 0;

/*  setsort ((void **)feedback_textual_modules, 0, (signed int(*)(const void *, const void*))feedback_time_sort);*/

  time_t tt = time(NULL) - 10;

  for (; feedback_textual_modules[i]; i++) {
   if (feedback_textual_modules[i]->module && ((feedback_textual_modules[i]->lastchange) < tt) &&
       !(feedback_textual_modules[i]->module->status & (status_enabled | status_working))) {
    continue;
   } else {
    if (enableansicodes)
     feedback_process_textual_ansi(feedback_textual_modules[i]);
    else
     feedback_process_textual_noansi(feedback_textual_modules[i]);
   }
  }
 }

 if (enableansicodes) {
  eputs ("\e[0J\n", stdout);
 }

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
  char cs = 0;
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

      free (command);

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
  feedback_textual_queue_update (ev->module, ev->status, ev->string, ev->seqid, ev->timestamp, NULL, ev->flag);
 } else if (ev->type == einit_feedback_register_fd) {
  feedback_textual_queue_fd_command (ftc_register_fd, ev->output, ev->ipc_options, ev->seqid);
 } else if (ev->type == einit_feedback_unregister_fd) {
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
}

void einit_feedback_visual_einit_event_handler(struct einit_event *ev) {
 if (ev->type == einit_core_service_update) {
  feedback_textual_queue_update (ev->module, ev->status, NULL, ev->seqid, ev->timestamp, NULL, ev->flag);
 } else if (ev->type == einit_core_mode_switching) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "[ \e[31m....\e[0m ] \e[34mswitching to mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

  feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);
 } else if (ev->type == einit_core_mode_switch_done) {
  char tmp[BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "[ \e[32mdone\e[0m ] \e[34mswitch complete: mode %s. (boot+%is)\e[0m\e[0K\n", ((struct cfgnode *)ev->para)->id, (int)(time(NULL) - boottime));

  feedback_textual_queue_update (NULL, status_working, NULL, ev->seqid, ev->timestamp, estrdup (tmp), 0);
 }
}

/*
  -------- power event-handler -------------------------------------------------
 */
void einit_feedback_visual_power_event_handler(struct einit_event *ev) {
// struct cfgnode *n;

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
     eprintf (stdout, "\e[0m[ \e[31m%4.4i\e[0m ] \e[31mWarning: Errors occured while shutting down, waiting...\e[0m\n", c);
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

#if 0
 pthread_t th;
 ethread_create (&th, &thread_attribute_detached, feedback_textual_io_handler_thread, NULL);
#endif

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
