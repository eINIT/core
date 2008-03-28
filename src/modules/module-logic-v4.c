/*
 *  module-logic-v4.c
 *  einit
 *
 *  Created by Magnus Deininger on 17/12/2007.
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

#include <einit/config.h>
#include <einit/module.h>
#include <einit/event.h>
#include <einit/tree.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <pthread.h>

#include <einit-modules/ipc.h>
#include <string.h>

int einit_module_logic_v4_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_module_logic_v4_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Module Logic Core (V4)",
 .rid       = "einit-module-logic-v4",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_module_logic_v4_configure
};

module_register(einit_module_logic_v4_self);

#endif

struct service_callback {
 char **enable;
 char **disable;
 void (*callback)(void *);
 void *para;
};

extern char shutting_down;
struct stree *module_logic_service_list = NULL;

struct stree **module_logic_free_on_idle_stree = NULL;

struct lmodule **module_logic_broken_modules = NULL; /* this will need to be cleared upon turning idle */
struct lmodule **module_logic_active_modules = NULL;

char **module_logic_list_enable = NULL;
char **module_logic_list_disable = NULL;

int module_logic_list_enable_max_count = 0;
int module_logic_list_disable_max_count = 0;

int module_logic_commit_count = 0;

struct service_callback **module_logic_callbacks = NULL;

pthread_mutex_t
 module_logic_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_free_on_idle_stree_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_broken_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_enable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_disable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_active_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_commit_count_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_callbacks_mutex = PTHREAD_MUTEX_INITIALIZER;

void module_logic_einit_event_handler_core_configuration_update (struct einit_event *);

void module_logic_call_back (char **enable, char **disable, void (*callback)(void *), void *para);
void module_logic_do_callbacks ();

void module_logic_call_back (char **enable, char **disable, void (*callback)(void *), void *para) {
 struct service_callback cb = {
  .enable = enable,
  .disable = disable,
  .callback = callback,
  .para = para
 };

 emutex_lock (&module_logic_callbacks_mutex);
 module_logic_callbacks = (struct service_callback **)set_fix_add ((void **)module_logic_callbacks, &cb, sizeof (cb));
 emutex_unlock (&module_logic_callbacks_mutex);
}

void module_logic_do_callbacks () {
 int i = 0;
 void (*callback)(void *) = NULL;
 void *para = NULL;

 emutex_lock (&module_logic_callbacks_mutex);
 if (module_logic_callbacks) {
  for (; module_logic_callbacks[i]; i++) {
   int y;

   emutex_lock (&module_logic_list_enable_mutex);
   retry_enab:

   if (!module_logic_list_enable) {
    if (module_logic_callbacks[i]->enable) efree (module_logic_callbacks[i]->enable);
    module_logic_callbacks[i]->enable = NULL;
   } else if (module_logic_callbacks[i]->enable) {
    for (y = 0; module_logic_callbacks[i]->enable[y]; y++) {
     if (!inset ((const void **)module_logic_list_enable, module_logic_callbacks[i]->enable[y], SET_TYPE_STRING)) {
      module_logic_callbacks[i]->enable = strsetdel (module_logic_callbacks[i]->enable, module_logic_callbacks[i]->enable[y]);
      goto retry_enab;
     }
    }
   }
   emutex_unlock (&module_logic_list_enable_mutex);

   emutex_lock (&module_logic_list_disable_mutex);
   retry_disab:

   if (!module_logic_list_disable) {
    if (module_logic_callbacks[i]->disable) efree (module_logic_callbacks[i]->disable);
    module_logic_callbacks[i]->disable = NULL;
   } else if (module_logic_callbacks[i]->disable) {
    for (y = 0; module_logic_callbacks[i]->disable[y]; y++) {
     if (!inset ((const void **)module_logic_list_disable, module_logic_callbacks[i]->disable[y], SET_TYPE_STRING)) {
      module_logic_callbacks[i]->disable = strsetdel (module_logic_callbacks[i]->disable, module_logic_callbacks[i]->disable[y]);
      goto retry_disab;
     }
    }
   }
   emutex_unlock (&module_logic_list_disable_mutex);

   if (!module_logic_callbacks[i]->enable && !module_logic_callbacks[i]->disable) {
    callback = module_logic_callbacks[i]->callback;
    para = module_logic_callbacks[i]->para;

    module_logic_callbacks = (struct service_callback **)setdel ((void **)module_logic_callbacks, module_logic_callbacks[i]);
    break;
   }
  }
 }
 emutex_unlock (&module_logic_callbacks_mutex);

 if (callback) {
  callback (para);
  module_logic_do_callbacks();
 }
}

char module_logic_service_exists_p (const char *service) {
 char rv = 0;
 emutex_lock (&module_logic_service_list_mutex);
 rv = (module_logic_service_list && streefind (module_logic_service_list, service, tree_find_first)) ? 1 : 0;
 emutex_unlock (&module_logic_service_list_mutex);
 return rv;
}

/* the sorting bit and the ipc handler are pretty much verbatim copies of -v3 */

void mod_sort_service_list_items_by_preference() {
 struct stree *cur;

 emutex_lock (&module_logic_service_list_mutex);

 cur = streelinear_prepare(module_logic_service_list);

 while (cur) {
  struct lmodule **lm = (struct lmodule **)cur->value;

  if (lm) {
/* order modules that should be enabled according to the user's preference */
   uint32_t mpx, mpy, mpz = 0;
   char *pnode = NULL, **preference = NULL;

/* first make sure all modules marked as "deprecated" are last */
   for (mpx = 0; lm[mpx]; mpx++); mpx--;
   for (mpy = 0; mpy < mpx; mpy++) {
    if (lm[mpy]->module && (lm[mpy]->module->mode & einit_module_deprecated)) {
     struct lmodule *t = lm[mpx];
     lm[mpx] = lm[mpy];
     lm[mpy] = t;
     mpx--;
    }
   }

/* now to the sorting bit... */
/* step 1: sort everything using <services-prefer></services-prefer> nodes */
   pnode = emalloc (strlen (cur->key)+18);
   pnode[0] = 0;
   strcat (pnode, "services-prefer-");
   strcat (pnode, cur->key);

   if ((preference = str2set (':', cfg_getstring (pnode, NULL)))) {
    for (mpx = 0; preference[mpx]; mpx++) {
     for (mpy = 0; lm[mpy]; mpy++) {
      if (lm[mpy]->module && lm[mpy]->module->rid && strmatch(lm[mpy]->module->rid, preference[mpx])) {
       struct lmodule *tm = lm[mpy];

       lm[mpy] = lm[mpz];
       lm[mpz] = tm;

       mpz++;
      }
     }
    }
    efree (preference);
   }

   efree (pnode);
  }

  cur = streenext(cur);
 }

 emutex_unlock (&module_logic_service_list_mutex);
}

/* callers of the following couple o' functions need to lock the appropriate mutex on their own! */

struct lmodule *module_logic_get_prime_candidate (struct lmodule **lm) {
 char do_rotate;
 struct lmodule *primus = lm[0];

/* here we should rotate lm[] to make the first entry one entry that is high up in the preference list, but also not blocked */
 do {
  emutex_lock (&module_logic_broken_modules_mutex);
  do_rotate = inset ((const void **)module_logic_broken_modules, lm[0], SET_NOALLOC);
  emutex_unlock (&module_logic_broken_modules_mutex);

  if (do_rotate) {
   int y = 0;

   struct lmodule *secundus = lm[0];
   for (; lm[y+1]; y++) {
    lm[y] = lm[y+1];
   }
   lm[y] = secundus;

   if (lm[0] == primus) {
    return NULL;
   }
  } else {
   return lm[0];
  }
 } while (do_rotate);

 return NULL;
}

