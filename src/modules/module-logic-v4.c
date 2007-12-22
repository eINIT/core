/*
 *  module-logic-v4.c
 *  einit
 *
 *  Created by Magnus Deininger on 17/12/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

int einit_module_logic_v4_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_module_logic_v4_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
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

pthread_mutex_t
 module_logic_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_free_on_idle_stree_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_broken_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_enable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_disable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_active_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_commit_count_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
 module_logic_list_enable_ping_cond = PTHREAD_COND_INITIALIZER,
 module_logic_list_disable_ping_cond = PTHREAD_COND_INITIALIZER;

void module_logic_einit_event_handler_core_configuration_update (struct einit_event *);

/* the sorting bit and the ipc handler are pretty much verbatim copies of -v3 */

void module_logic_update_init_d () {
 struct cfgnode *einit_d = cfg_getnode ("core-module-logic-maintain-init.d", NULL);

 if (einit_d && einit_d->flag && einit_d->svalue) {
  char *init_d_path = cfg_getstring ("core-module-logic-init.d-path", NULL);

  if (init_d_path) {
   struct stree *cur;
   emutex_lock (&module_logic_service_list_mutex);
//  struct stree *module_logics_service_list;
   cur = streelinear_prepare(module_logic_service_list);

   while (cur) {
    char tmp[BUFFERSIZE];
    esprintf (tmp, BUFFERSIZE, "%s/%s", init_d_path, cur->key);

    symlink (einit_d->svalue, tmp);

    cur = streenext(cur);
   }

   emutex_unlock (&module_logic_service_list_mutex);
  }
 }
}

#define STATUS2STRING(status)\
 (status == status_idle ? "idle" : \
 (status & status_working ? "working" : \
 (status & status_enabled ? "enabled" : "disabled")))

