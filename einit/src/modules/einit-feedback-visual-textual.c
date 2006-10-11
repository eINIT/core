/*
 *  einit-feedback-visual-textual.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/03/2006.
 *  Renamed from vis-text.c on 11/10/2006.
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
#include <inttypes.h>
#include <time.h>
#include <unistd.h>

#ifdef LINUX
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
/* okay, i think i found the proper file now */
#include <asm/ioctls.h>
#include <linux/vt.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"feedback-visual", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_FEEDBACK,
	.options	= 0,
	.name		= "visual/text-based feedback module",
	.rid		= "einit-feedback-visual-textual",
	.provides	= provides,
	.requires	= NULL,
	.notwith	= NULL
};

struct planref {
 struct mloadplan *plan;
 time_t startedat;
};

struct mstat {
 struct lmodule *mod;
 uint32_t seqid;
 time_t lastupdate;
};

void comment_event_handler(struct einit_event *);
void notice_event_handler(struct einit_event *);

struct planref **plans = NULL;
struct mstat **modules = NULL;
struct lmodule *me;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t modulesmutex = PTHREAD_MUTEX_INITIALIZER;
char enableansicodes = 1;

int configure (struct lmodule *this) {
 struct cfgnode *node = cfg_findnode ("use-ansi-codes", 0, NULL);
 if (node)
  enableansicodes = node->flag;

 me = this;
}

int cleanup (struct lmodule *this) {
 if (plans) {
  pthread_mutex_lock (&plansmutex);
  free (plans);
  plans = NULL;
  pthread_mutex_unlock (&plansmutex);
 }
 if (modules) {
  pthread_mutex_lock (&modulesmutex);
  free (modules);
  modules = NULL;
  pthread_mutex_unlock (&modulesmutex);
 }
}

int enable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&me->imutex);
 struct cfgnode *filenode = cfg_findnode ("std-io", 0, NULL);

 if (filenode && filenode->arbattrs) {
  uint32_t i = 0;
  FILE *tmp;
  for (; filenode->arbattrs[i]; i+=2) {
   if (filenode->arbattrs[i])
    if (!strcmp (filenode->arbattrs[i], "stdin")) {
     tmp = freopen (filenode->arbattrs[i+1], "r", stdin);
     if (!tmp)
      freopen ("einit-panic-stdin", "r+", stdin);
    } else if (!strcmp (filenode->arbattrs[i], "stdout")) {
     tmp = freopen (filenode->arbattrs[i+1], "w", stdout);
     if (!tmp)
      tmp = freopen ("einit-panic-stdout", "w", stdout);
    } else if (!strcmp (filenode->arbattrs[i], "stderr")) {
     tmp = freopen (filenode->arbattrs[i+1], "a", stderr);
     if (!tmp)
      tmp = freopen ("einit-panic-stdout", "a", stderr);
     if (tmp)
      fprintf (stderr, "\n%i: eINIT: visualiser einit-vis-text activated.\n", time(NULL));
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
      perror ("einit-tty: redirecting kernel messages");
#else
     fputs ("einit-tty: kernel message redirection support currently only available on LINUX\n", stderr);
#endif
    } else if (!strcmp (filenode->arbattrs[i], "activate-vt")) {
#ifdef LINUX
     uint32_t vtn = strtol (filenode->arbattrs[i+1], (char **)NULL, 10);
     int tfd = 0;
     errno = 0;
     if (tfd = open ("/dev/tty1", O_RDWR, 0))
      ioctl (tfd, VT_ACTIVATE, vtn);
     if (errno)
      perror ("einit-tty: activate terminal");
     if (tfd > 0) close (tfd);
#else
     fputs ("einit-tty: terminal activation support currently only available on LINUX\n", stderr);
#endif
    }
  }
 }

 if (enableansicodes)
  fputs ("\e[2J\e[0;0H", stdout);

 event_listen (EINIT_EVENT_TYPE_FEEDBACK, comment_event_handler);
 event_listen (EINIT_EVENT_TYPE_NOTICE, notice_event_handler);

 pthread_mutex_unlock (&me->imutex);
 return STATUS_OK;
}

int disable (void *pa, struct einit_event *status) {
 pthread_mutex_lock (&me->imutex);
 event_ignore (EINIT_EVENT_TYPE_FEEDBACK, comment_event_handler);
 event_ignore (EINIT_EVENT_TYPE_NOTICE, notice_event_handler);
// cleanup ((struct lmodule *)status->para);
 pthread_mutex_unlock (&me->imutex);
 return STATUS_OK;
}

