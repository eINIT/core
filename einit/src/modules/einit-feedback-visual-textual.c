/*
 *  einit-feedback-visual-textual.c
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

#define _MODULE

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/module-logic.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
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

char * provides[] = {"feedback-visual", "feedback-textual", NULL};
const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "visual/text-based feedback module",
 .rid       = "einit-feedback-visual-textual",
 .si        = {
  .provides = provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

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

void feedback_event_handler(struct einit_event *);
void einit_event_handler(struct einit_event *);
void power_event_handler(struct einit_event *);

void update_screen_neat (struct einit_event *, struct mstat *);
void update_screen_noansi (struct einit_event *, struct mstat *);
void update_screen_ansi (struct einit_event *, struct mstat *);

int nstringsetsort (struct nstring *, struct nstring *);
unsigned char broadcast_message (char *, char *);

double get_plan_progress (struct planref *);

FILE *vofile = NULL;

struct planref **plans = NULL;
struct mstat **modules = NULL;
struct feedback_fd **feedback_fds = NULL;
struct lmodule *me;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modulesmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t feedback_fdsmutex = PTHREAD_MUTEX_INITIALIZER;

char enableansicodes = 1;
char show_progress = 1;
uint32_t shutdownfailuretimeout = 10, statusbarlines = 2;

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-use-ansi-codes\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-std-io", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-std-io\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-use-ansi-codes", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-shutdown-failure-timeout\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }
  if (!cfg_getnode("configuration-feedback-visual-calculate-switch-status", NULL)) {
   fputs (" * configuration variable \"configuration-feedback-visual-calculate-switch-status\" not found.\n", (FILE *)ev->para);
   ev->task++;
  }

  ev->flag = 1;
 }
}

int configure (struct lmodule *this) {
 me = this;
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 if (plans) {
  pthread_mutex_lock (&plansmutex);
  free (plans);
  plans = NULL;
  pthread_mutex_unlock (&plansmutex);
 }
 if (modules) {
  uint32_t y = 0, x = 0;
  pthread_mutex_lock (&modulesmutex);

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
  pthread_mutex_unlock (&modulesmutex);
 }
}

/*
  -------- function to enable and configure this module -----------------------
 */