char module_logic_check_for_circular_dependencies (char *service, struct lmodule **dependencies) {
 struct stree *st = streefind(module_logic_service_list, service, tree_find_first);

 if (st) {
  struct lmodule *primus = module_logic_get_prime_candidate(st->value);

  if (inset ((const void **)dependencies, primus, SET_NOALLOC)) {
/* this 'ere means that we found a circular dep... */
   notice (1, "module %s: CIRCULAR DEPENDENCY DETECTED!", (primus->module && primus->module->rid) ? primus->module->rid : "");

   emutex_lock (&module_logic_broken_modules_mutex);
   if (!inset ((const void **)module_logic_broken_modules, primus, SET_NOALLOC))
    module_logic_broken_modules = (struct lmodule **)set_noa_add ((void **)module_logic_broken_modules, primus);
   emutex_unlock (&module_logic_broken_modules_mutex);

   return 1;
  }

  if (primus && primus->si && primus->si->requires) {
   int y = 0;
   struct lmodule **subdeps = (struct lmodule **)set_noa_add (set_noa_dup (dependencies), primus);

   for (; primus->si->requires[y]; y++) {
    if (module_logic_check_for_circular_dependencies (primus->si->requires[y], subdeps)) {
     if (subdeps)
      efree (subdeps);

     return 1;
    }
   }

   if (subdeps)
    efree (subdeps);
  } else
   return 0;
 }

 return 0;
}

/* this is the function that can figure out what to enable */

struct lmodule **module_logic_find_things_to_enable() {
 if (!module_logic_list_enable) {
  return NULL;
 }

 struct lmodule **rv = NULL;

 struct lmodule **candidates_level1 = NULL;
 struct lmodule **candidates_level2 = NULL;
 int i = 0;
 char **unresolved = NULL;
 char **broken = NULL;
 char **services_level1 = NULL;

 reeval_top:

 emutex_lock (&module_logic_service_list_mutex);

 for (i = 0; module_logic_list_enable[i]; i++) {
  if (i == 0) {
   int c = setcount ((const void **)module_logic_list_enable);
   module_logic_list_enable_max_count = (module_logic_list_enable_max_count > c) ? module_logic_list_enable_max_count : c;
  }

  if (mod_service_is_provided(module_logic_list_enable[i])) {
   module_logic_list_enable = strsetdel (module_logic_list_enable, module_logic_list_enable[i]);
   i--;
   if (!module_logic_list_enable) break;
   continue;
  }

  if (!inset ((const void **)services_level1, module_logic_list_enable[i], SET_TYPE_STRING))
   services_level1 = set_str_add_stable (services_level1, module_logic_list_enable[i]);

  struct stree *st = streefind(module_logic_service_list, module_logic_list_enable[i], tree_find_first);

  if (st) {
   struct lmodule **lm = st->value;

   if (!lm[1] && lm[0]->module && lm[0]->module->rid && strmatch (lm[0]->module->rid, module_logic_list_enable[i]) && (lm[0]->status & status_enabled)) {
/* this check prevents us from trying to enable a module specified via its RID twice */

    module_logic_list_enable = strsetdel (module_logic_list_enable, module_logic_list_enable[i]);
    i = -1;

    if (candidates_level1) {
     efree (candidates_level1);
     candidates_level1 = NULL;
    }
    if (services_level1) {
     efree (services_level1);
     services_level1 = NULL;
    }

    if (!module_logic_list_enable) break;
    continue;
   }

   struct lmodule *candidate = module_logic_get_prime_candidate (lm);

   if (!candidate) {
    broken = set_str_add_stable (broken, module_logic_list_enable[i]);
    module_logic_list_enable = strsetdel (module_logic_list_enable, module_logic_list_enable[i]);
    i = -1;

    if (candidates_level1) {
     efree (candidates_level1);
     candidates_level1 = NULL;
    }
    if (services_level1) {
     efree (services_level1);
     services_level1 = NULL;
    }

    if (!module_logic_list_enable) break;
    continue;
   }

   if (candidate->si && candidate->si->uses) {
    int y = 0;

    for (y = 0; candidate->si->uses[y]; y++) {
     if ((!broken || !inset ((const void **)broken, candidate->si->uses[y], SET_TYPE_STRING)) && !inset ((const void **)module_logic_list_enable, candidate->si->uses[y], SET_TYPE_STRING)) {
      module_logic_list_enable = set_str_add_stable (module_logic_list_enable, candidate->si->uses[y]);

      if (candidates_level1) {
       efree (candidates_level1);
       candidates_level1 = NULL;
      }
      if (services_level1) {
       efree (services_level1);
       services_level1 = NULL;
      }

      i = -1;
     }
    }

    if (i == -1) continue;
   }

   if (mod_service_requirements_met(candidate)) {
    candidates_level1 = (struct lmodule **)set_noa_add ((void **)candidates_level1, candidate);

    if (candidate->module) {
     if (candidate->module->rid) {
      if (!inset ((const void **)services_level1, candidate->module->rid, SET_TYPE_STRING))
       services_level1 = set_str_add_stable (services_level1, candidate->module->rid);
     }
     if (candidate->si && candidate->si->provides) {
      int j = 0;
      for (; candidate->si->provides[j]; j++) {
       if (!inset ((const void **)services_level1, candidate->si->provides[j], SET_TYPE_STRING))
        services_level1 = set_str_add_stable (services_level1, candidate->si->provides[j]);
      }
     }
    }
   } else {
    /* need to add stuff that is still needed somewhere... */
#if 0
    fprintf (stderr, "nyu?: %s\n", candidate->module->rid);
    fflush (stderr);
#endif

    if (candidate->si && candidate->si->requires) {
     int y = 0;
     char impossible = 0;

     for (y = 0; candidate->si->requires[y]; y++) {
      if (broken && inset ((const void **)broken, candidate->si->requires[y], SET_TYPE_STRING)) {
       impossible = 1;
      }
     }

     if (impossible) {
#if 0
      fprintf (stderr, "impossible: %s\n", candidate->module->rid);
      fflush (stderr);
#endif

      emutex_lock (&module_logic_broken_modules_mutex);
      module_logic_broken_modules = (struct lmodule **)set_noa_add ((void **)module_logic_broken_modules, candidate);
      emutex_unlock (&module_logic_broken_modules_mutex);

      i--;
      goto next;
     } else {
      for (y = 0; candidate->si->requires[y]; y++) {
       if (!inset ((const void **)module_logic_list_enable, candidate->si->requires[y], SET_TYPE_STRING)) {
#if 0
        fprintf (stderr, " !! %s needs: %s\n", candidate->module->rid, candidate->si->requires[y]);
        fflush (stderr);
#endif

        module_logic_list_enable = set_str_add_stable (module_logic_list_enable, candidate->si->requires[y]);

        if (candidates_level1) {
         efree (candidates_level1);
         candidates_level1 = NULL;
        }
        if (services_level1) {
         efree (services_level1);
         services_level1 = NULL;
        }

        i = -1;
       }
      }
     }
    }
   }
  } else {
   unresolved = set_str_add_stable (unresolved, module_logic_list_enable[i]);
  }

  next: ;
 }
 emutex_unlock (&module_logic_service_list_mutex);

 if (broken) {
  struct einit_event ee = evstaticinit(einit_feedback_broken_services);
  ee.set = (void **)broken;
  ee.stringset = broken;

  event_emit (&ee, einit_event_flag_broadcast);
  evstaticdestroy (ee);

  efree (broken);
  broken = NULL;
 }

 if (unresolved) {
  char all = 1;
  for (i = 0; module_logic_list_enable[i]; i++) {
   if (!inset ((const void **)unresolved, module_logic_list_enable[i], SET_TYPE_STRING)) {
    all = 0;
    break;
   }
  }

  if (all) { /* everything is unresolved, which means we can't do any progress... */
   struct einit_event ee = evstaticinit(einit_feedback_unresolved_services);
   ee.set = (void **)unresolved;
   ee.stringset = unresolved;

   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);

   efree (module_logic_list_enable);
   module_logic_list_enable = NULL;
  }

