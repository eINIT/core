/*
 *  einit-efl-feedback-evas-edje.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/05/2006.
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
#include <pthread.h>
#include <time.h>

#include <Evas.h>
#include <Ecore_Evas.h>
#include <Ecore.h>
#include <Edje.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

char * provides[] = {"feedback", "feedback-visual", NULL};

struct smodule self = {
 EINIT_VERSION, 1, EINIT_MOD_FEEDBACK, 0, "visual/evas-based feedback module", "einit-efl-feedback-evas-edje", provides, NULL, NULL
};

struct lmodule prevdefault;
char enableansicodes = 1;

struct planref {
 struct mloadplan *plan;
 time_t startedat;
};

struct planref **plans = NULL;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;

Ecore_Evas *ee;
Evas *evas;
Evas_Object *edje;
Evas_Coord edje_w, edje_h;
// int width = 1024, height = 768;
int width = 800, height = 600;

pthread_t ethread_th;
// pthread_cond_t ethread_cond = PTHREAD_COND_INITIALIZER;
// pthread_mutex_t ethread_mutex = PTHREAD_MUTEX_INITIALIZER;

int disable (void *, struct mfeedback *);

int configure (struct lmodule *this) {
 evas_init();
 ecore_init();
 ecore_evas_init();
 edje_init();
}

int cleanup (struct lmodule *this) {
 if (ee) {
  disable (NULL, NULL);
 }
 edje_shutdown();
 ecore_evas_shutdown();
 ecore_shutdown();
 evas_shutdown();
}

void *ethread (void *not_used) {
 char *edjepath = cfg_getpath ("edje-theme-path");
 char *themefile;
 struct cfgnode *node_theme = cfg_findnode ("edje-theme-default", 0, NULL);
 struct cfgnode *node_component = cfg_findnode ("edje-theme-default-component", 0, NULL);
 if (!edjepath || !node_theme || !node_theme->svalue || !node_component ||
	 !node_component->svalue)
  return NULL;

/* this is from evas tutorial 1: the basics */
 /* create our Ecore_Evas and show it */
 if (!(ee = ecore_evas_software_x11_new(0, 0, 0, 0, width, height)))
  if (!(ee = ecore_evas_fb_new(NULL, 0, width, height)))
   return NULL;
 ecore_evas_title_set(ee, "eINIT EVAS feedback");
 ecore_evas_show(ee);

 /* get a pointer to our new Evas canvas */
 evas = ecore_evas_get(ee);

 edje = edje_object_add(evas);

 themefile = ecalloc (1, sizeof(char)*(strlen(edjepath) + strlen(node_theme->svalue) + 1));
 themefile = strcat (themefile, edjepath);
 themefile = strcat (themefile, node_theme->svalue);

 edje_object_file_set(edje, themefile, node_component->svalue);
 free (themefile);

 evas_object_move(edje, 0, 0);
// edje_object_size_min_get(edje, &edje_w, &edje_h);
 evas_object_resize(edje, width, height);
 evas_object_show(edje);

// ecore_evas_resize(ee, (int)edje_w, (int)edje_h);
 ecore_evas_show(ee);

 /* start the main event loop */
 ecore_main_loop_begin();
// pthread_mutex_lock (&ethread_mutex);
// pthread_cond_wait (&ethread_cond, &ethread_mutex);
// while (1);

 /* when the main event loop exits, shutdown our libraries */

 ee = NULL;

 return NULL;
}

int enable (void *pa, struct mfeedback *status) {
 if (!pthread_create (&ethread_th, NULL, ethread, NULL)) {
  prevdefault.comment = mdefault.comment;
  if (status->module)
   mdefault.comment = status->module->comment;
  return STATUS_OK;
 } else {
  return STATUS_FAIL;
 }
}

int disable (void *pa, struct mfeedback *status) {
 if (ee) {
  ecore_main_loop_quit ();
  pthread_join (ethread_th, NULL);
 }
 mdefault.comment = prevdefault.comment;
 return STATUS_OK;
}

int comment (struct mfeedback *status) {
 if (status->task & MOD_SCHEDULER) {
  int i = 0;
  struct planref *plan = NULL;
  switch (status->task) {
   case MOD_SCHEDULER_PLAN_COMMIT_START:
//    if (ee)
//     ecore_evas_show(ee);
    plan = ecalloc (1, sizeof(struct planref));
    if (enableansicodes)
     printf ("\e[34mswitching to mode %s.\e[0m\n", newmode);
    else
     printf ("switching to mode %s.\n", newmode);
    pthread_mutex_lock (&plansmutex);
     plan->plan = status->plan;
     plan->startedat = time (NULL);
     plans = (struct planref **)setadd ((void **)plans, (void *)plan);
    pthread_mutex_unlock (&plansmutex);
    break;
   case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
//    if (ee)
//     ecore_evas_hide(ee);
    if (!plans) break;
    pthread_mutex_lock (&plansmutex);
     for (; plans[i]; i++)
      if (plans[i]->plan == status->plan) {
       plan = plans[i];
       break;
      }
     plans = (struct planref **)setdel ((void **)plans, (void *)plan);
    pthread_mutex_unlock (&plansmutex);
    if (enableansicodes)
     printf ("\e[34mnew mode %s is now in effect\e[0m; it has taken %i seconds to activate this mode.\n", currentmode,  time(NULL) - plan->startedat);
    else
     printf ("new mode %s is now in effect.\n", currentmode);
    free (plan);
    break;
  }
 } else {
  char *rid = "unknown/unspecified";
  if (status->module && status->module->module) {
   struct smodule *mod = status->module->module;
   if (mod->rid) {
    rid = mod->rid;
   }
  }

  if (status->task & MOD_FEEDBACK_SHOW) {
   status->task ^= MOD_FEEDBACK_SHOW;
   switch (status->task) {
    case MOD_ENABLE:
     printf ("enabling module: %s\n", rid);
     break;
    case MOD_DISABLE:
     printf ("disabling module: %s\n", rid);
     break;
   }
  }

  switch (status->status) {
   case STATUS_IDLE:
    printf ("%s: idle\n", rid);
    break;
   case STATUS_ENABLING:
    printf ("%s: enabling\n", rid);
    break;
  }

  if (status->verbose) {
   printf ("%s: %s\n", rid, status->verbose);
   status->verbose = NULL;
  }

  if (enableansicodes) {
   if ((status->status & STATUS_OK) && status->errorc)
    printf ("\e[33m%s succeeded, with %i error(s)\e[0m\n", rid, status->errorc);
   else if (status->status & STATUS_OK)
    printf ("\e[32m%s succeeded\e[0m\n", rid);
   else if (status->status & STATUS_FAIL)
    printf ("\e[31m%s failed\e[0m\n", rid);
  } else {
   if ((status->status & STATUS_OK) && status->errorc)
    printf ("%s: success, with %i error(s)\n", rid, status->errorc);
   else if (status->status & STATUS_OK)
    printf ("%s: success\n", rid);
   else if (status->status & STATUS_FAIL)
    printf ("%s: failed\n", rid);
  }
 }
 return 0;
}