int enable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&me->imutex);

 struct cfgnode *node = cfg_getnode ("configuration-feedback-visual-use-ansi-codes", NULL);
 if (node)
  enableansicodes = node->flag;

 if (node = cfg_getnode ("configuration-feedback-visual-calculate-switch-status", NULL))
  show_progress = node->flag;

 if (node = cfg_getnode ("configuration-feedback-visual-shutdown-failure-timeout", NULL))
  shutdownfailuretimeout = node->value;

 struct cfgnode *filenode = cfg_getnode ("configuration-feedback-visual-std-io", NULL);

 if (filenode && filenode->arbattrs) {
  uint32_t i = 0;
  FILE *tmp;
  struct stat st;

  for (; filenode->arbattrs[i]; i+=2) {
   errno = 0;

   if (filenode->arbattrs[i])
    if (!strcmp (filenode->arbattrs[i], "stdin")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "r", stdin);
      if (!tmp)
       freopen ("/dev/null", "r+", stdin);
     } else {
      perror ("einit-feedback-visual-textual: opening stdin");
     }
    } else if (!strcmp (filenode->arbattrs[i], "stdout")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "w", stdout);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "w", stdout);
     } else {
      perror ("einit-feedback-visual-textual: opening stdout");
      enableansicodes = 0;
     }
    } else if (!strcmp (filenode->arbattrs[i], "stderr")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      tmp = freopen (filenode->arbattrs[i+1], "a", stderr);
      if (!tmp)
       tmp = freopen ("einit-panic-stdout", "a", stderr);
      if (tmp)
       fprintf (stderr, "\n%i: eINIT: visualiser einit-vis-text activated.\n", time(NULL));
     } else {
      perror ("einit-feedback-visual-textual: opening stderr");
      enableansicodes = 0;
     }
    } else if (!strcmp (filenode->arbattrs[i], "verbose-output")) {
     if (!stat (filenode->arbattrs[i+1], &st)) {
      if (vofile)
       vofile = freopen (filenode->arbattrs[i+1], "w", vofile);
      else
       vofile = fopen (filenode->arbattrs[i+1], "w");
     } else {
      perror ("einit-feedback-visual-textual: opening verbose-output file");
     }
    } else if (!strcmp (filenode->arbattrs[i], "console")) {
#ifdef LINUX
     int tfd = 0, tioarg = (12 << 8) | 11;
     signed long int arg = 1;
     errno = 0;
     if (tfd = open (filenode->arbattrs[i+1], O_WRONLY, 0))
      ioctl (tfd, TIOCCONS, 0);
     if (errno)
//      perror ("einit-tty: redirecting console");
      perror (filenode->arbattrs[i+1]);

#else
     fputs ("einit-tty: console redirection support currently only available on LINUX\n", stderr);
#endif
/*     stderr = freopen (filenode->arbattrs[i+1], "w", stderr);
     if (!stdin)
      stderr = freopen ("einit-panic-stdout", "w", stderr);*/
    } else if (!strcmp (filenode->arbattrs[i], "kernel-vt")) {
#ifdef LINUX
     int arg = (strtol (filenode->arbattrs[i+1], (char **)NULL, 10) << 8) | 11;
     errno = 0;

     ioctl(0, TIOCLINUX, &arg);
     if (errno)
      perror ("einit-feedback-visual-textual: redirecting kernel messages");
#else
     fputs ("einit-feedback-visual-textual: kernel message redirection support currently only available on LINUX\n", stderr);
#endif
    } else if (!strcmp (filenode->arbattrs[i], "activate-vt")) {
#ifdef LINUX
     uint32_t vtn = strtol (filenode->arbattrs[i+1], (char **)NULL, 10);
     int tfd = 0;
     errno = 0;
     if (tfd = open ("/dev/tty1", O_RDWR, 0))
      ioctl (tfd, VT_ACTIVATE, vtn);
     if (errno)
      perror ("einit-feedback-visual-textual: activate terminal");
     if (tfd > 0) close (tfd);
#else
     fputs ("einit-feedback-visual-textual: terminal activation support currently only available on LINUX\n", stderr);
#endif
    }
  }
 }

 if (enableansicodes)
  fputs ("\e[2J\e[0;0H", stdout);
 if (vofile)
  fputs ("\e[2J\e[0;0H", vofile);

 event_listen (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_listen (EVENT_SUBSYSTEM_POWER, power_event_handler);

 pthread_mutex_unlock (&me->imutex);
 return STATUS_OK;
}

/*
  -------- function to disable this module ------------------------------------
 */
int disable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&me->imutex);
 event_ignore (EVENT_SUBSYSTEM_POWER, power_event_handler);
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);
 pthread_mutex_unlock (&me->imutex);
 return STATUS_OK;
}

/*
  -------- feedback event-handler ---------------------------------------------
 */
