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

char * einit_feedback_visual_provides[] = {"feedback-visual", "feedback-textual", NULL};
const struct smodule einit_feedback_visual_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
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

FILE *vofile = NULL;

struct planref **plans = NULL;
struct mstat **modules = NULL;
struct feedback_fd **feedback_fds = NULL;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modulesmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t feedback_fdsmutex = PTHREAD_MUTEX_INITIALIZER;

char enableansicodes = 1;
char show_progress = 1;
uint32_t shutdownfailuretimeout = 10, statusbarlines = 2;

void einit_feedback_visual_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && strmatch(ev->set[0], "examine") && strmatch(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-use-ansi-codes\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-std-io", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-std-io\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-shutdown-failure-timeout\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-calculate-switch-status", NULL)) {
   eputs (" * configuration variable \"configuration-feedback-visual-calculate-switch-status\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int einit_feedback_visual_cleanup (struct lmodule *this) {
 emutex_lock (&thismodule->imutex);
 event_ignore (EVENT_SUBSYSTEM_IPC, einit_feedback_visual_ipc_event_handler);
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

 event_listen (EVENT_SUBSYSTEM_FEEDBACK, einit_feedback_visual_feedback_event_handler);
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_feedback_visual_einit_event_handler);
 event_listen (EVENT_SUBSYSTEM_POWER, einit_feedback_visual_power_event_handler);

 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

/*
  -------- function to disable this module ------------------------------------
 */
int einit_feedback_visual_disable (void *pa, struct einit_event *status) {
 emutex_lock (&thismodule->imutex);
 event_ignore (EVENT_SUBSYSTEM_POWER, einit_feedback_visual_power_event_handler);
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_feedback_visual_einit_event_handler);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, einit_feedback_visual_feedback_event_handler);
 emutex_unlock (&thismodule->imutex);
 return STATUS_OK;
}

/*
  -------- feedback event-handler ---------------------------------------------
 */