  efree (unresolved);
  unresolved = NULL;
 }

/* this is where we should try to add some code that detects cyclic dependencies... */
/* ... come to think of it, it'd be better to add that code to the part that creates the service list ...
   or both */
#if 1
// if (!candidates_level1) {
  if (module_logic_list_enable) {
   for (i = 0; module_logic_list_enable[i]; i++) {
    char iscircular;

    emutex_lock (&module_logic_service_list_mutex);
	iscircular = module_logic_check_for_circular_dependencies (module_logic_list_enable[i], NULL);
    emutex_unlock (&module_logic_service_list_mutex);

    if (iscircular) {
     if (candidates_level1) {
      efree (candidates_level1);
      candidates_level1 = NULL;
     }
     if (services_level1) {
      efree (services_level1);
      services_level1 = NULL;
     }

     goto reeval_top;
    }
   }
  }
// }
#endif

/* here we need to filter the level1 list a bit, in order to remove modules that provide services that other modules already provide */
/* example: we have logger and syslog in the specs, logger resolves to v-metalog and syslog to v-syslog... then we want v-syslog to be used,
   because v-syslog provides both syslog and logger, but v-metalog only provides logger. */
/* here'd also be a good place to remove dupes */
 reeval:

 if (candidates_level1 && candidates_level1[1] && module_logic_list_enable) {
  for (i = 0; candidates_level1[i]; i++) {
   char **mdata = NULL;
   int i_in_target = 0;

   if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
    mdata = set_str_dup_stable (candidates_level1[i]->si->provides);

    int k;
    for (k = 0; candidates_level1[i]->si->provides[k]; k++) {
     if (inset ((const void **)module_logic_list_enable, candidates_level1[i]->si->provides[k], SET_TYPE_STRING)) {
      i_in_target++;
     }
    }
   }

   if (candidates_level1[i]->module && candidates_level1[i]->module->rid) {
    mdata = set_str_add_stable (mdata, candidates_level1[i]->module->rid);

    if (inset ((const void **)module_logic_list_enable, candidates_level1[i]->module->rid, SET_TYPE_STRING)) {
     i_in_target++;
    }
   }

   if (mdata) {
    int j;
    for (j = 0; candidates_level1[j]; j++) if (candidates_level1[j] != candidates_level1[i]) {
     char clash = 0;
     int j_in_target = 0;

     if (candidates_level1[j]->si && candidates_level1[j]->si->provides) {
      int k;
      for (k = 0; candidates_level1[j]->si->provides[k]; k++) {
       if (inset ((const void **)mdata, candidates_level1[j]->si->provides[k], SET_TYPE_STRING)) {
        clash = 1;
       }

       if (inset ((const void **)module_logic_list_enable, candidates_level1[j]->si->provides[k], SET_TYPE_STRING)) {
        j_in_target++;
       }
      }
     }

     if (!clash && candidates_level1[j]->si && candidates_level1[j]->module->rid) {
      if (inset ((const void **)mdata, candidates_level1[j]->module->rid, SET_TYPE_STRING)) {
       clash = 1;
      }

      if (inset ((const void **)module_logic_list_enable, candidates_level1[j]->module->rid, SET_TYPE_STRING)) {
       j_in_target++;
      }
     }

     if (clash) {
/* whoops, we got two modules with overlapping things that they provide */
      if (i_in_target > j_in_target) {
       /* use candidates_level1[i] */
       candidates_level1 = (struct lmodule **)setdel ((void **)candidates_level1, candidates_level1[j]);
       goto reeval;
      } else if (j_in_target > j_in_target) {
       /* use candidates_level1[j] */
       candidates_level1 = (struct lmodule **)setdel ((void **)candidates_level1, candidates_level1[i]);
       goto reeval;
      } else {
       /* hmph... better leave things as they are... */
      }
     }
    }

    efree (mdata);
   }
  }
 }

/* now we apply before/after specs */
 if (candidates_level1 && services_level1) {
  for (i = 0; candidates_level1[i]; i++) {
   char doskip = 0;

   if (candidates_level1[i]->si) {
/* after= ... this one's easy... */
    if (candidates_level1[i]->si->after) {
     int j = 0;

/* remove our own services so we don't accidentally match ourselves */
     if (candidates_level1[i]->module->rid) {
      services_level1 = strsetdel (services_level1, candidates_level1[i]->module->rid);
     }
     if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
      int k = 0;
      for (; candidates_level1[i]->si->provides[k]; k++) {
       services_level1 = strsetdel (services_level1, candidates_level1[i]->si->provides[k]);
      }
     }

     for (; candidates_level1[i]->si->after[j]; j++) {
      if (inset_pattern ((const void **)services_level1, candidates_level1[i]->si->after[j], SET_TYPE_STRING)) {
       /* reaching this branch means that there's something that we /could/ enable, but there's something else to be enabled that requests being enabled before this 'ere */
       doskip = 1;
       break;
      }
     }

/* re-add our own services */
     if (candidates_level1[i]->module->rid) {
      if (!inset ((const void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING))
       services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->module->rid);
     }
     if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
      int k = 0;
      for (; candidates_level1[i]->si->provides[k]; k++) {
       if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING))
        services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->si->provides[k]);
      }
     }
    }
   }

   if (!doskip) {
    candidates_level2 = (struct lmodule **)set_noa_add ((void **)candidates_level2, candidates_level1[i]);
   } else {
#if 0
    fprintf (stderr, " !! skipping: %s\n", candidates_level1[i]->module->rid);
    fflush (stderr);
#endif
   }
  }

  if (candidates_level2) {
   /* before= is a bitch */
   for (i = 0; candidates_level2[i]; i++) {
    if (candidates_level2[i]->si && candidates_level2[i]->si->before) {
     int y = 0;
     for (; candidates_level2[y]; y++) {
      if (candidates_level2[y] != candidates_level2[i]) {
       char **matchthis = (candidates_level2[y]->si && candidates_level2[y]->si->provides) ? set_str_dup_stable (candidates_level2[y]->si->provides) : NULL;
       if (candidates_level2[y]->module && candidates_level2[y]->module->rid) {
        matchthis = set_str_add_stable (matchthis, candidates_level2[y]->module->rid);
       }

       if (matchthis) {
        int j = 0;
        char doskip = 0;
        for (; candidates_level2[i]->si->before[j]; j++) {
          if (inset_pattern ((const void **)matchthis, candidates_level2[i]->si->before[j], SET_TYPE_STRING)) {
          doskip = 1;
          break;
         }
        }

        if (doskip) {
#if 0
         fprintf (stderr, " !! skipping: %s\n", candidates_level2[y]->module->rid);
         fflush (stderr);
#endif

         candidates_level2 = (struct lmodule **)setdel ((void **)candidates_level2, candidates_level2[y]);

         i = -1;
         efree (matchthis);
         break;
        }

        efree (matchthis);
       }
      }
     }
    }
   }
  }

  if (!candidates_level2) {
   /* at this point we'd pretty much need to panic... it means we hit some recursive deps due to before/after abuse */
   notice (2, "WARNING: before/after abuse detected! trying to enable /something/ anyway");

   candidates_level2 = candidates_level1;
//   efree (candidates_level1);
//   candidates_level1 = candidates_level2;
  } else {
   efree (candidates_level1);
  }
 }

 if (candidates_level2) {
  for (i = 0; candidates_level2[i]; i++) {
   char doskip = 0;

   emutex_lock (&module_logic_active_modules_mutex);
   if (module_logic_active_modules && inset ((const void **)module_logic_active_modules, candidates_level2[i], SET_NOALLOC)) {
    doskip = 1;
   }
   emutex_unlock (&module_logic_active_modules_mutex);

   if (!doskip) {
    rv = (struct lmodule **)set_noa_add ((void **)rv, candidates_level2[i]);
    emutex_lock (&module_logic_active_modules_mutex);
    module_logic_active_modules = (struct lmodule **)set_noa_add ((void **)module_logic_active_modules, candidates_level2[i]);
    emutex_unlock (&module_logic_active_modules_mutex);
   }
  }

  efree (candidates_level2);
 }

 if (services_level1) efree (services_level1);

 return rv;
}