void module_logic_ipc_event_handler (struct einit_event *ev) {
 if (ev->argv && ev->argv[0] && ev->argv[1] && ev->output) {
  if (strmatch (ev->argv[0], "update") && strmatch (ev->argv[1], "init.d")) {
   module_logic_update_init_d();

   ev->implemented = 1;
  } else if (strmatch (ev->argv[0], "examine") && strmatch (ev->argv[1], "configuration")) {
   struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);
   char **modes = NULL;

   while (cfgn) {
    if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !inset ((const void **)modes, (const void *)cfgn->mode->id, SET_TYPE_STRING))) {
     uint32_t i = 0;
     modes = (char **)setadd ((void **)modes, (void *)cfgn->mode->id, SET_TYPE_STRING);

     for (i = 0; cfgn->arbattrs[i]; i+=2) {
      if (strmatch(cfgn->arbattrs[i], "services")) {
       char **tmps = str2set (':', cfgn->arbattrs[i+1]);

       if (tmps) {
        uint32_t i = 0;

        emutex_lock(&module_logic_service_list_mutex);

        for (; tmps[i]; i++) {
         if (!streefind (module_logic_service_list, tmps[i], tree_find_first)) {
          eprintf (ev->output, " * mode \"%s\": service \"%s\" referenced but not found\n", cfgn->mode->id, tmps[i]);
          ev->ipc_return++;
         }
        }

        emutex_unlock(&module_logic_service_list_mutex);

        efree (tmps);
       }
       break;
      }
     }
    }

    cfgn = cfg_findnode ("mode-enable", 0, cfgn);
   }

   if (modes) efree (modes);

   ev->implemented = 1;
  } else if (strmatch (ev->argv[0], "list")) {
   if (strmatch (ev->argv[1], "services")) {
    struct stree *modes = NULL;
    struct stree *cur = NULL;
    struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);

    while (cfgn) {
     if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !streefind (modes, cfgn->mode->id, tree_find_first))) {
      uint32_t i = 0;
      for (i = 0; cfgn->arbattrs[i]; i+=2) {
       if (strmatch(cfgn->arbattrs[i], "services")) {
        char **tmps = str2set (':', cfgn->arbattrs[i+1]);

        modes = streeadd (modes, cfgn->mode->id, tmps, SET_NOALLOC, tmps);

        break;
       }
      }
     }

     cfgn = cfg_findnode ("mode-enable", 0, cfgn);
    }

    emutex_lock(&module_logic_service_list_mutex);

    cur = streelinear_prepare(module_logic_service_list);

    while (cur) {
     struct lmodule **xs = cur->value;

     if (!xs[1] && xs[0]->module && xs[0]->module->rid && strmatch (cur->key, xs[0]->module->rid)) {
      cur = streenext (cur);
      continue;
     }

     char **inmodes = NULL;
     struct stree *mcur = streelinear_prepare(modes);

     while (mcur) {
      if (inset ((const void **)mcur->value, (void *)cur->key, SET_TYPE_STRING)) {
       inmodes = (char **)setadd((void **)inmodes, (void *)mcur->key, SET_TYPE_STRING);
      }

      mcur = streenext(mcur);
     }

     if (inmodes) {
      char *modestr = NULL;
      if (ev->ipc_options & einit_ipc_output_xml) {
       modestr = set2str (':', (const char **)inmodes);
       eprintf (ev->output, " <service id=\"%s\" used-in=\"%s\" provided=\"%s\">\n", cur->key, modestr, mod_service_is_provided(cur->key) ? "yes" : "no");
      } else {
       if (ev->ipc_options & einit_ipc_output_ansi) {
        if (mod_service_is_provided(cur->key)) {
         eprintf (ev->output, "\e[32m%s\e[0m |", cur->key);
        } else {
         eprintf (ev->output, "%s |", cur->key);
        }
       } else
        eprintf (ev->output, "%s |", cur->key);
      }
      if (modestr) efree (modestr);
      efree (inmodes);
     } else if (!(ev->ipc_options & einit_ipc_only_relevant)) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       eprintf (ev->output, " <service id=\"%s\" provided=\"%s\">\n", cur->key, mod_service_is_provided(cur->key) ? "yes" : "no");
      } else {
       if (ev->ipc_options & einit_ipc_output_ansi) {
        if (mod_service_is_provided(cur->key)) {
         eprintf (ev->output, "\e[32m%s\e[0m |", cur->key);
        } else {
         eprintf (ev->output, "%s |", cur->key);
        }
       } else
        eprintf (ev->output, "%s |", cur->key);
      }
     }

     if (inmodes || (!(ev->ipc_options & einit_ipc_only_relevant))) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         char *name = escape_xml (xs[u]->module && xs[u]->module->rid ? xs[u]->module->name : "unknown");
         char *rid = escape_xml (xs[u]->module && xs[u]->module->name ? xs[u]->module->rid : "unknown");

         eprintf (ev->output, "  <module id=\"%s\" name=\"%s\" status=\"%s\"",
                  rid, name, STATUS2STRING(xs[u]->status));

         efree (name);
         efree (rid);

         if (xs[u]->si) {
          if (xs[u]->si->provides) {
           char *x = set2str(':', (const char **)xs[u]->si->provides);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  provides=\"%s\"", y);
           efree (y);
           efree (x);
          }
          if (xs[u]->si->requires) {
           char *x = set2str(':', (const char **)xs[u]->si->requires);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  requires=\"%s\"", y);
           efree (y);
           efree (x);
          }
          if (xs[u]->si->after) {
           char *x = set2str(':', (const char **)xs[u]->si->after);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  after=\"%s\"", y);
           efree (y);
           efree (x);
          }
          if (xs[u]->si->before) {
           char *x = set2str(':', (const char **)xs[u]->si->before);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  before=\"%s\"", y);
           efree (y);
           efree (x);
          }
         }

         char **functions = (char **)setdup ((const void **)xs[u]->functions, SET_TYPE_STRING);
         if (xs[u]->enable) functions = (char **)setadd ((void **)functions, "enable", SET_TYPE_STRING);
         if (xs[u]->disable) functions = (char **)setadd ((void **)functions, "disable", SET_TYPE_STRING);
         functions = (char **)setadd ((void **)functions, "zap", SET_TYPE_STRING);

         if (functions) {
          char *x = set2str(':', (const char **)functions);
          char *y = escape_xml (x);
          eprintf (ev->output, "\n  functions=\"%s\"", y);
          efree (y);
          efree (x);

          efree (functions);
         }

         eputs (" />\n", ev->output);
        }
       }

       eputs (" </service>\n", ev->output);
      } else {
       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         char *trid = xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown";

         if (ev->ipc_options & einit_ipc_output_ansi) {
          if (xs[u]->status & status_enabled) {
           eprintf (ev->output, " \e[32m%s\e[0m", trid);
          } else if (xs[u]->status & status_failed) {
           eprintf (ev->output, " \e[31m%s\e[0m", trid);
          } else if (xs[u]->module && (xs[u]->module->mode & einit_module_deprecated)) {
           eprintf (ev->output, " \e[35m%s\e[0m", trid);
          } else  {
           eprintf (ev->output, " %s", trid);
          }
         } else {
          eprintf (ev->output,
            ((xs[u]->module && (xs[u]->module->mode & einit_module_deprecated)) ?
              " (%s)" : " %s"), trid);
         }
        }
       }
      }
     }

     if (!(ev->ipc_options & einit_ipc_output_xml)) {
      eputs ("\n", ev->output);
     }

     cur = streenext (cur);
    }

    emutex_unlock(&module_logic_service_list_mutex);

    ev->implemented = 1;
   }

  }
 }
}

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

/* callers of the following two functions need to lock the appropriate mutex on their own! */

/* this is the function that can figure out what to enable */

struct lmodule **module_logic_find_things_to_enable() {
 if (!module_logic_list_enable) return NULL;

 struct lmodule **rv = NULL;

 struct lmodule **candidates_level1 = NULL;
 struct lmodule **candidates_level2 = NULL;
 int i = 0;
 char **unresolved = NULL;
 char **broken = NULL;
 char **services_level1 = NULL;

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

  emutex_lock (&module_logic_service_list_mutex);
  struct stree *st = streefind(module_logic_service_list, module_logic_list_enable[i], tree_find_first);