void einit_feedback_visual_feedback_event_handler(struct einit_event *ev) {
 uint32_t line = 0;

 if (ev->type == EVENT_FEEDBACK_BROKEN_SERVICES) {
  char *tmp = set2str (' ', (const char **)ev->set);
  if (tmp) {
   eprintf (stderr, ev->set[1] ? " >> broken services: %s\n" : " >> broken service: %s\n", tmp);

   free (tmp);
  }
 } else if (ev->type == EVENT_FEEDBACK_UNRESOLVED_SERVICES) {
  char *tmp = set2str (' ', (const char **)ev->set);
  if (tmp) {
   eprintf (stderr, ev->set[1] ? " >> unresolved services: %s\n" : " >> unresolved service: %s\n", tmp);

   free (tmp);
  }
 } else if (ev->type == EVENT_FEEDBACK_REGISTER_FD) {
  emutex_lock (&thismodule->imutex);

  struct feedback_fd *newfd = emalloc (sizeof (struct feedback_fd));

  newfd->fd = ev->para;
  newfd->options = ev->flag;

  emutex_lock (&feedback_fdsmutex);

  feedback_fds = (struct feedback_fd **)setadd ((void **)feedback_fds, (void *)newfd, SET_NOALLOC);
  emutex_unlock (&feedback_fdsmutex);

  emutex_unlock (&thismodule->imutex);
 } else if (ev->type == EVENT_FEEDBACK_UNREGISTER_FD) {
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
 } else if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
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
 } else if (ev->type == EVE_FEEDBACK_MODULE_STATUS) {
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

  if ((ev->status & STATUS_FAIL) || ev->flag) {
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

  if (ev->task & MOD_FEEDBACK_SHOW) {
   ev->task ^= MOD_FEEDBACK_SHOW;
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
    if (((struct mstat *)(modules[i]))->mod->status & STATUS_ENABLED) {
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
    if (((struct mstat *)(modules[i]))->mod->status & STATUS_DISABLED) {
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

 if (ev->task & MOD_FEEDBACK_SHOW) {
  if (ev->task & MOD_ENABLE) {
   esprintf (tfeedback, BUFFERSIZE, "%s: enabling\n", name);
  } else if (ev->task & MOD_DISABLE) {
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
  case STATUS_IDLE:
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

 if ((ev->status & STATUS_OK) && ev->flag) {
  if (feedback[0])
   esprintf (tfeedback, BUFFERSIZE, " > success, with %i error(s)\n", ev->flag);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: success, with %i error(s)\n", name, ev->flag);
  mst->errors = 1;
 } else if (ev->status & STATUS_OK) {
  if (feedback[0])
   strncpy (tfeedback, " > success\n", BUFFERSIZE);
  else
   esprintf (tfeedback, BUFFERSIZE, "%s: success\n", name);
  mst->errors = 0;
 } else if (ev->status & STATUS_FAIL) {
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

 if (ev->status == STATUS_IDLE) {
  eprintf (stdout, "\e[%i;0H[ \e[31mIDLE\e[0m ] %s\e[0K\n", line, name);
 }/* else if (ev->status & STATUS_WORKING) {
  eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s: working...\e[0K\n", line, name);
 }*/

 if (ev->task & MOD_FEEDBACK_SHOW) {
  if (ev->task & MOD_ENABLE) {
   eprintf (stdout, "\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[0K\n", line, name);
  } else if (ev->task & MOD_DISABLE) {
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

 if ((ev->status & STATUS_OK) && ev->flag) {
  eprintf (stdout, "\e[%i;0H[ \e[33mWA%2.2i\e[0m ] %s\n", line, ev->flag, name);
  mst->errors = 1;
 } else if (ev->status & STATUS_OK) {
  if (ev->task & MOD_ENABLE) {
   eprintf (stdout, "\e[%i;0H[ \e[32mENAB\e[0m ] %s\n", line, name);
  } else if (ev->task & MOD_DISABLE) {
   eprintf (stdout, "\e[%i;0H[ \e[32mDISA\e[0m ] %s\n", line, name);
  } else if (ev->task & MOD_CUSTOM) {
   eprintf (stdout, "\e[%i;0H[ \e[32mCSTM\e[0m ] %s\n", line, name);
  } else {
   eprintf (stdout, "\e[%i;0H[ \e[32m OK \e[0m ] %s\n", line, name);
  }

  mst->errors = 0;
 } else if (ev->status & STATUS_FAIL) {
  if (ev->status & STATUS_ENABLED) {
   eprintf (stdout, "\e[%i;0H! \e[31mENAB\e[0m ! %s\n", line, name);
  } else if (ev->status & STATUS_DISABLED) {
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

 if (ev->type == EVE_CONFIGURATION_UPDATE) {
  emutex_lock (&thismodule->imutex);

  struct cfgnode *node;

  if ((node = cfg_getnode ("configuration-feedback-visual-shutdown-failure-timeout", NULL)))
   shutdownfailuretimeout = node->value;

  emutex_unlock (&thismodule->imutex);
#if 0
 } else if (ev->type == EVE_MODULE_UPDATE) {
  if (show_progress && !(ev->status & STATUS_WORKING)) {
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
      bitch(BITCH_STDIO, 0, "puts() failed.");
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

 if ((ev->type == EVENT_POWER_DOWN_SCHEDULED) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a shutdown has been scheduled, commencing...");
 if ((ev->type == EVENT_POWER_RESET_SCHEDULED) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a reboot has been scheduled, commencing...");

 if ((ev->type == EVENT_POWER_DOWN_IMMINENT) || (ev->type == EVENT_POWER_RESET_IMMINENT)) {
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

  if ((ev->type == EVENT_POWER_DOWN_IMMINENT) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
   broadcast_message ("/dev/", "shutting down NOW!");
  if ((ev->type == EVENT_POWER_RESET_IMMINENT) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
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

int einit_feedback_visual_configure (struct lmodule *irr) {
 module_init (irr);

 irr->cleanup = einit_feedback_visual_cleanup;
 irr->enable  = einit_feedback_visual_enable;
 irr->disable = einit_feedback_visual_disable;

 event_listen (EVENT_SUBSYSTEM_IPC, einit_feedback_visual_ipc_event_handler);

 return 0;
}