/* this is the function that can figure out what to disable */

struct lmodule **module_logic_find_things_to_disable() {
 if (!module_logic_list_disable) return NULL;

 struct lmodule **rv = NULL;

 struct lmodule **candidates_level1 = NULL;
 struct lmodule **candidates_level2 = NULL;
 int i = 0;
 char **unresolved = NULL;
 char **services_level1 = NULL;

 reeval_top:

 if (module_logic_list_disable) {
  struct lmodule **pot = mod_list_all_enabled_modules();

  if (!pot) {
/* nothing enabled... nothing to do */
   efree (module_logic_list_disable);
   module_logic_list_disable = NULL;
   return NULL;
  }

  unresolved = set_str_dup_stable (module_logic_list_disable);

  for (i = 0; pot[i]; i++) {
//   mod_service_not_in_use (pot[i]);
   char doadd = 0;

   if (pot[i]->module->rid) {
    if (inset ((const void **)module_logic_list_disable, pot[i]->module->rid, SET_TYPE_STRING)) {
     doadd = 1;
    }
   }
   if (!doadd) {
    if (pot[i]->si && pot[i]->si->provides) {
     int y = 0;
     for (; pot[i]->si->provides[y]; y++) {
      if (inset ((const void **)module_logic_list_disable, pot[i]->si->provides[y], SET_TYPE_STRING)) {
       doadd = 1;
       break;
      }
     }
    }
   }

   if (doadd) {
    candidates_level1 = (struct lmodule **)set_noa_add ((void **)candidates_level1, pot[i]);

    if (pot[i]->module->rid) {
     unresolved = strsetdel (unresolved, pot[i]->module->rid);
     services_level1 = set_str_add_stable (services_level1, pot[i]->module->rid);
    }
    if (pot[i]->si && pot[i]->si->provides) {
     int y = 0;
     for (; pot[i]->si->provides[y]; y++) {
      unresolved = strsetdel (unresolved, pot[i]->si->provides[y]);
      services_level1 = set_str_add_stable (services_level1, pot[i]->si->provides[y]);
     }
    }
   }
  }

  efree (pot);
 } else {
  return NULL;
 }

/* make sure we disable everything that is using one of our candidates first */

 if (candidates_level1) {
  reeval:

  for (i = 0; candidates_level1[i]; i++) {
   struct lmodule **users = mod_get_all_users (candidates_level1[i]);

   if (users) {
    int j = 0;
    char impossible = 0;

    for (j = 0; users[j]; j++) {
     emutex_lock (&module_logic_broken_modules_mutex);
     impossible = inset ((const void **)module_logic_broken_modules, users[j], SET_NOALLOC);
     emutex_unlock (&module_logic_broken_modules_mutex);

     if (impossible)
      break;
    }

    if (impossible) {
     emutex_lock (&module_logic_broken_modules_mutex);
     module_logic_broken_modules = (struct lmodule **)set_noa_add ((void **)module_logic_broken_modules, candidates_level1[i]);
     emutex_unlock (&module_logic_broken_modules_mutex);

     efree (candidates_level1);
     candidates_level1 = NULL;

     efree (users);
     goto reeval_top;
    } else {
     candidates_level1 = (struct lmodule **)setdel ((void **)candidates_level1, candidates_level1[i]);
     for (j = 0; users[j]; j++) {
      candidates_level1 = (struct lmodule **)set_noa_add ((void **)candidates_level1, users[j]);
     }

     efree (users);
    }

    goto reeval;
   }
  }
 }

 if (unresolved) {
  char all = 1;
  for (i = 0; module_logic_list_disable[i]; i++) {
   if (!inset ((const void **)unresolved, module_logic_list_disable[i], SET_TYPE_STRING)) {
    all = 0;
    break;
   }
  }

  if (all) { /* everything is unresolved, which means we can't do any progress... */
/*   struct einit_event ee = evstaticinit(einit_feedback_unresolved_services);
   ee.set = (void **)unresolved;
   ee.stringset = unresolved;

   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);*/

   efree (module_logic_list_disable);
   module_logic_list_disable = NULL;
  }

  efree (unresolved);
 }

 if (!candidates_level1 && module_logic_list_disable) {
  char *n = set2str (':', (const char **)module_logic_list_disable);
  notice (1, "nothing to do? still need to disable: %s; retrying...\n", n);
  efree (n);

  goto reeval_top;
 }

 if (candidates_level1) {
  for (i = 0; candidates_level1[i]; i++) {
   if (candidates_level1[i]->module) {
    if (candidates_level1[i]->module->rid) {
     if (!inset ((const void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING))
      services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->module->rid);
    }
    if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
     int j = 0;
     for (; candidates_level1[i]->si->provides[j]; j++) {
      if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[j], SET_TYPE_STRING))
       services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->si->provides[j]);
     }
    }
   }
  }

/* the reorder-phase for things to disable is pretty much precisely the enable phase's but with before/after switched... */
  if (services_level1) {
   for (i = 0; candidates_level1[i]; i++) {
    char doskip = 0;

    if (candidates_level1[i]->si) {
     if (candidates_level1[i]->si->before) {
      int j = 0;

/* remove our own services so we don't accidentally match ourselves */
      if (candidates_level1[i]->module->rid) {
       services_level1 = strsetdel (services_level1, candidates_level1[i]->module->rid);
      }
      if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
       int k = 0;
       for (; candidates_level1[i]->si->provides[k]; k++) {
        services_level1 = strsetdel (services_level1, candidates_level1[i]->si->provides[k]);
       }
      }

      for (; candidates_level1[i]->si->before[j]; j++) {
       if (inset_pattern ((const void **)services_level1, candidates_level1[i]->si->before[j], SET_TYPE_STRING)) {
        doskip = 1;
        break;
       }
      }

/* re-add our own services */
      if (candidates_level1[i]->module->rid) {
       if (!inset ((const void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING))
        services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->module->rid);
      }
      if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
       int k = 0;
       for (; candidates_level1[i]->si->provides[k]; k++) {
        if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING))
         services_level1 = set_str_add_stable (services_level1, candidates_level1[i]->si->provides[k]);
       }
      }
     }
    }

    if (!doskip) {
     candidates_level2 = (struct lmodule **)set_noa_add ((void **)candidates_level2, candidates_level1[i]);
    } else {
#if 0
     fprintf (stderr, " !! skipping: %s\n", candidates_level1[i]->module->rid);
     fflush (stderr);
#endif
    }
   }

