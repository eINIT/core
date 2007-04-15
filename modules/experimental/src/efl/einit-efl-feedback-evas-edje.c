/*
 *  einit-efl-feedback-evas-edje.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/05/2006.
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
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
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

char * provides[] = {"feedback-visual", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_FEEDBACK,
	.name		= "visual/evas-based feedback module",
	.rid		= "einit-efl-feedback-evas-edje",
    .si           = {
        .provides = provides,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

struct planref {
 struct mloadplan *plan;
 time_t startedat;
};

struct planref **plans = NULL;
pthread_mutex_t plansmutex = PTHREAD_MUTEX_INITIALIZER;

void feedback_event_handler(struct einit_event *ev);

Ecore_Evas *ee;
Evas *evas;
Evas_Object *edje;
Evas_Coord edje_w, edje_h;
// int width = 1024, height = 768;
int width = 800, height = 600;
struct lmodule *me;

pthread_t ethread_th;
// pthread_cond_t ethread_cond = PTHREAD_COND_INITIALIZER;
// pthread_mutex_t ethread_mutex = PTHREAD_MUTEX_INITIALIZER;

int disable (void *, struct einit_event *);

int configure (struct lmodule *this) {
 me = this;
 evas_init();
 ecore_init();
 ecore_evas_init();
 sched_reset_event_handlers();
 edje_init();
}

int cleanup (struct lmodule *this) {
 if (ee) {
  disable (NULL, NULL);
 }
 edje_shutdown();
 ecore_evas_shutdown();
 ecore_shutdown();
 sched_reset_event_handlers();
 evas_shutdown();
}

void *ethread (void *not_used) {
 char *edjepath = cfg_getpath ("configuration-feedback-visual-edje-theme-path"),
      *themefile,
      *theme_default = cfg_getstring ("configuration-feedback-visual-edje-theme-default", NULL),
      *theme_default_component = cfg_getstring ("configuration-feedback-visual-edje-theme-default-component", NULL);

 if (!edjepath || !theme_default || !theme_default_component)
  return NULL;

/* this is (modified) from evas tutorial 1: the basics */
 /* create our Ecore_Evas and show it */
 if (!(ee = ecore_evas_software_x11_new(0, 0, 0, 0, width, height)))
  if (!(ee = ecore_evas_fb_new(NULL, 0, width, height)))
   return NULL;

 ecore_evas_title_set(ee, "eINIT EVAS feedback");
 ecore_evas_show(ee);

 /* get a pointer to our new Evas canvas */
 evas = ecore_evas_get(ee);

 edje = edje_object_add(evas);

 themefile = ecalloc (1, sizeof(char)*(strlen(edjepath) + strlen(theme_default) + 1));
 themefile = strcat (themefile, edjepath);
 themefile = strcat (themefile, theme_default);

 edje_object_file_set(edje, themefile, theme_default_component);
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

int enable (void *pa, struct einit_event *status) {
 if (!pthread_create (&ethread_th, NULL, ethread, NULL)) {
  event_listen (EVENT_SUBSYSTEM_FEEDBACK, feedback_event_handler);

  return STATUS_OK;
 } else {
  return STATUS_FAIL;
 }
}

int disable (void *pa, struct einit_event *status) {
 if (ee) {
  ecore_main_loop_quit ();
  pthread_join (ethread_th, NULL);
 }
 return STATUS_OK;
}

void feedback_event_handler(struct einit_event *ev) {
 pthread_mutex_lock (&me->imutex);

 if (ee) {
  if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
   int i = 0;
   struct planref plan, *cul = NULL;
   uint32_t startedat = 0;
   switch (ev->task) {
    case MOD_SCHEDULER_PLAN_COMMIT_START:
     pthread_mutex_lock (&plansmutex);
      plan.plan = (struct mloadplan *)ev->para;
      plan.startedat = time (NULL);
      plans = (struct planref **)setadd ((void **)plans, (void *)&plan, sizeof (struct planref));
      ecore_evas_show (ee);
     pthread_mutex_unlock (&plansmutex);
     break;
    case MOD_SCHEDULER_PLAN_COMMIT_FINISH:
     if (!plans) break;
     pthread_mutex_lock (&plansmutex);
      for (; plans[i]; i++)
       if (plans[i]->plan == (struct mloadplan *)ev->para) {
        cul = plans[i];
        startedat = plans[i]->startedat;
        break;
       }
      if (cul) {
       plans = (struct planref **)setdel ((void **)plans, (void *)cul);
       ecore_evas_hide (ee);
      }
     pthread_mutex_unlock (&plansmutex);
     break;
   }
  }
 }

 pthread_mutex_unlock (&me->imutex);
}