  if (st) {
   struct lmodule **lm = st->value;
   char do_rotate, isbroken = 0;
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
      isbroken = 1;
      break;
     }
    }
   } while (do_rotate);

   if (isbroken) {
    emutex_unlock (&module_logic_service_list_mutex);

    broken = (char **)setadd ((void **)broken, module_logic_list_enable[i], SET_TYPE_STRING);
    module_logic_list_enable = strsetdel (module_logic_list_enable, module_logic_list_enable[i]);
    i = -1;

    if (candidates_level1) {
     efree (candidates_level1);
     candidates_level1 = NULL;
    }

    if (!module_logic_list_enable) break;
    continue;
   }

   if (!lm[1] && lm[0]->module && lm[0]->module->rid && strmatch (lm[0]->module->rid, module_logic_list_enable[i]) && (lm[0]->status & status_enabled)) { /* this check prevents us from trying to enable a module specified via its RID twice */
    emutex_unlock (&module_logic_service_list_mutex);

    module_logic_list_enable = strsetdel (module_logic_list_enable, module_logic_list_enable[i]);
    i = -1;

    if (candidates_level1) {
     efree (candidates_level1);
     candidates_level1 = NULL;
    }

    if (!module_logic_list_enable) break;
    continue;
   }

   if (mod_service_requirements_met(lm[0])) {
    candidates_level1 = (struct lmodule **)setadd ((void **)candidates_level1, lm[0], SET_NOALLOC);

    if (lm[0]->module) {
     if (lm[0]->module->rid) {
      if (!inset ((const void **)services_level1, lm[0]->module->rid, SET_TYPE_STRING))
       services_level1 = (char **)setadd ((void **)services_level1, lm[0]->module->rid, SET_TYPE_STRING);
     }
     if (lm[0]->si && lm[0]->si->provides) {
      int j = 0;
      for (; lm[0]->si->provides[j]; j++) {
       if (!inset ((const void **)services_level1, lm[0]->si->provides[j], SET_TYPE_STRING))
        services_level1 = (char **)setadd ((void **)services_level1, lm[0]->si->provides[j], SET_TYPE_STRING);
      }
     }
    }
   } else {
    /* need to add stuff that is still needed somewhere... */
#if 0
    fprintf (stderr, "nyu?: %s\n", lm[0]->module->rid);
    fflush (stderr);
#endif

    if (lm[0]->si && lm[0]->si->requires) {
     int y = 0;
     char impossible = 0;

     for (y = 0; lm[0]->si->requires[y]; y++) {
      if (broken && inset ((const void **)broken, lm[0]->si->requires[y], SET_TYPE_STRING)) {
       impossible = 1;
      }
     }

     if (impossible) {
#if 0
      fprintf (stderr, "impossible: %s\n", lm[0]->module->rid);
      fflush (stderr);
#endif

      emutex_lock (&module_logic_broken_modules_mutex);
      module_logic_broken_modules = (struct lmodule **)setadd ((void **)module_logic_broken_modules, lm[0], SET_NOALLOC);
      emutex_unlock (&module_logic_broken_modules_mutex);

      i--;
      goto next;
     } else {
      for (y = 0; lm[0]->si->requires[y]; y++) {
       if (!inset ((const void **)module_logic_list_enable, lm[0]->si->requires[y], SET_TYPE_STRING)) {
#if 0
        fprintf (stderr, " !! %s needs: %s\n", lm[0]->module->rid, lm[0]->si->requires[y]);
        fflush (stderr);
#endif

        module_logic_list_enable = (char **)setadd ((void **)module_logic_list_enable, lm[0]->si->requires[y], SET_TYPE_STRING);

        if (candidates_level1) {
         efree (candidates_level1);
         candidates_level1 = NULL;
        }

        i = -1;
       }
      }
     }
    }
   }
  } else {
   unresolved = (char **)setadd ((void **)unresolved, module_logic_list_enable[i], SET_TYPE_STRING);
  }

  next:
  emutex_unlock (&module_logic_service_list_mutex);
 }

 if (broken) {
  struct einit_event ee = evstaticinit(einit_feedback_broken_services);
  ee.set = (void **)broken;
  ee.stringset = broken;

  event_emit (&ee, einit_event_flag_broadcast);
  evstaticdestroy (ee);

  efree (broken);
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
 }

/* here we need to filter the level1 list a bit, in order to remove modules that provide services that other modules already provide */
/* example: we have logger and syslog in the specs, logger resolves to v-metalog and syslog to v-syslog... then we want v-syslog to be used,
   because v-syslog provides both syslog and logger, but v-metalog only provides logger. */
/* here'd also be a good place to remove dupes */
#if 0
 if (candidates_level1) {
  reeval:

  for (i = 0; candidates_level1[i]; i++) {
   if (candidates_level1[i]->si->provides)
   int j = 0;
   for (j = 0; candidates_level1[j]; j++) {
    
   }
  }
 }