/* after= */
   if (candidates_level2) {
    for (i = 0; candidates_level2[i]; i++) {
     if (candidates_level2[i]->si && candidates_level2[i]->si->after) {
      int y = 0;
      for (; candidates_level2[y]; y++) {
       if (candidates_level2[y] != candidates_level2[i]) {
        char **matchthis = (candidates_level2[y]->si && candidates_level2[y]->si->provides) ? set_str_dup_stable (candidates_level2[y]->si->provides) : NULL;
        if (candidates_level2[y]->module && candidates_level2[y]->module->rid) {
         matchthis = set_str_add_stable (matchthis, candidates_level2[y]->module->rid);
        }

        if (matchthis) {
         int j = 0;
         char doskip = 0;
         for (; candidates_level2[i]->si->after[j]; j++) {
          if (inset_pattern ((const void **)matchthis, candidates_level2[i]->si->after[j], SET_TYPE_STRING)) {
           doskip = 1;
           break;
          }
         }

         if (doskip) {
#if 0
          fprintf (stderr, " !! skipping: %s\n", candidates_level2[y]->module->rid);
          fflush (stderr);
#endif


          candidates_level2 = (struct lmodule **)setdel ((void **)candidates_level2, candidates_level2[y]);

          i = -1;
          efree (matchthis);
          break;
         }

         efree (matchthis);
        }
       }
      }
     }
    }
   }

   if (!candidates_level2) {
    /* at this point we'd pretty much need to panic... it means we hit some recursive deps due to before/after abuse */
    notice (2, "WARNING: before/after abuse detected! trying to disable /something/ anyway");

    candidates_level2 = candidates_level1;
//    efree (candidates_level1);
   } else {
    efree (candidates_level1);
   }

   efree (services_level1);
  }
 }

 if (candidates_level2) {
  for (i = 0; candidates_level2[i]; i++) {
   char doskip = 0;

   emutex_lock (&module_logic_active_modules_mutex);
   if (module_logic_active_modules && inset ((const void **)module_logic_active_modules, candidates_level2[i], SET_NOALLOC)) {
    doskip = 1;

#if 0
    fprintf (stderr, "skipping: %s\n", candidates_level2[i]->module->rid);
    fflush (stderr);
#endif
   }
   emutex_unlock (&module_logic_active_modules_mutex);

   if (!doskip) {
    rv = (struct lmodule **)set_noa_add ((void **)rv, candidates_level2[i]);
    emutex_lock (&module_logic_active_modules_mutex);
    module_logic_active_modules = (struct lmodule **)set_noa_add ((void **)module_logic_active_modules, candidates_level2[i]);
    emutex_unlock (&module_logic_active_modules_mutex);
   }
  }

  efree (candidates_level2);
 }

 return rv;
}

void *module_logic_do_enable (void *module) {
 mod (einit_module_enable, module, NULL);
 return NULL;
}

void *module_logic_do_disable (void *module) {
 mod (einit_module_disable, module, NULL);
 return NULL;
}

void module_logic_spawn_set_enable (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  if (spawn[i+1]) {
   if (spawn[i]->module->mode & (einit_module_event_actions | einit_module_fork_actions))
    mod (einit_module_enable, spawn[i], NULL);
   else
    ethread_spawn_detached_run (module_logic_do_enable, spawn[i]);
  } else {
   mod (einit_module_enable, spawn[i], NULL);
  }
 }
}

void module_logic_spawn_set_enable_all (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  if (spawn[i]->module->mode & (einit_module_event_actions | einit_module_fork_actions))
   mod (einit_module_enable, spawn[i], NULL);
  else
   ethread_spawn_detached_run (module_logic_do_enable, spawn[i]);
 }
}

void module_logic_spawn_set_disable (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  if (spawn[i+1]) {
   if (spawn[i]->module->mode & einit_module_event_actions)
    mod (einit_module_disable, spawn[i], NULL);
   else
    ethread_spawn_detached_run (module_logic_do_disable, spawn[i]);
  } else {
   mod (einit_module_disable, spawn[i], NULL);
  }
 }
}

void module_logic_spawn_set_disable_all (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  if (spawn[i]->module->mode & einit_module_event_actions)
   mod (einit_module_disable, spawn[i], NULL);
  else
   ethread_spawn_detached_run (module_logic_do_disable, spawn[i]);
 }
}

/* in the following event handler, we (re-)build our service-name -> module(s) lookup table */
void module_logic_einit_event_handler_core_module_list_update (struct einit_event *ev) {
 struct stree *new_service_list = NULL;
 struct lmodule *cur = ev->para;

 while (cur) {
  if (cur->module && cur->module->rid) {
   struct lmodule **t = (struct lmodule **)set_noa_add ((void **)NULL, cur);

/* no need to check here, 'cause rids are required to be unique */
   new_service_list = streeadd (new_service_list, cur->module->rid, (void *)t, SET_NOALLOC, (void *)t);
  }

  if (cur->si && cur->si->provides) {
   ssize_t i = 0;

   for (; cur->si->provides[i]; i++) {
    struct stree *slnode = new_service_list ?
     streefind (new_service_list, cur->si->provides[i], tree_find_first) :
     NULL;
    struct lnode **curval = (struct lnode **) (slnode ? slnode->value : NULL);

    curval = (struct lnode **)set_noa_add ((void **)curval, cur);

    if (slnode) {
     slnode->value = curval;
     slnode->luggage = curval;
    } else {
     new_service_list = streeadd (new_service_list, cur->si->provides[i], (void *)curval, SET_NOALLOC, (void *)curval);
    }
   }
  }
  cur = cur->next;
 }

 emutex_lock (&module_logic_service_list_mutex);

 struct stree *old_service_list = module_logic_service_list;
 module_logic_service_list = new_service_list;
 emutex_unlock (&module_logic_service_list_mutex);

 /* I'll need to free this later... */
 emutex_lock (&module_logic_free_on_idle_stree_mutex);
 module_logic_free_on_idle_stree = (struct stree **)set_noa_add ((void **)module_logic_free_on_idle_stree, old_service_list);
 emutex_unlock (&module_logic_free_on_idle_stree_mutex);

 /* updating the list of modules does mean we also need re-apply preferences... */
 mod_sort_service_list_items_by_preference();
}

/* what we also need is a list of preferences... */
void module_logic_einit_event_handler_core_configuration_update (struct einit_event *ev) {
 mod_sort_service_list_items_by_preference();
}

/* the following three events are used to make the module logics core do something */
struct cfgnode *module_logic_prepare_mode_switch (char *modename, char ***enable_r, char ***disable_r) {
 if (!modename) return NULL;

 struct cfgnode *mode = cfg_findnode (modename, einit_node_mode, NULL);

 if (!mode) {
  return NULL;
 }

 char **enable = *enable_r, **disable = *disable_r;

 char **tmplist;
 if ((tmplist = str2set (':', cfg_getstring ("enable/services", mode)))) {
  int i = 0;
  for (; tmplist[i]; i++) {
   if (!enable || !inset ((const void **)enable, tmplist[i], SET_TYPE_STRING)) {
    enable = set_str_add_stable (enable, tmplist[i]);
   }
  }

  efree (tmplist);
 }

 if ((tmplist = str2set (':', cfg_getstring ("disable/services", mode)))) {
  int i = 0;
  for (; tmplist[i]; i++) {
   if (!disable || !inset ((const void **)disable, tmplist[i], SET_TYPE_STRING)) {
    disable = set_str_add_stable (disable, tmplist[i]);
   }
  }

  efree (tmplist);
 }

 if (strmatch (modename, "power-down") || strmatch (modename, "power-reset")) {
  shutting_down = 1;
 }

 if (mode->arbattrs) {
  int i = 0;
  char **base = NULL;

  for (; mode->arbattrs[i]; i+=2) {
   if (strmatch(mode->arbattrs[i], "base")) {
    base = str2set (':', mode->arbattrs[i+1]);
   } else if (strmatch (mode->arbattrs[i], "wait-for-base") && parse_boolean (mode->arbattrs[i+1])) {
    char tmp[BUFFERSIZE];

    esprintf (tmp, BUFFERSIZE, "checkpoint-mode-%s", mode->id);

    if (!enable || !inset ((const void **)enable, tmp, SET_TYPE_STRING))
     enable = set_str_add_stable (enable, tmp);
   }
  }

  if (base) {
   for (i = 0; base[i]; i++) {
    module_logic_prepare_mode_switch (base[i], &enable, &disable);
   }
  }
 }