void comment_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);

 uint32_t line = 0, olines = 0;
 if (ev->task & MOD_SCHEDULER) {
  int i = 0;
  struct planref plan, *cul = NULL;
  uint32_t startedat = 0;
  switch (ev->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
    if (enableansicodes)
     printf ("\e[0;0H[ \e[31m....\e[0m ] \e[34mswitching to mode \"%s\".\e[0m\e[K\n", newmode);
    else
     printf ("switching to mode %s.\n", newmode);
    pthread_mutex_lock (&plansmutex);
     plan.plan = (struct mloadplan *)ev->para;
     plan.startedat = time (NULL);
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
     printf ("\e[0;0H[ \e[33m%04.4i\e[0m ] \e[34mnew mode \"%s\" is now in effect.\e[0m\e[K\n", time(NULL) - startedat, currentmode);
    else
     printf ("new mode %s is now in effect.\n", currentmode);
    break;
  }
 } else {
  time_t lupdate;

  if (enableansicodes) {
   pthread_mutex_lock (&modulesmutex);
   if (modules) {
    for (; modules[line]; line++)
     if (((struct mstat *)(modules[line]))->mod == ev->para) {
      if (((struct mstat *)(modules[line]))->seqid > ev->integer) {
/* discard older messages that came in after newer ones (happens frequently in multi-threaded situations) */
       pthread_mutex_unlock (&me->imutex);
       pthread_mutex_unlock (&modulesmutex);
       return;
      }
      ((struct mstat *)(modules[line]))->seqid = ev->integer;
      break;
     }
   }
   if (!modules || !modules [line]) {
    struct mstat m; memset (&m, 0, sizeof (struct mstat));
    m.mod = ev->para;
    m.seqid = ev->integer;
    modules = (struct mstat **)setadd ((void **)modules, (void *)&m, sizeof (struct mstat));
   }

   olines = setcount ((void **)modules) + 2;
   pthread_mutex_unlock (&modulesmutex);

   line+=2;
  }

  char *name = "unknown/unspecified";
  if (ev->para && ((struct lmodule *)ev->para)->module) {
   struct smodule *mod = ((struct lmodule *)ev->para)->module;
   if (mod->name) {
    name = mod->name;
   } else if (mod->rid) {
    name = mod->rid;
   }
  }

  if (enableansicodes) {
   if (ev->task & MOD_FEEDBACK_SHOW) {
    ev->task ^= MOD_FEEDBACK_SHOW;
    switch (ev->task) {
     case MOD_ENABLE:
      printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[K\n", line, name);
      break;
     case MOD_DISABLE:
      printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: disabling\e[K\n", line, name);
      break;
     default:
      printf ("\e[%i;0H[ \e[31m....\e[0m ] %s\e[K\n", line, name);
      break;
    }
   }

   switch (ev->status) {
    case STATUS_IDLE:
      printf ("\e[%i;0H[ \e[31mIDLE\e[0m ] %s\e[K\n", line, name);
     break;
    case STATUS_ENABLING:
      printf ("\e[%i;0H[ \e[31m....\e[0m ] %s: enabling\e[K\n", line, name);
     break;
   }

   if (ev->string) {
    printf ("\e[%i;10H%s: %s\e[K\n", line, name, ev->string);
    ev->string = NULL;
   }
  } else {
   if (ev->task & MOD_FEEDBACK_SHOW) {
    ev->task ^= MOD_FEEDBACK_SHOW;
    switch (ev->task) {
     case MOD_ENABLE:
      printf ("%s: enabling\n", name);
      break;
     case MOD_DISABLE:
      printf ("%s: disabling\n", name);
      break;
     default:
      printf ("%s:\n", name);
      break;
    }
   }

   switch (ev->status) {
    case STATUS_IDLE:
      printf ("%s: idle\n", name);
     break;
    case STATUS_ENABLING:
      printf ("%s: enabling\n", name);
     break;
   }

   if (ev->string) {
    printf ("%s: %s\n", name, ev->string);
    ev->string = NULL;
   }
  }

  if (enableansicodes) {
   if ((ev->status & STATUS_OK) && ev->flag)
    printf ("\e[%i;0H[ \e[33mWA%2.2i\e[0m ] %s\n", line, ev->flag, name);
   else if (ev->status & STATUS_OK) {
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
   } else if (ev->status & STATUS_FAIL)
    printf ("\e[%i;0H[ \e[31mFAIL\e[0m ] %s\n", line, name);

/* goto the last line, so that subsequent output of other sources will not mess up the screen */
   printf ("\e[%i;0H", olines);
  } else {
   if ((ev->status & STATUS_OK) && ev->flag)
    printf ("%s: success, with %i error(s)\n", name, ev->flag);
   else if (ev->status & STATUS_OK)
    printf ("%s: success\n", name);
   else if (ev->status & STATUS_FAIL)
    printf ("%s: failed\n", name);
  }
 }

 fsync(STDOUT_FILENO);

 pthread_mutex_unlock (&me->imutex);
 return;
}

void notice_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);
 if (ev->string) {
  strtrim (ev->string);
  fprintf (stderr, "[time=%i; severity=%i] %s\n", time(NULL), ev->flag, ev->string);
 }
 pthread_mutex_unlock (&me->imutex);
}