#endif

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
       services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING);
     }
     if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
      int k = 0;
      for (; candidates_level1[i]->si->provides[k]; k++) {
       if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING))
        services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING);
      }
     }
    }
   }

   if (!doskip) {
    candidates_level2 = (struct lmodule **)setadd ((void **)candidates_level2, candidates_level1[i], SET_NOALLOC);
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
       char **matchthis = (candidates_level2[y]->si && candidates_level2[y]->si->provides) ? (char **)setdup ((const void **)candidates_level2[y]->si->provides, SET_TYPE_STRING) : NULL;
       if (candidates_level2[y]->module && candidates_level2[y]->module->rid) {
        matchthis = (char **)setadd ((void **)matchthis, candidates_level2[y]->module->rid, SET_TYPE_STRING);
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

   efree (candidates_level1);
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
    rv = (struct lmodule **)setadd ((void **)rv, candidates_level2[i], SET_NOALLOC);
    emutex_lock (&module_logic_active_modules_mutex);
    module_logic_active_modules = (struct lmodule **)setadd ((void **)module_logic_active_modules, candidates_level2[i], SET_NOALLOC);
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

 for (i = 0; module_logic_list_disable[i]; i++) {
  if (i == 0) {
   int c = setcount ((const void **)module_logic_list_disable);
   module_logic_list_disable_max_count = (module_logic_list_disable_max_count > c) ? module_logic_list_disable_max_count : c;
  }

  if (!mod_service_is_provided(module_logic_list_disable[i])) {
   module_logic_list_disable = strsetdel (module_logic_list_disable, module_logic_list_disable[i]);
   i--;
   if (!module_logic_list_disable) break;
   continue;
  }

  emutex_lock (&module_logic_service_list_mutex);
  struct stree *st = streefind(module_logic_service_list, module_logic_list_disable[i], tree_find_first);

  if (st) {
   struct lmodule **lm = st->value;
   int i = 0;
   char added = 0;

   for (; lm[i]; i++) {
    if (lm[i]->status & status_enabled) {
     candidates_level1 = (struct lmodule **)setadd ((void **)candidates_level1, lm[i], SET_NOALLOC);
     added = 1;
    }
   }

   if (!added) {
    module_logic_list_disable = strsetdel (module_logic_list_disable, module_logic_list_disable[i]);

    if (candidates_level1) {
     efree (candidates_level1);
     candidates_level1 = NULL;
    }

    i = -1;

    emutex_unlock (&module_logic_service_list_mutex);

    if (!module_logic_list_disable) {
     return NULL;
    }
    continue;
   }
  } else {
   unresolved = (char **)setadd ((void **)unresolved, module_logic_list_disable[i], SET_TYPE_STRING);
  }
  emutex_unlock (&module_logic_service_list_mutex);
 }

/* make sure we disable everything used by our candidates first */

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
     module_logic_broken_modules = (struct lmodule **)setadd ((void **)module_logic_broken_modules, candidates_level1[i], SET_NOALLOC);
     emutex_unlock (&module_logic_broken_modules_mutex);

     efree (candidates_level1);
     candidates_level1 = NULL;
     goto reeval_top;
    } else {
     candidates_level1 = (struct lmodule **)setdel ((void **)candidates_level1, candidates_level1[i]);
     for (j = 0; users[j]; j++) {
      candidates_level1 = (struct lmodule **)setadd ((void **)candidates_level1, users[j], SET_NOALLOC);
     }
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
   struct einit_event ee = evstaticinit(einit_feedback_unresolved_services);
   ee.set = (void **)unresolved;
   ee.stringset = unresolved;

   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);

   efree (module_logic_list_disable);
   module_logic_list_disable = NULL;
  }

  efree (unresolved);
 }

 if (candidates_level1) {
  for (i = 0; candidates_level1[i]; i++) {
   if (candidates_level1[i]->module) {
    if (candidates_level1[i]->module->rid) {
     if (!inset ((const void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING))
      services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING);
    }
    if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
     int j = 0;
     for (; candidates_level1[i]->si->provides[j]; j++) {
      if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[j], SET_TYPE_STRING))
       services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->si->provides[j], SET_TYPE_STRING);
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
        services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->module->rid, SET_TYPE_STRING);
      }
      if (candidates_level1[i]->si && candidates_level1[i]->si->provides) {
       int k = 0;
       for (; candidates_level1[i]->si->provides[k]; k++) {
        if (!inset ((const void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING))
         services_level1 = (char **)setadd ((void **)services_level1, candidates_level1[i]->si->provides[k], SET_TYPE_STRING);
       }
      }
     }
    }

    if (!doskip) {
     candidates_level2 = (struct lmodule **)setadd ((void **)candidates_level2, candidates_level1[i], SET_NOALLOC);
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
        char **matchthis = (candidates_level2[y]->si && candidates_level2[y]->si->provides) ? (char **)setdup ((const void **)candidates_level2[y]->si->provides, SET_TYPE_STRING) : NULL;
        if (candidates_level2[y]->module && candidates_level2[y]->module->rid) {
         matchthis = (char **)setadd ((void **)matchthis, candidates_level2[y]->module->rid, SET_TYPE_STRING);
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
    notice (2, "WARNING: before/after abuse detected! trying to enable /something/ anyway");

    efree (candidates_level1);
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
   }
   emutex_unlock (&module_logic_active_modules_mutex);

   if (!doskip) {
    rv = (struct lmodule **)setadd ((void **)rv, candidates_level2[i], SET_NOALLOC);
    emutex_lock (&module_logic_active_modules_mutex);
    module_logic_active_modules = (struct lmodule **)setadd ((void **)module_logic_active_modules, candidates_level2[i], SET_NOALLOC);
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
   ethread_spawn_detached_run (module_logic_do_enable, spawn[i]);
  } else {
   mod (einit_module_enable, spawn[i], NULL);
  }
 }
}

void module_logic_spawn_set_enable_all (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  ethread_spawn_detached_run (module_logic_do_enable, spawn[i]);
 }
}

void module_logic_spawn_set_disable (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  if (spawn[i+1]) {
   ethread_spawn_detached_run (module_logic_do_disable, spawn[i]);
  } else {
   mod (einit_module_disable, spawn[i], NULL);
  }
 }
}

