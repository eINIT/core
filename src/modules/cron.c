/*
 *  cron.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_cron_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * einit_cron_provides[] = { "ecron", NULL};

const struct smodule einit_cron_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT Cron",
 .rid       = "einit-cron",
 .si        = {
  .provides = einit_cron_provides,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_cron_configure
};

module_register(einit_cron_self);

#endif

struct einit_cron_job {
 uintptr_t *years;
 uintptr_t *months;
 uintptr_t *days;
 uintptr_t *weekdays;
 uintptr_t *hours;
 uintptr_t *minutes;
 uintptr_t *seconds;

 char *id;
 char *command;
};

struct stree *einit_cron_jobs = NULL;
pthread_mutex_t einit_cron_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

struct einit_cron_job *einit_cron_parse_attrs_to_cron_job (char **attributes) {
 if (!attributes) return NULL;

 struct einit_cron_job *cj = emalloc (sizeof (struct einit_cron_job));
 uint32_t i = 0;
 memset (cj, 0, sizeof (struct einit_cron_job));

 for (; attributes[i]; i+=2) {
  if (strmatch (attributes[i], "command")) {
   cj->command = estrdup(attributes[i+1]);
  } else if (strmatch (attributes[i], "id")) {
   cj->id = estrdup(attributes[i+1]);
  } else if (strmatch (attributes[i], "years") || strmatch (attributes[i], "year")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->years = (uintptr_t *)set_noa_add ((void **)cj->years, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "months") || strmatch (attributes[i], "month")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->months = (uintptr_t *)set_noa_add ((void **)cj->months, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "days") || strmatch (attributes[i], "day")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->days = (uintptr_t *)set_noa_add ((void **)cj->days, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "hours") || strmatch (attributes[i], "hour")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->hours = (uintptr_t *)set_noa_add ((void **)cj->hours, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "minutes") || strmatch (attributes[i], "minute")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->minutes = (uintptr_t *)set_noa_add ((void **)cj->minutes, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "seconds") || strmatch (attributes[i], "second")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    uintptr_t num = parse_integer (x[j]);

    if (num) {
     cj->seconds = (uintptr_t *)set_noa_add ((void **)cj->seconds, (void *)num);
	}
   }

   efree (x);
  } else if (strmatch (attributes[i], "weekdays") || strmatch (attributes[i], "weekday")) {
   char **x = str2set (':', attributes[i+1]);
   uint32_t j = 0;

   for (; x[j]; j++) {
    if (strmatch (x[j], "sunday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)0);
    } else if (strmatch (x[j], "monday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)1);
    } else if (strmatch (x[j], "tuesday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)2);
    } else if (strmatch (x[j], "wednesday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)3);
    } else if (strmatch (x[j], "thursday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)4);
    } else if (strmatch (x[j], "friday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)5);
    } else if (strmatch (x[j], "saturday")) {
     cj->weekdays = (uintptr_t *)set_noa_add ((void **)cj->weekdays, (void *)6);
    }
   }

   efree (x);
  }
 }

 return cj;
}

void einit_cron_einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_configuration_update) {
  struct cfgnode *node = NULL;

//  notice (1, "meow");

  while ((node = cfg_findnode ("services-cron-job", 0, node))) {
   struct einit_cron_job *cj = einit_cron_parse_attrs_to_cron_job (node->arbattrs);

   if (cj->id) {
    emutex_lock (&einit_cron_jobs_mutex);

    struct stree *cur = streefind (einit_cron_jobs, cj->id, tree_find_first);
	if (cur) {
     cur->value = cj;
	 cur->luggage = cj;

     notice (2, "updated job with id=%s", cj->id);
	} else {
	 einit_cron_jobs = streeadd (einit_cron_jobs, cj->id, cj, SET_NOALLOC, cj);

     notice (2, "new job with id=%s", cj->id);
	}

    emutex_unlock (&einit_cron_jobs_mutex);
   } else {
    notice (2, "what's this?");
   }
  }
 }
}

void einit_cron_timer_event_handler (struct einit_event *ev) {
 notice (1, "timer PING");
}

int einit_cron_cleanup (struct lmodule *this) {
 event_ignore (einit_event_subsystem_timer, einit_cron_timer_event_handler);
 event_ignore (einit_event_subsystem_core, einit_cron_einit_event_handler);

 return 0;
}

int einit_cron_enable (void *pa, struct einit_event *status) {
 event_timer_register_timeout (5);
 event_timer_register_timeout (10);
 event_timer_register_timeout (30);
 event_timer_register_timeout (60);

 return status_ok;
}

int einit_cron_disable (void *pa, struct einit_event *status) {
 return status_ok;
}

int einit_cron_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = einit_cron_cleanup;
 thismodule->enable = einit_cron_enable;
 thismodule->disable = einit_cron_disable;

 event_listen (einit_event_subsystem_timer, einit_cron_timer_event_handler);
 event_listen (einit_event_subsystem_core, einit_cron_einit_event_handler);

 return 0;
}
