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
char show_progress = 1;
char enableansicodes = 1;

#if 1
FILE *vofile = NULL;

struct planref {
 struct mloadplan *plan;
 time_t startedat;
 uint32_t max_changes;
 uint32_t min_changes;
};

struct nstring {
 uint32_t seqid;
 char *string;
};

struct feedback_fd {
 FILE *fd;
 unsigned char options;
};

struct mstat {
 struct lmodule *mod;
 uint32_t seqid, lines, task;
 time_t lastupdate;
 char errors, display;
 struct nstring **textbuffer;
};

void einit_feedback_visual_feedback_event_handler(struct einit_event *);
void einit_feedback_visual_einit_event_handler(struct einit_event *);
void einit_feedback_visual_power_event_handler(struct einit_event *);

void update_screen_neat (struct einit_event *, struct mstat *);
void update_screen_noansi (struct einit_event *, struct mstat *);
void update_screen_ansi (struct einit_event *, struct mstat *);

int nstringsetsort (struct nstring *, struct nstring *);
unsigned char broadcast_message (char *, char *);

struct planref **plans = NULL;
struct mstat **modules = NULL;
struct feedback_fd **feedback_fds = NULL;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modulesmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t feedback_fdsmutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t statusbarlines = 2;

void einit_feedback_visual_ipc_event_handler (struct einit_event *ev) {
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
  if (!cfg_getnode("configuration-feedback-visual-calculate-switch-status", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-calculate-switch-status\" not found.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }
}

int einit_feedback_visual_cleanup (struct lmodule *this) {
 emutex_lock (&thismodule->imutex);
 event_ignore (einit_event_subsystem_ipc, einit_feedback_visual_ipc_event_handler);
 if (plans) {
  emutex_lock (&plansmutex);
  free (plans);
  plans = NULL;
  emutex_unlock (&plansmutex);
 }
 if (modules) {
  uint32_t y = 0, x = 0;
  emutex_lock (&modulesmutex);

  for (y = 0; modules[y]; y++) {
   if (modules[y]->textbuffer) {
    for (x = 0; modules[y]->textbuffer[x]; x++) {
     if (modules[y]->textbuffer[x]->string) {
      free (modules[y]->textbuffer[x]->string);
     }
    }
    free (modules[y]->textbuffer);
   }
  }

  free (modules);
  modules = NULL;
  emutex_unlock (&modulesmutex);
 }

 emutex_unlock (&thismodule->imutex);
 return 0;
}

/*
  -------- feedback event-handler ---------------------------------------------
 */
void einit_feedback_visual_feedback_event_handler(struct einit_event *ev) {
 uint32_t line = 0;

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
 } else if (ev->type == einit_feedback_register_fd) {
  emutex_lock (&thismodule->imutex);

  struct feedback_fd *newfd = emalloc (sizeof (struct feedback_fd));

  newfd->fd = ev->para;
  newfd->options = ev->flag;

  emutex_lock (&feedback_fdsmutex);

  feedback_fds = (struct feedback_fd **)setadd ((void **)feedback_fds, (void *)newfd, SET_NOALLOC);
  emutex_unlock (&feedback_fdsmutex);

  emutex_unlock (&thismodule->imutex);
 } else if (ev->type == einit_feedback_unregister_fd) {
  emutex_lock (&thismodule->imutex);

  uint32_t i = 0;
   emutex_lock (&feedback_fdsmutex);

   if (feedback_fds) for (; feedback_fds[i]; i++) {
    struct feedback_fd *newfd = (struct feedback_fd *)feedback_fds[i];
    if (newfd->fd == ev->para) {
     feedback_fds = (struct feedback_fd **)setdel ((void **)feedback_fds, (void *)newfd);
     free (newfd);
     break;
    }
   }

   emutex_unlock (&feedback_fdsmutex);

  emutex_unlock (&thismodule->imutex);
 } else if (ev->type == einit_feedback_plan_status) {
  emutex_lock (&thismodule->imutex);

  int i = 0;
  struct planref plan, *cul = NULL;
  uint32_t startedat = 0;
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    if (enableansicodes) {
     eprintf (stdout, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
    } else {
     eprintf (stdout, "switching to mode %s.\n", (cmode && cmode->id) ? cmode->id : "unknown");
    }

    if (vofile) {
     eprintf (vofile, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
    }
    emutex_lock (&plansmutex);
     plan.plan = (struct mloadplan *)ev->para;
     plan.startedat = time (NULL);
     plan.max_changes = 0;
     plan.min_changes = 0;
     plans = (struct planref **)setadd ((void **)plans, (void *)&plan, sizeof (struct planref));
    emutex_unlock (&plansmutex);
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
    if (enableansicodes) {
     emutex_lock (&modulesmutex);
     line = setcount ((const void **)modules) +1;
     emutex_unlock (&modulesmutex);
    }

    if (!plans) {
     if (enableansicodes) {
      eprintf (stdout, "\e[0;0H[ \e[33m%4.4i\e[0m ] \e[34mswitch complete.\e[0m\e[0K\n", (int)(time(NULL) - boottime));
     }
     break;
    }
    emutex_lock (&plansmutex);
     for (; plans[i]; i++)
      if (plans[i]->plan == (struct mloadplan *)ev->para) {
       cul = plans[i];
       startedat = plans[i]->startedat;
       break;
      }
     if (cul)
      plans = (struct planref **)setdel ((void **)plans, (void *)cul);
    emutex_unlock (&plansmutex);
    if (enableansicodes) {
     eprintf (stdout, "\e[0;0H[ \e[33m%4.4i\e[0m ] \e[34mnew mode \"%s\" is now in effect (BT+%i).\e[0m\e[0K\n", (int)(time(NULL) - startedat), (amode && amode->id) ? amode->id : "unknown", (int)(time(NULL) - boottime));
    } else {
     eprintf (stdout, "new mode %s is now in effect (BT+%i).\n", (amode && amode->id) ? amode->id : "unknown", (int)(time(NULL) - boottime));
    }

    if (vofile)
     eprintf (vofile, "\e[0;0H[ \e[33m%4.4i\e[0m ] \e[34mnew mode \"%s\" is now in effect (BT+%i).\e[0m\e[0K\n", (int)(time(NULL) - startedat), (amode && amode->id) ? amode->id : "unknown", (int)(time(NULL) - boottime));
    break;
  }

  emutex_unlock (&thismodule->imutex);
 } else if (ev->type == einit_feedback_module_status) {
  emutex_lock (&thismodule->imutex);

  struct mstat *mst = NULL;
  uint32_t i = 0;

  emutex_lock (&modulesmutex);

  if (modules) {
   for (i = 0; modules[i]; i++)
    if (((struct mstat *)(modules[i]))->mod == ev->para) {
     if (ev->string) {
      struct nstring tm = {
       .seqid = ev->seqid,
       .string = estrdup (ev->string)
      };

      ((struct mstat *)(modules[i]))->textbuffer = (struct nstring **)setadd ((void **)(((struct mstat *)(modules[i]))->textbuffer), (void *)&tm, sizeof (struct nstring));

      setsort ((void **)(((struct mstat *)(modules[i]))->textbuffer), 0, (signed int(*)(const void *, const void*))nstringsetsort);
     }

     if (((struct mstat *)(modules[i]))->seqid > ev->seqid) {
/* discard older messages that came in after newer ones (happens frequently in multi-threaded situations) */
      emutex_unlock (&modulesmutex);
      emutex_unlock (&thismodule->imutex);
      return;
     } else {
      ((struct mstat *)(modules[i]))->seqid = ev->seqid;
     }
     break;
    }
  }
  if (!modules || !modules [i]) {
   struct mstat m; memset (&m, 0, sizeof (struct mstat));
   m.mod = ev->para;
   m.seqid = ev->seqid;
   m.errors = 0;
   m.lines = 1;
   m.display = 1;
   if (ev->string) {
    struct nstring tm = {
     .seqid = ev->seqid,
     .string = estrdup (ev->string)
    };

    m.textbuffer = (struct nstring **)setadd ((void **)m.textbuffer, (void *)&tm, sizeof (struct nstring));
   } else {
    m.textbuffer = NULL;
   }

   modules = (struct mstat **)setadd ((void **)modules, (void *)&m, sizeof (struct mstat));
  }

  emutex_unlock (&modulesmutex);

  for (i = 0; modules[i]; i++) {
   if (((struct mstat *)(modules[i]))->mod == ev->para) {
    mst = (struct mstat *)(modules[i]);
    break;
   }
  }

  mst->task = ev->task;

  if ((ev->status & status_failed) || ev->flag) {
   mst->lines = 4;
   mst->errors = 1 + ev->flag;
  }

  if (enableansicodes) update_screen_ansi (ev, mst);
  if (vofile) update_screen_neat (ev, mst);

  emutex_lock (&feedback_fdsmutex);
  if (feedback_fds || !enableansicodes) {
   update_screen_noansi (ev, mst);
  }
  emutex_unlock (&feedback_fdsmutex);

  if (ev->task & einit_module_feedback_show) {
   ev->task ^= einit_module_feedback_show;
  }
  if (ev->string) {
   ev->string = NULL;
  }

  emutex_unlock (&thismodule->imutex);
 }

 fsync(STDOUT_FILENO);

 return;
}

/*
  -------- update screen the complicated way ----------------------------------
 */
void update_screen_neat (struct einit_event *ev, struct mstat *mst) {
 uint32_t i, line = 4, j;

 if (!vofile) return;

// statusbarlines = 4;

 if (plans) {
  eprintf (vofile, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
 }

 if (modules) {
  eputs ("\e[2;0H( \e[32menabled\e[0m  |", vofile);
  for (i = 0; modules[i]; i++) {
   if ((!((struct mstat *)(modules[i]))->errors) &&
       (((struct mstat *)(modules[i]))->mod) &&
       (((struct mstat *)(modules[i]))->mod->si) &&
       (((struct mstat *)(modules[i]))->mod->si->provides) &&
       (((struct mstat *)(modules[i]))->mod->si->provides[0])) {
    if (((struct mstat *)(modules[i]))->mod->status & status_enabled) {
     eputs (" ", vofile);
     eputs (((struct mstat *)(modules[i]))->mod->si->provides[0], vofile);
     ((struct mstat *)(modules[i]))->display = 0;
     ((struct mstat *)(modules[i]))->lines = 0;
    }
   }
  }
  eputs (" )\e[0K\n", vofile);

  eputs ("\e[3;0H( \e[32mdisabled\e[0m |", vofile);
  for (i = 0; modules[i]; i++) {
   if ((!((struct mstat *)(modules[i]))->errors) &&
       (((struct mstat *)(modules[i]))->mod) &&
       (((struct mstat *)(modules[i]))->mod->si) &&
       (((struct mstat *)(modules[i]))->mod->si->provides) &&
       (((struct mstat *)(modules[i]))->mod->si->provides[0])) {
    if (((struct mstat *)(modules[i]))->mod->status & status_disabled) {
     eputs (" ", vofile);
     eputs (((struct mstat *)(modules[i]))->mod->si->provides[0], vofile);
     ((struct mstat *)(modules[i]))->display = 0;
     ((struct mstat *)(modules[i]))->lines = 0;
    }
   }
  }
  eputs (" )\e[0K\n", vofile);
 }

 for (i = 0; modules[i]; i++) {
  if ((((struct mstat *)(modules[i]))->errors) &&
      (((struct mstat *)(modules[i]))->mod) &&
      (((struct mstat *)(modules[i]))->mod->module)) {
   char *name = (((struct mstat *)(modules[i]))->mod->module->name ? ((struct mstat *)(modules[i]))->mod->module->name : "unknown");

   eprintf (vofile, "\e[%i;0H[ \e[33m..%2.2i\e[0m ] %s:\e[0K\n", line, (((struct mstat *)(modules[i]))->errors -1), name);

   for (j = 0; ((struct mstat *)(modules[i]))->textbuffer[j] && ((j +1) < ((struct mstat *)(modules[i]))->lines); j++) {
    if (((struct mstat *)(modules[i]))->textbuffer[j]->string && (strlen (((struct mstat *)(modules[i]))->textbuffer[j]->string) < 76)) {
     eprintf (vofile, " \e[33m>>\e[0m %s\e[0K\n", ((struct mstat *)(modules[i]))->textbuffer[j]->string);
    } else {
     fputs (" \e[33m>>\e[0m \e[31m...\e[0m", vofile);
    }
   }
  }
  line += ((struct mstat *)(modules[i]))->lines;
//  line ++;
 }
}

/*
  -------- update screen without ansi codes -----------------------------------
 */
void update_screen_noansi (struct einit_event *ev, struct mstat *mst) {
 char *name = "unknown/unnamed",
  feedback[BUFFERSIZE], tfeedback[BUFFERSIZE];

 if (((struct lmodule *)ev->para)->module) {
  const struct smodule *mod = ((struct lmodule *)ev->para)->module;
  if (mod->name) {
   name = mod->name;
  } else if (mod->rid) {
   name = mod->rid;
  }
 }

 *feedback = 0;
 *tfeedback = 0;

 if (ev->task & einit_module_feedback_show) {
  if (ev->task & einit_module_enable) {
   esprintf (tfeedback, BUFFERSIZE, "%s: enabling\n", name);
  } else if (ev->task & einit_module_disable) {
   esprintf (tfeedback, BUFFERSIZE, "%s: disabling\n", name);
  } else  {
   esprintf (tfeedback, BUFFERSIZE, "%s: unknown status change\n", name);
  }
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

/* switch (ev->status) {
  case status_idle:
   if (feedback[0])
    strncpy (tfeedback, " > idle\n", BUFFERSIZE);
   else
    esprintf (tfeedback, BUFFERSIZE, "%s: idle\n", name);
   break;
  case STATUS_ENABLING:
   if (feedback[0])
    strncpy (tfeedback, " > enabling\n", BUFFERSIZE);
   else
    esprintf (tfeedback, BUFFERSIZE, "%s: enabling\n", name);
   break;
 }*/

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if (ev->string) {
  if (feedback[0])
   esprintf (tfeedback, BUFFERSIZE, " > %s\n", ev->string);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: %s\n", name, ev->string);
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if ((ev->status & status_ok) && ev->flag) {
  if (feedback[0])
   esprintf (tfeedback, BUFFERSIZE, " > success, with %i error(s)\n", ev->flag);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: success, with %i error(s)\n", name, ev->flag);
  mst->errors = 1;
 } else if (ev->status & status_ok) {
  if (feedback[0])
   strncpy (tfeedback, " > success\n", BUFFERSIZE);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: success\n", name);
  mst->errors = 0;
 } else if (ev->status & status_failed) {
  if (feedback[0])
   strncpy (tfeedback, " > failed\n", BUFFERSIZE);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: failed\n", name);
  mst->errors = 1;
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if (feedback[0]) {
  if (!enableansicodes) {
   eputs (feedback, stdout);
  }

  if (feedback_fds) {
   uint32_t i = 0;
   for (; feedback_fds[i]; i++) {
    eputs (feedback, feedback_fds[i]->fd);
   }
  }
 }

}

/*
  -------- update screen with ansi codes --------------------------------------
 */
void update_screen_ansi (struct einit_event *ev, struct mstat *mst) {

 char *name = "unknown/unnamed";
 uint32_t line = statusbarlines, i = 0;

 for (i = 0; modules[i]; i++) {
  if (((struct mstat *)(modules[i]))->mod == ev->para) {
   mst = (struct mstat *)(modules[i]);
   break;
  }
//  line += ((struct mstat *)(modules[i]))->lines;
  line ++;
 }

 if (((struct lmodule *)ev->para)->module) {
  const struct smodule *mod = ((struct lmodule *)ev->para)->module;
  if (mod->name) {
   name = mod->name;
  } else if (mod->rid) {
   name = mod->rid;
  }
 }

 if (ev->status == status_idle) {
  eprintf (stdout, "\e[%i;0H[ \e[31mIDLE\e[0m ] %s\e[0K\n", line, name);
 }/* else if (ev->status & status_working) {
  eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s: working...\e[0K\n", line, name);
 }*/

 if (ev->task & einit_module_feedback_show) {
  if (ev->task & einit_module_enable) {
   eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[0K\n", line, name);
  } else if (ev->task & einit_module_disable) {
   eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s: disabling\e[0K\n", line, name);
  } else  {
   eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s\e[0K\n", line, name);
  }
 }

 if (ev->string) {
  strtrim (ev->string);

  if (strlen(ev->string) < 45) {
   eprintf (stdout, "\e[%i;10H%s: %s\e[0K\n", line, name, ev->string);
  }/* else {
   char tmp[BUFFERSIZE];
   eprintf (stdout, "\e[%i;10H%s: <...>\e[0K\n", line, name);

   esprintf (tmp, BUFFERSIZE, "%s: %s", name, ev->string);
   notice (3, tmp);
  }*/
 }

 if ((ev->status & status_ok) && ev->flag) {
  eprintf (stdout, "\e[%i;0H[ \e[33mWA%2.2i\e[0m ] %s\n", line, ev->flag, name);
  mst->errors = 1;
 } else if (ev->status & status_ok) {
  if (ev->task & einit_module_enable) {
   eprintf (stdout, "\e[%i;0H[ \e[32mENAB\e[0m ] %s\n", line, name);
  } else if (ev->task & einit_module_disable) {
   eprintf (stdout, "\e[%i;0H[ \e[32mDISA\e[0m ] %s\n", line, name);
  } else if (ev->task & einit_module_custom) {
   eprintf (stdout, "\e[%i;0H[ \e[32mCSTM\e[0m ] %s\n", line, name);
  } else {
   eprintf (stdout, "\e[%i;0H[ \e[32m OK \e[0m ] %s\n", line, name);
  }

  mst->errors = 0;
 } else if (ev->status & status_failed) {
  if (ev->status & status_enabled) {
   eprintf (stdout, "\e[%i;0H! \e[31mENAB\e[0m ! %s\n", line, name);
  } else if (ev->status & status_disabled) {
   eprintf (stdout, "\e[%i;0H! \e[31mDISA\e[0m ! %s\n", line, name);
  } else {
   eprintf (stdout, "\e[%i;0H[ \e[31mFAIL\e[0m ] %s\n", line, name);
  }

  mst->errors = 1;
 }

}

/*
  -------- function to sort the string-buffer ---------------------------------
 */
int nstringsetsort (struct nstring *st1, struct nstring *st2) {
 if (!st1) return 1;
 if (!st2) return -1;

 return (st2->seqid - st1->seqid);
}

/*
  -------- core event-handler -------------------------------------------------
 */
void einit_feedback_visual_einit_event_handler(struct einit_event *ev) {

 if (ev->type == einit_core_configuration_update) {
  emutex_lock (&thismodule->imutex);

  struct cfgnode *node;

  if ((node = cfg_getnode ("configuration-feedback-visual-shutdown-failure-timeout", NULL)))
   shutdownfailuretimeout = node->value;

  emutex_unlock (&thismodule->imutex);
#if 0
 } else if (ev->type == einit_core_module_update) {
  if (show_progress && !(ev->status & status_working)) {
   if (plans) {
    uint32_t i = 0;
    emutex_lock (&plansmutex);

    if (enableansicodes) {

     eprintf (stdout, "\e[0;0H[ \e[31m....\e[0m ] \e[34m");

     for (; plans[i]; i++) {
      if (plans[i]->plan) {
       eprintf (stdout, " ( %s | %f%% )",
           "unknown",
           get_plan_progress (plans[i]->plan) * 100);
      }
     }

     eprintf (stdout, "\e[0m\e[0K\n");
    } else {
     for (; plans[i]; i++) {
      if (plans[i]->plan) {
       if (plans[i]->plan) {
        eprintf (stdout, " ( %s | %f%% )",
            "unknown",
            get_plan_progress (plans[i]->plan) * 100);
       }
      }
     }
     if (eputs ("", stderr) < 0)
      bitch(bitch_stdio, 0, "puts() failed.");
    }

    emutex_unlock (&plansmutex);
   }
  }
#endif
 }

 return;
}

/*
  -------- power event-handler -------------------------------------------------
 */
void einit_feedback_visual_power_event_handler(struct einit_event *ev) {
 struct cfgnode *n;

 if ((ev->type == einit_power_down_scheduled) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a shutdown has been scheduled, commencing...");
 if ((ev->type == einit_power_reset_scheduled) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a reboot has been scheduled, commencing...");

 if ((ev->type == einit_power_down_imminent) || (ev->type == einit_power_reset_imminent)) {
// shutdown imminent
//  emutex_lock (&thismodule->imutex);

  uint32_t c = shutdownfailuretimeout;
  char errors = 0;

  if (modules) {
   uint32_t i = 0;
   for (; modules [i]; i++) {
    if ((modules [i])->errors) {
     errors = 1;
    }
   }
  }

  if (errors)
   while (c) {
    if (enableansicodes) {
     eprintf (stdout, "\e[0;0H\e[0m[ \e[31m%4.4i\e[0m ] \e[31mWarning: Errors occured while shutting down, waiting...\e[0m\n", c);
    } else {
     eprintf (stdout, "[ %4.4i ] Warning: Errors occured while shutting down, waiting...\n", c);
    }

    sleep (1);
    c--;
   }

  if ((ev->type == einit_power_down_imminent) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
   broadcast_message ("/dev/", "shutting down NOW!");
  if ((ev->type == einit_power_reset_imminent) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
   broadcast_message ("/dev/", "rebooting NOW!");

//  emutex_lock (&thismodule->imutex);
 }

 return;
}

unsigned char broadcast_message (char *path, char *message) {
#if 0
#ifdef POSIXREGEX
 DIR *dir;
 struct dirent *entry;
 if (!path) path = "/dev/";

 unsigned char nfitfc = 0;
 static char *npattern = NULL;
 static regex_t devpattern;
 static unsigned char havedevpattern = 0;

 if (!npattern) {
  nfitfc = 1;
  npattern = cfg_getstring ("configuration-feedback-visual-broadcast-constraints", NULL);
  if (npattern) {
   havedevpattern = !eregcomp (&devpattern, npattern);
  }
 }

 dir = eopendir (path);
 if (dir != NULL) {
  while ((entry = ereaddir (dir))) {
   if (entry->d_name[0] == '.') continue;
   struct stat statbuf;
   char *tmp = emalloc (strlen(path) + entry->d_reclen);
   tmp[0] = 0;
   tmp = strcat (tmp, path);
   tmp = strcat (tmp, entry->d_name);
   if (lstat (tmp, &statbuf)) {
    perror ("einit-feedback-visual-textual");
    free (tmp);
    continue;
   }
   if (!S_ISLNK(statbuf.st_mode)) {
    if (S_ISCHR (statbuf.st_mode) && (!havedevpattern || !regexec (&devpattern, tmp, 0, NULL, 0))) {
     FILE *sf = efopen (tmp, "w");
     if (sf) {
      eprintf (sf, "\n---( BROADCAST MESSAGE )------------------------------------------------------\n >> %s\n-----------------------------------------------------------( eINIT-%6.6i )---\n", message, getpid());

      efclose (sf);
     }
    } else if (S_ISDIR (statbuf.st_mode)) {
     tmp = strcat (tmp, "/");
     broadcast_message (tmp, message);
    }
   }

   free (tmp);
  }
  eclosedir (dir);
 } else {
  errno = 0;
  return 1;
 }


 if (nfitfc) {
  npattern = NULL;
  havedevpattern = 0;
  regfree (&devpattern);
 }
#endif
#endif
 return 0;
}

/*
  -------- function to enable and configure this module -----------------------
 */
int einit_feedback_visual_enable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 struct cfgnode *node = cfg_getnode ("configuration-feedback-visual-use-ansi-codes", NULL);
 if (node)
  enableansicodes = node->flag;

 if ((node = cfg_getnode ("configuration-feedback-visual-calculate-switch-status", NULL)))
  show_progress = node->flag;

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
    } else if (strmatch (filenode->arbattrs[i], "verbose-output")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      if (vofile)
       vofile = freopen (filenode->arbattrs[i+1], "w", vofile);

      if (!vofile);
       vofile = efopen (filenode->arbattrs[i+1], "w");
     } else {
      perror ("einit-feedback-visual-textual: opening verbose-output file");
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
  eputs ("\e[2J\e[0;0H"
         "\e[0;0H[ \e[31m....\e[0m ] \e[34minitialising\e[0m\e[0K\n", stdout);
 }
 if (vofile) {
  eputs ("\e[2J\e[0;0H", vofile);
 }

 event_listen (einit_event_subsystem_feedback, einit_feedback_visual_feedback_event_handler);
 event_listen (einit_event_subsystem_core, einit_feedback_visual_einit_event_handler);
 event_listen (einit_event_subsystem_power, einit_feedback_visual_power_event_handler);

 emutex_unlock (&thismodule->imutex);
 return status_ok;
}

#else

struct feedback_textual_command {
 struct lmodule *module;
 enum einit_module_status status;
 char *message;
 uint32_t seqid;
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
 char warnings;
 time_t lastchange;
};

struct feedback_textual_command **feedback_textual_commandQ = NULL;
struct feedback_textual_module_status **feedback_textual_modules = NULL;

pthread_mutex_t
 feedback_textual_commandQ_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 feedback_textual_commandQ_cond_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t feedback_textual_commandQ_cond = PTHREAD_COND_INITIALIZER;

void feedback_textual_queue_comand (struct lmodule *module, enum einit_module_status status, char *message, uint32_t seqid, time_t ctime) {
 struct feedback_textual_command tnc;
 memset (&tnc, 0, sizeof (struct feedback_textual_command));

 tnc.module = module;
 tnc.status = status;
 tnc.seqid = seqid;
 if (message)
  tnc.message = estrdup (message);
 tnc.ctime = ctime;

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

 char *wmarker = " ";
 char *emarker = " ";
 char *rmarker = " ";
 char *status = "\e[31mwtf?\e[0m";

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
   wmarker = "\e[33m*\e[0m";
  }

  if (st->module->status & status_failed) {
   emarker = "\e[31m!\e[0m";
  }
 }

 if (st->log) {
  uint32_t y = 0;

  for (; st->log[y]; y++) ;

  if (y != 0) {
   y--;
   eprintf (stdout, "%s%s%s[ %s ] %s: %s\e[0K\n", rmarker, emarker, wmarker, status, name, st->log[y]->message);
  } else {
   eprintf (stdout, "%s%s%s[ %s ] %s\e[0K\n", rmarker, emarker, wmarker, status, name);
  }
 } else {
  eprintf (stdout, "%s%s%s[ %s ] %s\e[0K\n", rmarker, emarker, wmarker, status, name);
 }
}

void feedback_textual_update_screen () {
 emutex_lock (&feedback_textual_modules_mutex);

 if (enableansicodes) {
  eputs (/*"\e[2J\e[0;0H"*/
         "\e[0;0H[ \e[31m....\e[0m ] \e[34minitialising\e[0m\e[0K\n", stdout);
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

void feedback_textual_update_module (struct lmodule *module, time_t ctime, uint32_t seqid, char *message) {
 char create_module = 1;
 char do_update = 0;

 emutex_lock (&feedback_textual_modules_mutex);

 if (feedback_textual_modules) {
  uint32_t i = 0;

  for (; feedback_textual_modules[i]; i++) {
   if (feedback_textual_modules[i]->module == module) {
    create_module = 0;

    if (feedback_textual_modules[i]->lastchange < ctime) {
     feedback_textual_modules[i]->lastchange = ctime;
     do_update = 1;
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
   }
  }
 }

 if (create_module) {
  struct feedback_textual_module_status nm;

  memset (&nm, 0, sizeof (struct feedback_textual_module_status));

  nm.module = module;
  nm.lastchange = ctime;

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
/* char *rid = (command->module && command->module->module && command->module->module->rid) ? command->module->module->rid : "no idea",
      *name = (command->module && command->module->module && command->module->module->name) ? command->module->module->name : "no idea";

 if (command->status == status_idle) {
  eprintf (stdout, "[ IDLE ] %s (%s): %s\n", name, rid, command->message ? command->message : " -- ");
 } else {
  if (command->status & status_enabled) {
   eprintf (stdout, "[ ENAB ] %s (%s): %s\n", name, rid, command->message ? command->message : " -- ");
  }

  if (command->status & status_disabled) {
   eprintf (stdout, "[ DISA ] %s (%s): %s\n", name, rid, command->message ? command->message : " -- ");
  }
 }*/

 if (command->module) {
  feedback_textual_update_module (command->module, command->ctime, command->seqid, command->message);
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

//    if (command->message) free (command->message);
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
 if (ev->type == einit_feedback_module_status) {
  feedback_textual_queue_comand (ev->module, ev->status, ev->string, ev->seqid, ev->timestamp);
 }
}

void einit_feedback_visual_einit_event_handler(struct einit_event *ev) {
 if (ev->type == einit_core_service_update) {
  feedback_textual_queue_comand (ev->module, ev->status, NULL, ev->seqid, ev->timestamp);
 }
}

void einit_feedback_visual_power_event_handler(struct einit_event *ev) {
}

void einit_feedback_visual_ipc_event_handler(struct einit_event *ev) {
}

int einit_feedback_visual_cleanup (struct lmodule *this) {
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

 if ((node = cfg_getnode ("configuration-feedback-visual-calculate-switch-status", NULL)))
  show_progress = node->flag;

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
  eputs ("\e[2J\e[0;0H"
    "\e[0;0H[ \e[31m....\e[0m ] \e[34minitialising\e[0m\e[0K\n", stdout);
 }

 event_listen (einit_event_subsystem_feedback, einit_feedback_visual_feedback_event_handler);
 event_listen (einit_event_subsystem_core, einit_feedback_visual_einit_event_handler);
 event_listen (einit_event_subsystem_power, einit_feedback_visual_power_event_handler);

 emutex_unlock (&thismodule->imutex);
 return status_ok;
}

#endif

/*
  -------- function to disable this module ------------------------------------
 */
int einit_feedback_visual_disable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_ignore (einit_event_subsystem_power, einit_feedback_visual_power_event_handler);
 event_ignore (einit_event_subsystem_core, einit_feedback_visual_einit_event_handler);
 event_ignore (einit_event_subsystem_feedback, einit_feedback_visual_feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return status_ok;
}

int einit_feedback_visual_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = einit_feedback_visual_cleanup;
 irr->enable  = einit_feedback_visual_enable;
 irr->disable = einit_feedback_visual_disable;

 event_listen (einit_event_subsystem_ipc, einit_feedback_visual_ipc_event_handler);

 return 0;
}