void module_logic_spawn_set_disable_all (struct lmodule **spawn) {
 int i = 0;
 for (; spawn[i]; i++) {
  ethread_spawn_detached_run (module_logic_do_disable, spawn[i]);
 }
}

/* in the following event handler, we (re-)build our service-name -> module(s) lookup table */

void module_logic_einit_event_handler_core_module_list_update (struct einit_event *ev) {
 struct stree *new_service_list = NULL;
 struct lmodule *cur = ev->para;

 while (cur) {
  if (cur->module && cur->module->rid) {
   struct lmodule **t = (struct lmodule **)setadd ((void **)NULL, cur, SET_NOALLOC);

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

    curval = (struct lnode **)setadd ((void **)curval, cur, SET_NOALLOC);

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
 module_logic_free_on_idle_stree = (struct stree **)setadd ((void **)module_logic_free_on_idle_stree, old_service_list, SET_NOALLOC);
 emutex_unlock (&module_logic_free_on_idle_stree_mutex);

 /* updating the list of modules does mean we also need re-apply preferences... */
 mod_sort_service_list_items_by_preference();
}

/* what we also need is a list of preferences... */

void module_logic_einit_event_handler_core_configuration_update (struct einit_event *ev) {
 mod_sort_service_list_items_by_preference();
}

/* the following three events are used to make the module logics core do something */

void module_logic_ping_wait (pthread_cond_t *cond, pthread_mutex_t *mutex) {
 int e;

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
 struct timespec ts;

 if (clock_gettime(CLOCK_REALTIME, &ts))
  bitch (bitch_stdio, errno, "gettime failed!");

 ts.tv_sec += 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (cond, mutex, &ts);
#elif defined(DARWIN)
 struct timespec ts;
 struct timeval tv;

 gettimeofday (&tv, NULL);

 ts.tv_sec = tv.tv_sec + 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (cond, mutex, &ts);
#else
 notice (2, "warning: un-timed lock.");
 e = pthread_cond_wait (cond, mutex);
#endif

 if (e
#ifdef ETIMEDOUT
     && (e != ETIMEDOUT)
#endif
    ) {
  bitch (bitch_epthreads, e, "waiting on conditional variable for plan");
 }
}

void module_logic_wait_for_services_to_be_enabled(char **services) {
 emutex_lock (&module_logic_list_enable_mutex);

 do {
 /* check here ... */

  if (!module_logic_list_enable) {
   emutex_unlock (&module_logic_list_enable_mutex);
   return;
  } else if (services) {
   int i = 0;

   for (; services[i]; i++) {
    if (!inset ((const void **)module_logic_list_enable, services[i], SET_TYPE_STRING)) {
     if ((services = strsetdel (services, services[i])))
      i = -1;
     else
      break;
    } else {
#if 0
     fprintf (stderr, " ** still waiting for: %s\n", services[i]);
     fflush (stderr);
#endif
    }
   }
  }

  if (!services) {
   emutex_unlock (&module_logic_list_enable_mutex);
   return;
  }

  /* wait until we get ping'd that stuff was enabled */
  module_logic_ping_wait (&module_logic_list_enable_ping_cond, &module_logic_list_enable_mutex);
 } while (1);
}

void module_logic_wait_for_services_to_be_disabled(char **services) {
 emutex_lock (&module_logic_list_disable_mutex);

 do {
  /* check here ... */

  if (!module_logic_list_disable) {
   emutex_unlock (&module_logic_list_disable_mutex);
   return;
  } else if (services) {
   int i = 0;

   for (; services[i]; i++) {
    if (!inset ((const void **)module_logic_list_disable, services[i], SET_TYPE_STRING)) {
     if ((services = strsetdel (services, services[i])))
      i = -1;
     else
      break;
    }
   }
  }

  if (!services) {
   emutex_unlock (&module_logic_list_disable_mutex);
   return;
  }

  /* wait until we get ping'd that stuff was disabled */
  module_logic_ping_wait (&module_logic_list_disable_ping_cond, &module_logic_list_disable_mutex);
 } while (1);
}

struct cfgnode *module_logic_prepare_mode_switch (char *modename, char ***enable_r, char ***disable_r) {
 if (!modename) return NULL;

 struct cfgnode *mode = cfg_findnode (modename, einit_node_mode, NULL);

 if (!mode) {
  return NULL;
 }

 char **enable = *enable_r, **disable = *disable_r;

 char **tmplist, *tmpstring;
 if ((tmplist = str2set (':', cfg_getstring ("enable/services", mode)))) {
  int i = 0;
  for (; tmplist[i]; i++) {
   if (!enable || !inset ((const void **)enable, tmplist[i], SET_TYPE_STRING)) {
    enable = (char **)setadd ((void **)enable, tmplist[i], SET_TYPE_STRING);
   }
  }

  efree (tmplist);
 }

 if ((tmplist = str2set (':', cfg_getstring ("disable/services", mode)))) {
  int i = 0;
  for (; tmplist[i]; i++) {
   if (!disable || !inset ((const void **)disable, tmplist[i], SET_TYPE_STRING)) {
    disable = (char **)setadd ((void **)disable, tmplist[i], SET_TYPE_STRING);
   }
  }

  efree (tmplist);
 }

 if ((tmpstring = cfg_getstring ("options/shutdown", mode)) && parse_boolean(tmpstring)) {
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
     enable = (char **)setadd ((void **)enable, tmp, SET_TYPE_STRING);
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
  char disable_all_but_feedback = inset ((const void **)disable, (void *)"all-but-feedback", SET_TYPE_STRING);

  if (disable_all || disable_all_but_feedback) {
   struct stree *cur;
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
     } else if ((disable_all && strmatch(tmp[i], "all")) ||
                (disable_all_but_feedback && strmatch(tmp[i], "all-but-feedback"))) {
      add = 0;
     } else {
      emutex_lock (&module_logic_service_list_mutex);
      if (module_logic_service_list && (cur = streefind (module_logic_service_list, tmp[i], tree_find_first))) {
       struct lmodule **lm = (struct lmodule **)cur->value;
       if (lm) {
        ssize_t y = 0;
        for (; lm[y]; y++) {
         if (disable_all_but_feedback && (lm[y]->module->mode & einit_module_feedback)) {
          add = 0;

          break;
         }
        }
       }
      }
      emutex_unlock (&module_logic_service_list_mutex);
     }

     if (add) {
      disable = (char **)setadd((void **)disable, (void *)tmp[i], SET_TYPE_STRING);
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

  if (ev->output) {
   struct einit_event ee = evstaticinit(einit_feedback_register_fd);
   ee.output = ev->output;
   ee.ipc_options = ev->ipc_options;
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy(ee);
  }

  struct cfgnode *mode = module_logic_prepare_mode_switch (ev->string, &enable, &disable);

  if (mode) {
   char *cmdt;
   cmode = mode;

   struct einit_event eex = evstaticinit (einit_core_mode_switching);
   eex.para = (void *)mode;
   eex.string = mode->id;
   event_emit (&eex, einit_event_flag_broadcast);
   evstaticdestroy (eex);

   if ((cmdt = cfg_getstring ("before-switch/emit-event", cmode))) {
    struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy (ee);
   }

   if ((cmdt = cfg_getstring ("before-switch/ipc", cmode))) {
    char **cmdts = str2set (':', cmdt);
    uint32_t in = 0;

    if (cmdts) {
     for (; cmdts[in]; in++)
      ipc_process(cmdts[in], stderr);

     efree (cmdts);
    }
   }
  }

  if (enable) {
   int i = 0;
   emutex_lock (&module_logic_list_enable_mutex);
   for (; enable[i]; i++) {
    if (!inset ((const void **)module_logic_list_enable, enable[i], SET_TYPE_STRING))
     module_logic_list_enable = (char **)setadd ((void **)module_logic_list_enable, enable[i], SET_TYPE_STRING);
   }

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable_all (spawn);
  }

  if (disable) {
   int i = 0;
   emutex_lock (&module_logic_list_disable_mutex);
   for (; disable[i]; i++) {
    if (!inset ((const void **)module_logic_list_disable, disable[i], SET_TYPE_STRING))
     module_logic_list_disable = (char **)setadd ((void **)module_logic_list_disable, disable[i], SET_TYPE_STRING);
   }

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn)
    module_logic_spawn_set_disable_all (spawn);
  }

/* waiting on things to be enabled and disabled works just fine if we do it serially... */
  module_logic_wait_for_services_to_be_enabled(enable);
  module_logic_wait_for_services_to_be_disabled(disable);

  if (mode) {
   char *cmdt;
   cmode = mode;
   amode = mode;

   struct einit_event eex = evstaticinit (einit_core_mode_switch_done);
   eex.para = (void *)mode;
   eex.string = mode->id;
   event_emit (&eex, einit_event_flag_broadcast);
   evstaticdestroy (eex);

   if (amode->id) {
//   notice (1, "emitting feedback notice");

    struct einit_event eema = evstaticinit (einit_core_plan_update);
    eema.string = estrdup(amode->id);
    eema.para   = (void *)amode;
    event_emit (&eema, einit_event_flag_broadcast);
    efree (eema.string);
    evstaticdestroy (eema);
   }

   if ((cmdt = cfg_getstring ("after-switch/ipc", amode))) {
//   notice (1, "doing ipc");

    char **cmdts = str2set (':', cmdt);
    uint32_t in = 0;

    if (cmdts) {
     for (; cmdts[in]; in++) {
      ipc_process(cmdts[in], stderr);
     }
     efree (cmdts);
    }
   }

   if ((cmdt = cfg_getstring ("after-switch/emit-event", amode))) {
//   notice (1, "emitting event");
    struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy (ee);
   }
  }

  if (ev->output) {
   struct einit_event ee = evstaticinit(einit_feedback_unregister_fd);
   ee.output = ev->output;
   ee.ipc_options = ev->ipc_options;
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy(ee);
  }
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

  notice (3, "suspending and unloading unused modules...");

  struct einit_event eml = evstaticinit(einit_core_suspend_all);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy(eml);
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
     module_logic_list_enable = (char **)setadd ((void **)module_logic_list_enable, ev->stringset[i], SET_TYPE_STRING);
   }

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable (spawn);

   module_logic_wait_for_services_to_be_enabled((char **)setdup((const void **)ev->stringset, SET_TYPE_STRING));
  } else if (ev->task & einit_module_disable) {
   int i = 0;
   emutex_lock (&module_logic_list_disable_mutex);
   for (; ev->stringset[i]; i++) {
    if (!inset ((const void **)module_logic_list_disable, ev->stringset[i], SET_TYPE_STRING))
     module_logic_list_disable = (char **)setadd ((void **)module_logic_list_disable, ev->stringset[i], SET_TYPE_STRING);
   }

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn)
    module_logic_spawn_set_disable (spawn);

   module_logic_wait_for_services_to_be_disabled((char **)setdup((const void **)ev->stringset, SET_TYPE_STRING));
  }
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

  notice (3, "suspending and unloading unused modules...");

  struct einit_event eml = evstaticinit(einit_core_suspend_all);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy(eml);
 }
}