 if (disable) {
  char disable_all = inset ((const void **)disable, (void *)"all", SET_TYPE_STRING);

  if (disable_all) {
   char **tmp = mod_list_all_provided_services();

   if (disable) {
    efree (disable);
    disable = NULL;
   }

   if (tmp) {
    int i = 0;
    for (; tmp[i]; i++) {
     char add = 1;

     if (inset ((const void **)disable, (void *)tmp[i], SET_TYPE_STRING)) {
      add = 0;
     } else if (disable_all && strmatch(tmp[i], "all")) {
      add = 0;
     }

     if (add) {
      disable = set_str_add_stable(disable, (void *)tmp[i]);
     }
    }

    efree (tmp);
   }
  }
 }

 *enable_r = enable;
 *disable_r = disable;

 return mode;
}

void module_logic_idle_actions () {
 module_logic_list_enable_max_count = 0;
 module_logic_list_disable_max_count = 0;

 emutex_lock (&module_logic_broken_modules_mutex);
 if (module_logic_broken_modules)
  efree (module_logic_broken_modules);

 module_logic_broken_modules = NULL;
 emutex_unlock (&module_logic_broken_modules_mutex);

 emutex_lock (&module_logic_free_on_idle_stree_mutex);
 if (module_logic_free_on_idle_stree) {
  int i = 0;
  for (; module_logic_free_on_idle_stree[i]; i++) {
   streefree (module_logic_free_on_idle_stree[i]);
  }

  efree (module_logic_free_on_idle_stree);
 }

 module_logic_free_on_idle_stree = NULL;
 emutex_unlock (&module_logic_free_on_idle_stree_mutex);
}

struct mode_switch_callback_data {
 char *modename;
 struct cfgnode *mode;
};

void module_logic_einit_event_handler_core_switch_mode_callback (void *p) {
 struct mode_switch_callback_data *d = p;
 char sw;

 /* waiting on things to be enabled and disabled works just fine if we do it serially... */
 if (d->mode) {
  cmode = d->mode;
  amode = d->mode;

  if (strmatch (d->modename, "power-down")) {
   struct einit_event ee = evstaticinit (einit_power_down_imminent);
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);
  } else if (strmatch (d->modename, "power-reset")) {
   struct einit_event ee = evstaticinit (einit_power_reset_imminent);
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);
  }

  struct einit_event eex = evstaticinit (einit_core_mode_switch_done);
  eex.para = (void *)d->mode;
  eex.string = d->mode->id;
  event_emit (&eex, einit_event_flag_broadcast);
  evstaticdestroy (eex);
 }

 emutex_lock (&module_logic_commit_count_mutex);
 module_logic_commit_count--;
 sw = (module_logic_commit_count == 0);
 emutex_unlock (&module_logic_commit_count_mutex);

 if (sw) {
  struct einit_event evs = evstaticinit (einit_core_done_switching);
  event_emit (&evs, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy (evs);

  module_logic_idle_actions();
 }

 if (d->modename && (strmatch (d->modename, "power-down") || strmatch (d->modename, "power-reset"))) {
  usleep (50000);
  /* at this point, we really should be dead already... */

  notice (1, "exiting");
  _exit (0);
 }

 efree (d);
}


void module_logic_einit_event_handler_core_switch_mode (struct einit_event *ev) {
 char sw;

 emutex_lock (&module_logic_commit_count_mutex);
 sw = (module_logic_commit_count == 0);
 module_logic_commit_count++;
 emutex_unlock (&module_logic_commit_count_mutex);

 if (sw) {
  mod_sort_service_list_items_by_preference();

  struct einit_event evs = evstaticinit (einit_core_switching);
  event_emit (&evs, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy (evs);
 }

 if (ev->string) {
  char **enable = NULL, **disable = NULL;

  struct cfgnode *mode = module_logic_prepare_mode_switch (ev->string, &enable, &disable);

  if (mode) {
   cmode = mode;

   struct einit_event eex = evstaticinit (einit_core_mode_switching);
   eex.para = (void *)mode;
   eex.string = mode->id;
   event_emit (&eex, einit_event_flag_broadcast);
   evstaticdestroy (eex);

   if (strmatch (ev->string, "power-down")) {
    struct einit_event ee = evstaticinit (einit_power_down_scheduled);
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy (ee);
   } else if (strmatch (ev->string, "power-reset")) {
    struct einit_event ee = evstaticinit (einit_power_reset_scheduled);
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy (ee);
   }
  }

  if (enable) {
   int i = 0;
   emutex_lock (&module_logic_list_enable_mutex);
   for (; enable[i]; i++) {
    if (!inset ((const void **)module_logic_list_enable, enable[i], SET_TYPE_STRING))
     module_logic_list_enable = set_str_add_stable (module_logic_list_enable, enable[i]);
   }

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn) {
    if (disable) {
     module_logic_spawn_set_enable_all (spawn);
    } else { /* re-use this thread if there's nothing to disable */
     module_logic_spawn_set_enable (spawn);
    }
   }
  }

  if (disable) {
   int i = 0;
   emutex_lock (&module_logic_list_disable_mutex);
   for (; disable[i]; i++) {
    if (!inset ((const void **)module_logic_list_disable, disable[i], SET_TYPE_STRING))
     module_logic_list_disable = set_str_add_stable (module_logic_list_disable, disable[i]);
   }

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn) {
    if (enable) {
     module_logic_spawn_set_disable_all (spawn);
    } else { /* re-use this thread if there's nothing to enable*/
     module_logic_spawn_set_disable (spawn);
    }
   }
  }

  struct mode_switch_callback_data *d = ecalloc (1, sizeof (struct mode_switch_callback_data));

  d->modename = (char *)str_stabilise (ev->string);
  d->mode = mode;

  module_logic_call_back (enable, disable, module_logic_einit_event_handler_core_switch_mode_callback, d);
  module_logic_do_callbacks ();
 }
}

void module_logic_fix_count_callback (void *p) {
 char sw;

 emutex_lock (&module_logic_commit_count_mutex);
 module_logic_commit_count--;
 sw = (module_logic_commit_count == 0);
 emutex_unlock (&module_logic_commit_count_mutex);

 if (sw) {
  struct einit_event evs = evstaticinit (einit_core_done_switching);
  event_emit (&evs, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy (evs);

  module_logic_idle_actions();
 }
}

void module_logic_einit_event_handler_core_manipulate_services (struct einit_event *ev) {
 char sw;

 emutex_lock (&module_logic_commit_count_mutex);
 sw = (module_logic_commit_count == 0);
 module_logic_commit_count++;
 emutex_unlock (&module_logic_commit_count_mutex);

 if (sw) {
  mod_sort_service_list_items_by_preference();

  struct einit_event evs = evstaticinit (einit_core_switching);
  event_emit (&evs, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy (evs);
 }

 if (ev->stringset) {
  if (ev->task & einit_module_enable) {
   int i = 0;
   emutex_lock (&module_logic_list_enable_mutex);
   for (; ev->stringset[i]; i++) {
    if (!inset ((const void **)module_logic_list_enable, ev->stringset[i], SET_TYPE_STRING))
     module_logic_list_enable = set_str_add_stable (module_logic_list_enable, ev->stringset[i]);
   }

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable (spawn);

   module_logic_call_back (set_str_dup_stable(ev->stringset), NULL, module_logic_fix_count_callback, NULL);
   module_logic_do_callbacks ();
  } else if (ev->task & einit_module_disable) {
   int i = 0;
   emutex_lock (&module_logic_list_disable_mutex);
   for (; ev->stringset[i]; i++) {
    if (!inset ((const void **)module_logic_list_disable, ev->stringset[i], SET_TYPE_STRING))
     module_logic_list_disable = set_str_add_stable (module_logic_list_disable, ev->stringset[i]);
   }

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn)
    module_logic_spawn_set_disable (spawn);

   module_logic_call_back (NULL, set_str_dup_stable(ev->stringset), module_logic_fix_count_callback, NULL);
   module_logic_do_callbacks ();
  }
 }
}

void module_logic_einit_event_handler_core_change_service_status (struct einit_event *ev) {
 if (module_logic_commit_count == 0) {
  mod_sort_service_list_items_by_preference();
 }

 /* do stuff here */

 if (ev->set && ev->set[0] && ev->set[1]) {
  if (strmatch (ev->set[1], "enable") || strmatch (ev->set[1], "start")) {
   emutex_lock (&module_logic_list_enable_mutex);
   if (!inset ((const void **)module_logic_list_enable, ev->set[0], SET_TYPE_STRING))
    module_logic_list_enable = set_str_add_stable (module_logic_list_enable, ev->set[0]);

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable (spawn);

  } else if (strmatch (ev->set[1], "disable") || strmatch (ev->set[1], "stop")) {
   emutex_lock (&module_logic_list_disable_mutex);
   if (!inset ((const void **)module_logic_list_disable, ev->set[0], SET_TYPE_STRING))
    module_logic_list_disable = set_str_add_stable (module_logic_list_disable, ev->set[0]);

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn)
    module_logic_spawn_set_disable (spawn);
  } else {
   struct lmodule **m = NULL;
   emutex_lock (&module_logic_service_list_mutex);
   struct stree *st = streefind(module_logic_service_list, ev->set[0], tree_find_first);

   if (st) {
    m = (struct lmodule **)set_noa_dup (st->value);
   }
   emutex_unlock (&module_logic_service_list_mutex);

   if (m) {
    int i = 0;
    for (; m[i]; i++) {
     int r = mod(einit_module_custom, m[i], ev->set[1]);

     ev->integer = ev->integer || (r & status_failed);
    }

    efree (m);
   }
  }
 }
}