void feedback_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);

 uint32_t line = 0, olines = 0;

 if (ev->type == EVENT_FEEDBACK_REGISTER_FD) {
  struct feedback_fd *newfd = emalloc (sizeof (struct feedback_fd));

  newfd->fd = ev->para;
  newfd->options = ev->flag;

  pthread_mutex_lock (&feedback_fdsmutex);

//  fdputs (" >> registering feedback\n", ev->integer);

  feedback_fds = (struct feedback_fd **)setadd ((void **)feedback_fds, (void *)newfd, SET_NOALLOC);
  pthread_mutex_unlock (&feedback_fdsmutex);
 } else if (ev->type == EVENT_FEEDBACK_UNREGISTER_FD) {
   uint32_t i = 0;
   pthread_mutex_lock (&feedback_fdsmutex);

//   fdputs (" >> unregistering feedback\n", ev->integer);

   if (feedback_fds) for (; feedback_fds[i]; i++) {
    struct feedback_fd *newfd = (struct feedback_fd *)feedback_fds[i];
    if (newfd->fd == ev->para) {
     feedback_fds = (struct feedback_fd **)setdel ((void **)feedback_fds, (void *)newfd);
     free (newfd);
     break;
    }
   }

   pthread_mutex_unlock (&feedback_fdsmutex);
 } else if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
  int i = 0;
  struct planref plan, *cul = NULL;
  uint32_t startedat = 0;
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    if (enableansicodes)
     printf ("\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
    else
     printf ("switching to mode %s.\n", (cmode && cmode->id) ? cmode->id : "unknown");

    if (vofile)
     fprintf (vofile, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
    pthread_mutex_lock (&plansmutex);
     plan.plan = (struct mloadplan *)ev->para;
     plan.startedat = time (NULL);
     plan.max_changes = 0;
     plan.min_changes = 0;
     plans = (struct planref **)setadd ((void **)plans, (void *)&plan, sizeof (struct planref));
    pthread_mutex_unlock (&plansmutex);
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
    if (enableansicodes) {
     pthread_mutex_lock (&modulesmutex);
     line = setcount ((void **)modules) +1;
     pthread_mutex_unlock (&modulesmutex);
    }

    if (!plans) break;
    pthread_mutex_lock (&plansmutex);
     for (; plans[i]; i++)
      if (plans[i]->plan == (struct mloadplan *)ev->para) {
       cul = plans[i];
       startedat = plans[i]->startedat;
       break;
      }
     if (cul)
      plans = (struct planref **)setdel ((void **)plans, (void *)cul);
    pthread_mutex_unlock (&plansmutex);
    if (enableansicodes)
     printf ("\e[0;0H[ \e[33m%04.4i\e[0m ] \e[34mnew mode \"%s\" is now in effect.\e[0m\e[0K\n", time(NULL) - startedat, (amode && amode->id) ? amode->id : "unknown");
    else
     printf ("new mode %s is now in effect.\n", (amode && amode->id) ? amode->id : "unknown");

    if (vofile)
     fprintf (vofile, "\e[0;0H[ \e[33m%04.4i\e[0m ] \e[34mnew mode \"%s\" is now in effect.\e[0m\e[0K\n", time(NULL) - startedat, (amode && amode->id) ? amode->id : "unknown"); break;
  }
 } else if (ev->type == EVE_FEEDBACK_MODULE_STATUS) {
  time_t lupdate;
  struct mstat *mst = NULL;
  char *name = "unknown/unspecified";
  uint32_t i = 0;

  pthread_mutex_lock (&modulesmutex);
  if (modules) {
   for (i = 0; modules[i]; i++)
    if (((struct mstat *)(modules[i]))->mod == ev->para) {
     if (ev->string) {
      struct nstring tm = {
       .seqid = ev->seqid,
       .string = estrdup (ev->string)
      };

      ((struct mstat *)(modules[i]))->textbuffer = (struct nstring **)setadd ((void **)(((struct mstat *)(modules[i]))->textbuffer), (void *)&tm, sizeof (struct nstring));

      setsort ((void **)(((struct mstat *)(modules[i]))->textbuffer), 0, (signed int(*)(void *, void*))nstringsetsort);
     }

     if (((struct mstat *)(modules[i]))->seqid > ev->seqid) {
/* discard older messages that came in after newer ones (happens frequently in multi-threaded situations) */
      pthread_mutex_unlock (&modulesmutex);
      pthread_mutex_unlock (&me->imutex);
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

  pthread_mutex_unlock (&modulesmutex);

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
  update_screen_noansi (ev, mst);

  if (ev->task & MOD_FEEDBACK_SHOW) {
   ev->task ^= MOD_FEEDBACK_SHOW;
  }
  if (ev->string) {
   ev->string = NULL;
  }

 } else if (ev->type == EVE_FEEDBACK_NOTICE) {
  if (ev->string) {
   strtrim (ev->string);
   fprintf (stderr, "[time=%i; severity=%i] %s\n", time(NULL), ev->flag, ev->string);
  }
 }

 fsync(STDOUT_FILENO);

 pthread_mutex_unlock (&me->imutex);
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
  fprintf (vofile, "\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[0K\n", (cmode && cmode->id) ? cmode->id : "unknown");
 }

 pthread_mutex_lock (&modulesmutex);

 if (modules) {
  fputs ("\e[2;0H( \e[32menabled\e[0m  |", vofile);
  for (i = 0; modules[i]; i++) {
   if ((!((struct mstat *)(modules[i]))->errors) &&
       (((struct mstat *)(modules[i]))->mod) &&
       (((struct mstat *)(modules[i]))->mod->si) &&
       (((struct mstat *)(modules[i]))->mod->si->provides) &&
       (((struct mstat *)(modules[i]))->mod->si->provides[0])) {
    if (((struct mstat *)(modules[i]))->mod->status & STATUS_ENABLED) {
     fputs (" ", vofile);
     fputs (((struct mstat *)(modules[i]))->mod->si->provides[0], vofile);
     ((struct mstat *)(modules[i]))->display = 0;
     ((struct mstat *)(modules[i]))->lines = 0;
    }
   }
  }
  fputs (" )\e[0K\n", vofile);

  fputs ("\e[3;0H( \e[32mdisabled\e[0m |", vofile);
  for (i = 0; modules[i]; i++) {
   if ((!((struct mstat *)(modules[i]))->errors) &&
       (((struct mstat *)(modules[i]))->mod) &&
       (((struct mstat *)(modules[i]))->mod->si) &&
       (((struct mstat *)(modules[i]))->mod->si->provides) &&
       (((struct mstat *)(modules[i]))->mod->si->provides[0])) {
    if (((struct mstat *)(modules[i]))->mod->status & STATUS_DISABLED) {
     fputs (" ", vofile);
     fputs (((struct mstat *)(modules[i]))->mod->si->provides[0], vofile);
     ((struct mstat *)(modules[i]))->display = 0;
     ((struct mstat *)(modules[i]))->lines = 0;
    }
   }
  }
  fputs (" )\e[0K\n", vofile);
 }

 for (i = 0; modules[i]; i++) {
  if ((((struct mstat *)(modules[i]))->errors) &&
      (((struct mstat *)(modules[i]))->mod) &&
      (((struct mstat *)(modules[i]))->mod->module)) {
   char *name = (((struct mstat *)(modules[i]))->mod->module->name ? ((struct mstat *)(modules[i]))->mod->module->name : "unknown");

   fprintf (vofile, "\e[%i;0H[ \e[33m..%2.2i\e[0m ] %s:\e[0K\n", line, (((struct mstat *)(modules[i]))->errors -1), name);
   for (j = 0; ((struct mstat *)(modules[i]))->textbuffer[j] && ((j +1) < ((struct mstat *)(modules[i]))->lines); j++) {
    if (((struct mstat *)(modules[i]))->textbuffer[j]->string && (strlen (((struct mstat *)(modules[i]))->textbuffer[j]->string) < 76)) {
     fprintf (vofile, " \e[33m>>\e[0m %s\e[0K\n", ((struct mstat *)(modules[i]))->textbuffer[j]->string);
    } else {
     fprintf (vofile, " \e[33m>>\e[0m \e[31m...\e[0m");
    }
   }
  }
  line += ((struct mstat *)(modules[i]))->lines;
//  line ++;
 }

 pthread_mutex_unlock (&modulesmutex);
}

/*
  -------- update screen without ansi codes -----------------------------------
 */
void update_screen_noansi (struct einit_event *ev, struct mstat *mst) {
 char *name = "unknown/unnamed",
       feedback[2048], tfeedback[256];

 if (((struct lmodule *)ev->para)->module) {
  struct smodule *mod = ((struct lmodule *)ev->para)->module;
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
   snprintf (tfeedback, 256, "%s: enabling\n", name);
  } else if (ev->task & MOD_DISABLE) {
   snprintf (tfeedback, 256, "%s: disabling\n", name);
  } else  {
   snprintf (tfeedback, 256, "%s: unknown status change\n", name);
  }
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 switch (ev->status) {
  case STATUS_IDLE:
   if (feedback[0])
    snprintf (tfeedback, 256, " > idle\n");
   else
    snprintf (tfeedback, 256, "%s: idle\n", name);
   break;
  case STATUS_ENABLING:
   if (feedback[0])
    snprintf (tfeedback, 256, " > enabling\n");
   else
    snprintf (tfeedback, 256, "%s: enabling\n", name);
   break;
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if (ev->string) {
  if (feedback[0])
   snprintf (tfeedback, 256, " > %s\n", ev->string);
  else
   snprintf (tfeedback, 256, "%s: %s\n", name, ev->string);
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if ((ev->status & STATUS_OK) && ev->flag) {
  if (feedback[0])
   snprintf (tfeedback, 256, " > success, with %i error(s)\n", ev->flag);
  else
   snprintf (tfeedback, 256, "%s: success, with %i error(s)\n", name, ev->flag);
  mst->errors = 1;
 } else if (ev->status & STATUS_OK) {
  if (feedback[0])
   snprintf (tfeedback, 256, " > success\n");
  else
   snprintf (tfeedback, 256, "%s: success\n", name);
  mst->errors = 0;
 } else if (ev->status & STATUS_FAIL) {
  if (feedback[0])
   snprintf (tfeedback, 256, " > failed\n");
  else
   snprintf (tfeedback, 256, "%s: failed\n", name);
  mst->errors = 1;
 }

 if (tfeedback[0]) {
  strcat (feedback, tfeedback);
  *tfeedback = 0;
 }

 if (feedback[0]) {
  if (!enableansicodes)
   printf (feedback);

  if (feedback_fds) {
   pthread_mutex_lock (&feedback_fdsmutex);

   if (feedback_fds) {
    uint32_t i = 0;
    for (; feedback_fds[i]; i++) {
     fputs (feedback, feedback_fds[i]->fd);
    }
   }

   pthread_mutex_unlock (&feedback_fdsmutex);
  }
 }
}

/*
  -------- update screen with ansi codes --------------------------------------
 */
void update_screen_ansi (struct einit_event *ev, struct mstat *mst) {
 char *name = "unknown/unnamed";
 uint32_t line = statusbarlines, lines = 0, i = 0;

 for (i = 0; modules[i]; i++) {
  if (((struct mstat *)(modules[i]))->mod == ev->para) {
   mst = (struct mstat *)(modules[i]);
   break;
  }
//  line += ((struct mstat *)(modules[i]))->lines;
  line ++;
 }

 if (((struct lmodule *)ev->para)->module) {
  struct smodule *mod = ((struct lmodule *)ev->para)->module;
  if (mod->name) {
   name = mod->name;
  } else if (mod->rid) {
   name = mod->rid;
  }
 }

 if (ev->task & MOD_FEEDBACK_SHOW) {
  if (ev->task & MOD_ENABLE) {
   printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[0K\n", line, name);
  } else if (ev->task & MOD_DISABLE) {
   printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: disabling\e[0K\n", line, name);
  } else  {
   printf ("\e[%i;0H[ \e[31m....\e[0m ] %s\e[0K\n", line, name);
  }
 }

 switch (ev->status) {
  case STATUS_IDLE:
    printf ("\e[%i;0H[ \e[31mIDLE\e[0m ] %s\e[0K\n", line, name);
   break;
  case STATUS_ENABLING:
    printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[0K\n", line, name);
   break;
 }

 if (ev->string) {
  strtrim (ev->string);

  if (strlen(ev->string) < 45)
   printf ("\e[%i;10H%s: %s\e[0K\n", line, name, ev->string);
  else {
   char tmp[1024];
   printf ("\e[%i;10H%s: <...>\e[0K\n", line, name);

   snprintf (tmp, 1024, "%s: %s", name, ev->string);
   notice (3, tmp);
  }
 }

 if ((ev->status & STATUS_OK) && ev->flag) {
  printf ("\e[%i;0H[ \e[33mWA%2.2i\e[0m ] %s\n", line, ev->flag, name);
  mst->errors = 1;
 } else if (ev->status & STATUS_OK) {
  if (ev->task & MOD_ENABLE)
   printf ("\e[%i;0H[ \e[32mENAB\e[0m ] %s\n", line, name);
  else if (ev->task & MOD_DISABLE)
   printf ("\e[%i;0H[ \e[32mDISA\e[0m ] %s\n", line, name);
  else if (ev->task & MOD_RESET)
   printf ("\e[%i;0H[ \e[32mRSET\e[0m ] %s\n", line, name);
  else if (ev->task & MOD_RELOAD)
   printf ("\e[%i;0H[ \e[32mRELO\e[0m ] %s\n", line, name);
  else
   printf ("\e[%i;0H[ \e[32mOK\e[0m ] %s\n", line, name);

  mst->errors = 0;
 } else if (ev->status & STATUS_FAIL) {
  printf ("\e[%i;0H[ \e[31mFAIL\e[0m ] %s\n", line, name);
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
void einit_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);

 struct cfgnode *n;

 if (ev->type == EVE_CONFIGURATION_UPDATE) {
  struct cfgnode *node;
  fprintf (stderr, "[[ updating configuration ]]\n");

  if (node = cfg_getnode ("configuration-feedback-visual-shutdown-failure-timeout", NULL))
   shutdownfailuretimeout = node->value;
 } else if (ev->type == EVE_MODULE_UPDATE) {
  if (show_progress && !(ev->status & STATUS_WORKING)) {
   if (plans) {
    uint32_t i = 0;
    pthread_mutex_lock (&plansmutex);

    if (enableansicodes) {

     printf ("\e[0;0H[ \e[31m....\e[0m ] \e[34m");

     for (; plans[i]; i++) {
      if (plans[i]->plan) {
       printf (" ( %s | %f%% )", (plans[i]->plan->mode && plans[i]->plan->mode->id) ? plans[i]->plan->mode->id : "unknown", get_plan_progress (plans[i]) * 100);
      }
     }

     printf ("\e[0m\e[0K\n");
    } else {
     for (; plans[i]; i++) {
      if (plans[i]->plan) {
       if (plans[i]->plan) {
        printf (" ( %s | %f%% )", (plans[i]->plan->mode && plans[i]->plan->mode->id) ? plans[i]->plan->mode->id : "unknown", get_plan_progress (plans[i]) * 100);
       }
      }
     }
     puts ("");
    }

    pthread_mutex_unlock (&plansmutex);
   }
  }
 }

 pthread_mutex_unlock (&me->imutex);
 return;
}

/*
  -------- power event-handler -------------------------------------------------
 */
void power_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);

 struct cfgnode *n;

 if ((ev->type == EVENT_POWER_DOWN_SCHEDULED) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a shutdown has been scheduled, commencing...");
 if ((ev->type == EVENT_POWER_RESET_SCHEDULED) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
  broadcast_message ("/dev/", "a reboot has been scheduled, commencing...");

 if ((ev->type == EVENT_POWER_DOWN_IMMINENT) || (ev->type == EVENT_POWER_RESET_IMMINENT)) {
// shutdown imminent
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
    if (enableansicodes)
     printf ("\e[0;0H\e[0m[ \e[31m%04.4i\e[0m ] \e[31mWarning: Errors occured while shutting down, waiting...\e[0m\n", c);
    else
     printf ("[ %04.4i ] Warning: Errors occured while shutting down, waiting...\n", c);

    sleep (1);
    c--;
   }

  if ((ev->type == EVENT_POWER_DOWN_IMMINENT) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
   broadcast_message ("/dev/", "shutting down NOW!");
  if ((ev->type == EVENT_POWER_RESET_IMMINENT) && ((n = cfg_getnode ("configuration-feedback-visual-reset-shutdown-broadcast-messages", NULL)) && n->flag))
   broadcast_message ("/dev/", "rebooting NOW!");
 }

 pthread_mutex_unlock (&me->imutex);
 return;
}

unsigned char broadcast_message (char *path, char *message) {
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
   uint32_t err;
   if (!(err = regcomp (&devpattern, npattern, REG_EXTENDED)))
    havedevpattern = 1;
   else {
    char errorcode [1024];
    regerror (err, &devpattern, errorcode, 1024);
    fputs (errorcode, stdout);
   }
  }
 }

 dir = opendir (path);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
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
     FILE *sf = fopen (tmp, "w");
     if (sf) {
      fprintf (sf, "\n---( BROADCAST MESSAGE )------------------------------------------------------\n >> %s\n-----------------------------------------------------------( eINIT-%6.6i )---\n", message, getpid());
      fclose (sf);
     }
    } else if (S_ISDIR (statbuf.st_mode)) {
     tmp = strcat (tmp, "/");
     broadcast_message (tmp, message);
    }
   }

   free (tmp);
  }
  closedir (dir);
 } else {
  fprintf (stdout, "einit-feedback-visual-textual: could not open %s\n", path);
  errno = 0;
  return 1;
 }


 if (nfitfc) {
  npattern = NULL;
  havedevpattern = 0;
  regfree (&devpattern);
 }
#endif
 return 0;
}

double get_plan_progress (struct planref *planr) {
 if (!planr || !planr->plan) return -1;

 struct mloadplan *plan = planr->plan;
 struct stree *scur = plan->services;

 uint32_t changes = 0;

 pthread_mutex_lock (&(plan->vizmutex));

 if (!planr->min_changes)
  planr->min_changes = setcount ((void **)plan->enable) + setcount ((void **)plan->disable) + setcount ((void **)plan->reset);
 if (!planr->max_changes) {
  while (scur) {
   planr->max_changes++;
   scur = streenext(scur);
  }
  scur = plan->services;
 }

 while (scur) {
  struct mloadplan_node *n = scur->value;

  if (n && (n->changed >= 2)) changes++;

  scur = streenext(scur);
 }

 pthread_mutex_unlock (&(plan->vizmutex));

 if (!changes) return 0;
 if (!planr->max_changes) return -1;
 return (double)changes / (double)planr->max_changes;
}