void module_logic_einit_event_handler_core_change_service_status (struct einit_event *ev) {
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

 /* do stuff here */

 if (ev->set && ev->set[0] && ev->set[1]) {
  if (ev->output) {
   if (!strmatch (ev->set[1], "status")) {
    struct einit_event ee = evstaticinit(einit_feedback_register_fd);
    ee.output = ev->output;
    ee.ipc_options = ev->ipc_options;
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy(ee);
   }
  }

  if (strmatch (ev->set[1], "enable") || strmatch (ev->set[1], "start")) {
   emutex_lock (&module_logic_list_enable_mutex);
   if (!inset ((const void **)module_logic_list_enable, ev->set[0], SET_TYPE_STRING))
    module_logic_list_enable = (char **)setadd ((void **)module_logic_list_enable, ev->set[0], SET_TYPE_STRING);

   struct lmodule **spawn = module_logic_find_things_to_enable();
   emutex_unlock (&module_logic_list_enable_mutex);

   if (spawn)
    module_logic_spawn_set_enable (spawn);

   module_logic_wait_for_services_to_be_enabled((char **)str2set(0, ev->set[0]));
   ev->integer = !(mod_service_is_provided (ev->set[0]));
  } else if (strmatch (ev->set[1], "disable") || strmatch (ev->set[1], "stop")) {
   emutex_lock (&module_logic_list_disable_mutex);
   if (!inset ((const void **)module_logic_list_disable, ev->set[0], SET_TYPE_STRING))
    module_logic_list_disable = (char **)setadd ((void **)module_logic_list_disable, ev->set[0], SET_TYPE_STRING);

   struct lmodule **spawn = module_logic_find_things_to_disable();
   emutex_unlock (&module_logic_list_disable_mutex);

   if (spawn)
    module_logic_spawn_set_disable (spawn);

   module_logic_wait_for_services_to_be_disabled((char **)str2set(0, ev->set[0]));
   ev->integer = mod_service_is_provided (ev->set[0]);
  } else if (strmatch (ev->set[1], "status")) {
   struct lmodule **tm = NULL;

   if (ev->output) {
    emutex_lock (&module_logic_service_list_mutex);
    struct stree *st = streefind(module_logic_service_list, ev->set[0], tree_find_first);

    if (st) {
     tm = (struct lmodule **)setdup ((const void **)st->value, SET_NOALLOC);
    }
    emutex_unlock (&module_logic_service_list_mutex);
   }

   if (tm) {
    int r = 0;

    for (; tm[r]; r++) if (tm[r]->module) {
     if (r == 0) {
      eprintf (ev->output, "%s: \"%s\".\n", (char *)ev->set[0], tm[r]->module->name);
      eprintf (ev->output, " \e[32m**\e[0m service \"%s\" is currently %s\e[0m.\n", (char *)ev->set[0], mod_service_is_provided (ev->set[0]) ? "\e[32mprovided" : "\e[31mnot provided");
     } else {
      eprintf (ev->output, "backup candiate #%i: \"%s\".\n", r, tm[r]->module->name);
     }

     if (tm[r]->module->rid)
      eprintf (ev->output, " \e[34m>>\e[0m rid: %s.\n", tm[r]->module->rid);
     if (tm[r]->source)
      eprintf (ev->output, " \e[34m>>\e[0m source: %s.\n", tm[r]->source);

     if (tm[r]->suspend) { eputs (" \e[34m>>\e[0m this module supports suspension.\n", ev->output); }

     eputs (" \e[34m>>\e[0m supported functions:", ev->output);

     if (tm[r]->enable) { eputs (" enable", ev->output); }
     if (tm[r]->disable) { eputs (" disable", ev->output); }
     if (tm[r]->custom) { eputs (" *", ev->output); }

     eputs ("\n \e[34m>>\e[0m status flags: (", ev->output);

     if (tm[r]->status & status_working) {
      eputs (" working", ev->output);
     }

     if (tm[r]->status & status_enabled) {
      eputs (" enabled", ev->output);
     }

     if (tm[r]->status & status_disabled) {
      eputs (" disabled", ev->output);
     }

     if (tm[r]->status == status_idle) {
      eputs (" idle", ev->output);
     }

     eputs (" )\n", ev->output);
    }

    efree (tm);
   }

   ev->integer = 0;
  } else {
   struct lmodule **m = NULL;
   emutex_lock (&module_logic_service_list_mutex);
   struct stree *st = streefind(module_logic_service_list, ev->set[0], tree_find_first);

   if (st) {
    m = (struct lmodule **)setdup ((const void **)st->value, SET_NOALLOC);
   }
   emutex_unlock (&module_logic_service_list_mutex);

   if (m) {
    ev->integer = 0;
    int i = 0;
    for (; m[i]; i++) {
     int r = mod(einit_module_custom, m[i], ev->set[1]);

     ev->integer = ev->integer || (r & status_failed);
    }

    efree (m);
   } else {
    ev->integer = 1;
   }
  }

  if (ev->output) {
   struct einit_event ee = evstaticinit(einit_feedback_unregister_fd);

   if (!strmatch (ev->set[1], "status")) {
    ee.output = ev->output;
    ee.ipc_options = ev->ipc_options;
    event_emit (&ee, einit_event_flag_broadcast);
    evstaticdestroy(ee);
   }

   fflush (ev->output);

   if (!strmatch(ev->set[1], "status")) {
    if (ev->integer) {
     eputs (" \e[31m!! request failed.\e[0m\n", ev->output);
    } else {
     eputs (" \e[32m>> request succeeded.\e[0m\n", ev->output);
    }
   }

   fflush (ev->output);
  }
 }

 /* done with stuff */

 emutex_lock (&module_logic_commit_count_mutex);
 module_logic_commit_count--;
 sw = (module_logic_commit_count == 0);
 emutex_unlock (&module_logic_commit_count_mutex);

 if (sw) {
  struct einit_event evs = evstaticinit (einit_core_done_switching);
  event_emit (&evs, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy (evs);

  module_logic_idle_actions();

  notice (3, "suspending and unloading unused modules...");

  struct einit_event eml = evstaticinit(einit_core_suspend_all);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy(eml);
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

// pthread_cond_broadcast (&module_logic_list_enable_ping_cond);

 module_logic_emit_progress_event();

 if (spawn)
  module_logic_spawn_set_enable (spawn);
}

void module_logic_einit_event_handler_core_service_disabled (struct einit_event *ev) {
 emutex_lock (&module_logic_list_disable_mutex);
 module_logic_list_disable = strsetdel (module_logic_list_disable, ev->string);

 struct lmodule **spawn = module_logic_find_things_to_disable();
 emutex_unlock (&module_logic_list_disable_mutex);

// pthread_cond_broadcast (&module_logic_list_disable_ping_cond);

 module_logic_emit_progress_event();

 if (spawn)
  module_logic_spawn_set_disable (spawn);
}

/* this is the event we use to "unblock" modules for use in future switches */

void module_logic_einit_event_handler_core_service_update (struct einit_event *ev) {
#if 0
 char run_idle_actions = 0;
#endif

 if (!(ev->status & status_working)) {
  emutex_lock (&module_logic_active_modules_mutex);
  module_logic_active_modules = (struct lmodule **)setdel ((void **)module_logic_active_modules, ev->para);

#if 0
  if (!module_logic_active_modules) {
   run_idle_actions = 1;
  }
#endif
  emutex_unlock (&module_logic_active_modules_mutex);
 }

 if (ev->status & status_failed) {
  emutex_lock (&module_logic_broken_modules_mutex);
  module_logic_broken_modules = (struct lmodule **)setadd ((void **)module_logic_broken_modules, ev->para, SET_NOALLOC);
  emutex_unlock (&module_logic_broken_modules_mutex);

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

#if 0
 if (run_idle_actions)
  module_logic_idle_actions();
#endif

 pthread_cond_broadcast (&module_logic_list_disable_ping_cond);
}



int einit_module_logic_v4_cleanup (struct lmodule *this) {
 ipc_cleanup (this);

 event_ignore (einit_ipc_request_generic, module_logic_ipc_event_handler);
 event_ignore (einit_core_configuration_update, module_logic_einit_event_handler_core_configuration_update);
 event_ignore (einit_core_module_list_update, module_logic_einit_event_handler_core_module_list_update);
 event_ignore (einit_core_service_enabled, module_logic_einit_event_handler_core_service_enabled);
 event_ignore (einit_core_service_disabled, module_logic_einit_event_handler_core_service_disabled);
 event_ignore (einit_core_service_update, module_logic_einit_event_handler_core_service_update);
 event_ignore (einit_core_switch_mode, module_logic_einit_event_handler_core_switch_mode);
 event_ignore (einit_core_manipulate_services, module_logic_einit_event_handler_core_manipulate_services);
 event_ignore (einit_core_change_service_status, module_logic_einit_event_handler_core_change_service_status);

 return 0;
}

int einit_module_logic_v4_configure (struct lmodule *this) {
 module_init(this);
 ipc_configure (this);

 thismodule->cleanup = einit_module_logic_v4_cleanup;

 event_listen (einit_ipc_request_generic, module_logic_ipc_event_handler);
 event_listen (einit_core_configuration_update, module_logic_einit_event_handler_core_configuration_update);
 event_listen (einit_core_module_list_update, module_logic_einit_event_handler_core_module_list_update);
 event_listen (einit_core_service_enabled, module_logic_einit_event_handler_core_service_enabled);
 event_listen (einit_core_service_disabled, module_logic_einit_event_handler_core_service_disabled);
 event_listen (einit_core_service_update, module_logic_einit_event_handler_core_service_update);
 event_listen (einit_core_switch_mode, module_logic_einit_event_handler_core_switch_mode);
 event_listen (einit_core_manipulate_services, module_logic_einit_event_handler_core_manipulate_services);
 event_listen (einit_core_change_service_status, module_logic_einit_event_handler_core_change_service_status);

 module_logic_einit_event_handler_core_configuration_update(NULL);

 return 0;
}