/* the next two events are feedback from the core, which we use to advance our... plans */

void module_logic_emit_progress_event() {
 struct einit_event ee = evstaticinit(einit_feedback_switch_progress);

 int fenable = 0;
 int fdisable = 0;

 int enable_progress = 0;
 int disable_progress = 0;

 emutex_lock (&module_logic_list_enable_mutex);
 fenable = setcount ((const void **)module_logic_list_enable);
 emutex_unlock (&module_logic_list_enable_mutex);

 emutex_lock (&module_logic_list_disable_mutex);
 fdisable = setcount ((const void **)module_logic_list_disable);
 emutex_unlock (&module_logic_list_disable_mutex);

 if (module_logic_list_enable_max_count != 0)
  enable_progress = ((module_logic_list_enable_max_count - fenable) * 100 / module_logic_list_enable_max_count);
 else
  enable_progress = -1;

 if (module_logic_list_disable_max_count != 0)
  disable_progress = ((module_logic_list_disable_max_count - fdisable) * 100 / module_logic_list_disable_max_count);
 else
  disable_progress = -1;

 if (enable_progress != -1) {
  if (disable_progress != -1) {
   ee.integer = (enable_progress + disable_progress) / 2;
  } else {
   ee.integer = enable_progress;
  }
 } else if (disable_progress != -1) {
  ee.integer = disable_progress;
 } else {
  ee.integer = 100;
 }

 event_emit (&ee, einit_event_flag_broadcast);
 evstaticdestroy (&ee);
}

void module_logic_einit_event_handler_core_service_enabled (struct einit_event *ev) {
 emutex_lock (&module_logic_list_enable_mutex);
 module_logic_list_enable = strsetdel (module_logic_list_enable, ev->string);

 struct lmodule **spawn = module_logic_find_things_to_enable();
 emutex_unlock (&module_logic_list_enable_mutex);

 module_logic_emit_progress_event();

 module_logic_do_callbacks ();

 if (spawn)
  module_logic_spawn_set_enable (spawn);
}

void module_logic_einit_event_handler_core_service_disabled (struct einit_event *ev) {
 emutex_lock (&module_logic_list_disable_mutex);
 module_logic_list_disable = strsetdel (module_logic_list_disable, ev->string);

 struct lmodule **spawn = module_logic_find_things_to_disable();
 emutex_unlock (&module_logic_list_disable_mutex);

 module_logic_emit_progress_event();

 module_logic_do_callbacks ();

 if (spawn)
  module_logic_spawn_set_disable (spawn);
}

/* this is the event we use to "unblock" modules for use in future switches */

void module_logic_einit_event_handler_core_service_update (struct einit_event *ev) {
 if (!(ev->status & status_working)) {
  emutex_lock (&module_logic_active_modules_mutex);
  module_logic_active_modules = (struct lmodule **)setdel ((void **)module_logic_active_modules, ev->para);

  emutex_unlock (&module_logic_active_modules_mutex);
 }

 if (!(ev->status & status_failed) && (ev->status & status_enabled)) {
  struct lmodule *m = ev->para;

  if (!m->si || !m->si->provides) {
   struct lmodule **spawn = NULL;

   emutex_lock (&module_logic_list_enable_mutex);
   if (module_logic_list_enable) {
    if (m->module && m->module->rid && inset ((const void **)module_logic_list_enable, m->module->rid, SET_TYPE_STRING)) {
     spawn = module_logic_find_things_to_enable();
    }
   }
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable_all (spawn);
  }
 } else if (ev->status & (status_failed | status_disabled)) {
  if (ev->status & status_failed) {
   emutex_lock (&module_logic_broken_modules_mutex);
   module_logic_broken_modules = (struct lmodule **)set_noa_add ((void **)module_logic_broken_modules, ev->para);
   emutex_unlock (&module_logic_broken_modules_mutex);
  }

  struct lmodule **spawn = NULL;

  emutex_lock (&module_logic_list_enable_mutex);
  if (module_logic_list_enable) {
   struct lmodule *m = ev->para;

   if (m->module && m->module->rid && inset ((const void **)module_logic_list_enable, m->module->rid, SET_TYPE_STRING)) {
    spawn = module_logic_find_things_to_enable();
   } else if (m->si && m->si->provides) {
    int i = 0;
    for (; m->si->provides[i]; i++) {
     if (inset ((const void **)module_logic_list_enable, m->si->provides[i], SET_TYPE_STRING)) {
      spawn = module_logic_find_things_to_enable();
      break;
     }
    }
   }
  }
  emutex_unlock (&module_logic_list_enable_mutex);

  if (spawn)
   module_logic_spawn_set_enable_all (spawn);

  emutex_lock (&module_logic_list_disable_mutex);
  if (module_logic_list_disable) {
   struct lmodule *m = ev->para;

   if (m->module && m->module->rid && inset ((const void **)module_logic_list_disable, m->module->rid, SET_TYPE_STRING)) {
    module_logic_list_disable = strsetdel (module_logic_list_disable, ev->string);

    spawn = module_logic_find_things_to_disable();
   } else if (m->si && m->si->provides) {
    int i = 0;
    for (; m->si->provides[i]; i++) {
     if (inset ((const void **)module_logic_list_disable, m->si->provides[i], SET_TYPE_STRING)) {
      spawn = module_logic_find_things_to_disable();
      break;
     }
    }
   }
  }
  emutex_unlock (&module_logic_list_disable_mutex);

  if (spawn)
   module_logic_spawn_set_disable_all (spawn);
 }

 module_logic_do_callbacks ();
}

/* helpers for the new ipc */

void module_logic_ipc_read (struct einit_event *ev) {
 char **path = ev->para;

 struct ipc_fs_node n;

 if (!path) {
  n.name = (char *)str_stabilise ("services");
  n.is_file = 0;
  ev->set = set_fix_add (ev->set, &n, sizeof (n));
 } else if (path && path[0]) {
  if (strmatch (path[0], "services")) {
   if (!path[1]) {
    n.name = (char *)str_stabilise ("all");
    n.is_file = 0;
    ev->set = set_fix_add (ev->set, &n, sizeof (n));
    n.name = (char *)str_stabilise ("provided");
    ev->set = set_fix_add (ev->set, &n, sizeof (n));
   } else if (strmatch (path[1], "all") && !path[2]) {
    n.is_file = 0;

    emutex_lock(&module_logic_service_list_mutex);

    struct stree *cur = streelinear_prepare(module_logic_service_list);

    while (cur) {
     struct lmodule **lm = cur->value;
     if (!(lm && lm[0] && !lm[1] && lm[0]->module && lm[0]->module->rid && strmatch (lm[0]->module->rid, cur->key))) {
      n.name = (char *)str_stabilise (cur->key);
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
     }

     cur = streenext (cur);
    }

    emutex_unlock(&module_logic_service_list_mutex);
   } else if (strmatch (path[1], "provided") && !path[2]) {
    n.is_file = 0;
    char **s = mod_list_all_provided_services();

    if (s) {
     int i = 0;
     for (; s[i]; i++) {
      n.name = (char *)str_stabilise (s[i]);
      ev->set = set_fix_add (ev->set, &n, sizeof (n));
     }

     efree (s);
    }

    emutex_unlock(&module_logic_service_list_mutex);
   } else if (path[2] && !path[3]) {
    n.is_file = 1;

    n.name = (char *)str_stabilise ("status");
    ev->set = set_fix_add (ev->set, &n, sizeof (n));

    n.is_file = 0;

    n.name = (char *)str_stabilise ("providers");
    ev->set = set_fix_add (ev->set, &n, sizeof (n));
    n.name = (char *)str_stabilise ("users");
    ev->set = set_fix_add (ev->set, &n, sizeof (n));
    n.name = (char *)str_stabilise ("modules");
    ev->set = set_fix_add (ev->set, &n, sizeof (n));
   } else if (path[2] && path[3]) {
    if (strmatch (path[3], "status")) {
     if (mod_service_is_provided (path[2])) {
      ev->stringset = set_str_add_stable (ev->stringset, "provided");
     } else {
      ev->stringset = set_str_add_stable (ev->stringset, "not provided ");
     }
    } else if (strmatch (path[3], "modules") && !path[4]) {
     n.is_file = 0;

     emutex_lock(&module_logic_service_list_mutex);

     if (module_logic_service_list) {
      struct stree *cur = streefind(module_logic_service_list, path[2], tree_find_first);

      if (cur) {
       struct lmodule **lm = cur->value;
       if (lm) {
        int i = 0;
        for (; lm[i]; i++) if (lm[i]->module && lm[i]->module->rid) {
         n.name = (char *)str_stabilise (lm[i]->module->rid);
         ev->set = set_fix_add (ev->set, &n, sizeof (n));
        }
       }
      }
     }

     emutex_unlock(&module_logic_service_list_mutex);
    } else if (strmatch (path[3], "users") && !path[4]) {
     n.is_file = 0;

     struct lmodule **lm = mod_get_all_users_of_service (path[2]);
     if (lm) {
      int i = 0;
      for (; lm[i]; i++) if (lm[i]->module && lm[i]->module->rid) {
       n.name = (char *)str_stabilise (lm[i]->module->rid);
       ev->set = set_fix_add (ev->set, &n, sizeof (n));
      }
     }
    } else if (strmatch (path[3], "providers") && !path[4]) {
     n.is_file = 0;

     struct lmodule **lm = mod_get_all_providers (path[2]);
     if (lm) {
      int i = 0;
      for (; lm[i]; i++) if (lm[i]->module && lm[i]->module->rid) {
       n.name = (char *)str_stabilise (lm[i]->module->rid);
       ev->set = set_fix_add (ev->set, &n, sizeof (n));
      }
     }
    }
   }
  } else if (strmatch (path[0], "issues")) {
   char have_issues = 0;
   struct cfgnode *node = NULL;

   while (!have_issues && (node = cfg_findnode ("mode-enable", 0, node))) {
    char *s = NULL;
    if (node->arbattrs) {
     int i = 0;
     for (; node->arbattrs[i]; i+=2) {
      if (strmatch (node->arbattrs[i], "services"))
       s = node->arbattrs[i+1];
     }
    }
    if (s) {
     char **services = str2set (':', s);
     if (services) {
      int si = 0;
      for (; !have_issues && services[si]; si++) {
       have_issues = !module_logic_service_exists_p (services[si]);
      }

      efree (services);
     }
    }
   }

   if (have_issues) {
    if (!path[1]) {
     n.is_file = 1;
     n.name = (char *)str_stabilise ("services");
     ev->set = set_fix_add (ev->set, &n, sizeof (n));
    } else if (strmatch (path[1], "services")) {
     node = NULL;
     char buffer[BUFFERSIZE];

     while ((node = cfg_findnode ("mode-enable", 0, node))) {
      char *s = NULL;
      if (node->arbattrs) {
       int i = 0;
       for (; node->arbattrs[i]; i+=2) {
        if (strmatch (node->arbattrs[i], "services"))
         s = node->arbattrs[i+1];
       }
      }
      if (s) {
       char **services = str2set (':', s);
       if (services) {
        int si = 0;
        for (; services[si]; si++) {
         if (!module_logic_service_exists_p (services[si])) {
          snprintf (buffer, BUFFERSIZE, "service \"%s\" does not exist (referenced in mode \"%s\")", services[si], ((node->mode && node->mode->id) ? node->mode->id : "(none)"));
          ev->stringset = set_str_add (ev->stringset, buffer);
         }
        }

        efree (services);
       }
      }
     }
    }
   }
  }
 }
}

struct ch_thread_data {
 int a;
 struct lmodule *b;
 char *c;
};

void *module_logic_ipc_write_detach_action (struct ch_thread_data *da) {
 mod (da->a, da->b, da->c);

 if (da->c) {
  efree (da->c);
 }
 efree (da);

 return NULL;
}

void module_logic_ipc_write (struct einit_event *ev) {
 char **path = ev->para;

 if (path && ev->set && ev->set[0] && path[0]) {
  if (path[1] && path[2] && path[3] && strmatch (path[0], "services") && strmatch (path[3], "status")) {
   struct einit_event ee = evstaticinit(einit_core_change_service_status);

   ee.set = (void **)set_str_add_stable (NULL, path[2]);
   ee.set = (void **)set_str_add_stable ((char **)ee.set, ev->set[0]);

   ee.stringset = (char **)ee.set;

   fflush (stderr);

   event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
   evstaticdestroy(ee);
  }
 }
}

void module_logic_ipc_stat (struct einit_event *ev) {
 char **path = ev->para;

 if (path && path[0]) {
  if (strmatch (path[0], "services")) {
   ev->flag = (path[1] && path[2] && path[3] && strmatch (path[3], "status") ? 1 : 0);
  }
 }
}

int einit_module_logic_v4_configure (struct lmodule *this) {
 module_init(this);

 event_listen (einit_core_configuration_update, module_logic_einit_event_handler_core_configuration_update);
 event_listen (einit_core_module_list_update, module_logic_einit_event_handler_core_module_list_update);
 event_listen (einit_core_service_enabled, module_logic_einit_event_handler_core_service_enabled);
 event_listen (einit_core_service_disabled, module_logic_einit_event_handler_core_service_disabled);
 event_listen (einit_core_service_update, module_logic_einit_event_handler_core_service_update);
 event_listen (einit_core_switch_mode, module_logic_einit_event_handler_core_switch_mode);
 event_listen (einit_core_manipulate_services, module_logic_einit_event_handler_core_manipulate_services);
 event_listen (einit_core_change_service_status, module_logic_einit_event_handler_core_change_service_status);

 event_listen (einit_ipc_read, module_logic_ipc_read);
 event_listen (einit_ipc_stat, module_logic_ipc_stat);
 event_listen (einit_ipc_write, module_logic_ipc_write);

 module_logic_einit_event_handler_core_configuration_update(NULL);

 return 0;
}
