/*
 *  module-scheme-guile.c
 *  einit
 *
 *  Created on 10/11/2007.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>

#include <einit-modules/exec.h>
#include <einit-modules/ipc.h>

#include <string.h>

#include <libguile.h>
#include <pthread.h>
#include <inttypes.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_scheme_guile_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_scheme_guile_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.scm, Guile)",
 .rid       = "einit-module-scheme-guile",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_scheme_guile_configure
};

module_register(module_scheme_guile_self);

#endif

struct stree *module_scheme_guile_modules = NULL,
 *module_scheme_guile_module_actions = NULL,
 *module_scheme_guile_event_handlers = NULL;

char module_scheme_guile_have_event_thread = 0;

struct scheme_action {
 SCM action;
 struct einit_event *status;
 char *id;
};

struct scheme_event_handler {
 SCM handler;
};

scm_t_bits einit_event_smob;

pthread_mutex_t
 module_scheme_guile_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_scheme_guile_module_actions_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_scheme_guile_event_handlers_mutex = PTHREAD_MUTEX_INITIALIZER;

void module_scheme_guile_scanmodules_work_scheme (char **modules) {
 size_t i = 0;

 for (; modules[i]; i++) {
  scm_c_primitive_load (modules[i]);
 }

 return;
}

void module_scheme_guile_unprotect_event_handler (struct scheme_event_handler *h) {
 scm_gc_unprotect_object (h->handler);
}

void module_scheme_guile_protect_event_handler (struct scheme_event_handler *h) {
 scm_gc_protect_object (h->handler);
}

int module_scheme_guile_scanmodules ( struct lmodule *modchain ) {
 char **modules = NULL;
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode ("subsystem-scheme-modules", 0, node))) {
  char **nmodules = readdirfilter(node, "/lib/einit/modules-scheme/", ".*\\.scm$", NULL, 0);

  if (nmodules) {
   modules = (char **)setcombine_nc ((void **)modules, (const void **)nmodules, SET_TYPE_STRING);

   efree (nmodules);
  }
 }

 if (modules) {
//  emutex_lock (&module_scheme_guile_event_handlers_mutex);
  struct stree *st = streelinear_prepare(module_scheme_guile_event_handlers);

  while (st) {
   struct scheme_event_handler *h = st->value;

   scm_with_guile ((void *(*)(void *))module_scheme_guile_unprotect_event_handler, h);

//   efree (h);

   st = streenext (st);
  }

//  streefree (module_scheme_guile_event_handlers);
  module_scheme_guile_event_handlers = NULL;
//  emutex_unlock (&module_scheme_guile_event_handlers_mutex);

  scm_with_guile ((void *(*)(void *))module_scheme_guile_scanmodules_work_scheme, (void *)modules);

  efree (modules);
 }

 return 1;
}

/* glue-code for making these buggers service-providing modules */

uintptr_t module_scheme_guile_module_custom_w (struct scheme_action *na) {
 SCM rv = scm_call_1 (na->action, scm_from_locale_symbol (na->id));
 return scm_is_true (rv);
}

int module_scheme_guile_module_custom (void *p, char *action, struct einit_event *status) {
 if (status && status->module && status->module->module && status->module->module->rid) {
  size_t indexlen = strlen (status->module->module->rid) + strlen (action) + 2;
  char *index = emalloc (indexlen);
  struct stree *st;

  esprintf (index, indexlen, "%s#%s", status->module->module->rid, action);

  emutex_lock (&module_scheme_guile_module_actions_mutex);
  st = streefind (module_scheme_guile_module_actions, index, tree_find_first);
  emutex_unlock (&module_scheme_guile_module_actions_mutex);

  if (st) {
   struct scheme_action *sa = st->value;
   sa->status = status;
   sa->id = st->key;

   uintptr_t rv = (uintptr_t)scm_with_guile ((void *(*)(void *))module_scheme_guile_module_custom_w, sa);

   sa->status = NULL;
   sa->id = NULL;

   if (rv) return status_ok;
  }

  return status_failed;
 }

 return status_failed;
}

int module_scheme_guile_module_enable (void *p, struct einit_event *status) {
 return module_scheme_guile_module_custom (p, "enable", status);
}

int module_scheme_guile_module_disable (void *p, struct einit_event *status) {
 return module_scheme_guile_module_custom (p, "disable", status);
}

int module_scheme_guile_module_cleanup (struct lmodule *lm) {
 return status_ok;
}

int module_scheme_guile_module_configure (struct lmodule *lm) {
 lm->enable = module_scheme_guile_module_enable;
 lm->disable = module_scheme_guile_module_disable;
 lm->custom = module_scheme_guile_module_custom;
 lm->cleanup = module_scheme_guile_module_cleanup;

 return status_ok;
}

/* library functions below here */

uintptr_t module_scheme_make_module_wo (struct smodule *sm) {
 struct lmodule *lm = NULL;
 struct stree *st = NULL;

 emutex_lock (&module_scheme_guile_modules_mutex);

 st = streefind (module_scheme_guile_modules, sm->rid, tree_find_first);

 emutex_unlock (&module_scheme_guile_modules_mutex);

 if (st && st->value) {
  lm = st->value;
  struct smodule *smo = (struct smodule *)lm->module;

  lm->module = sm;
  mod_update (lm);

  if (smo->si.provides) efree (smo->si.provides);
  if (smo->si.requires) efree (smo->si.requires);
  if (smo->si.after) efree (smo->si.after);
  if (smo->si.before) efree (smo->si.before);
  if (smo->rid) efree (smo->rid);
  if (smo->name) efree (smo->name);
  if (smo) efree (smo);

  return 2;
 } else {
  sm->eiversion = EINIT_VERSION;
  sm->eibuild = BUILDNUMBER;
  sm->version = 1;

  sm->configure = module_scheme_guile_module_configure;

  lm = mod_add (NULL, sm);

  emutex_lock (&module_scheme_guile_modules_mutex);

  module_scheme_guile_modules = streeadd (module_scheme_guile_modules, sm->rid, lm, SET_NOALLOC, NULL);

  emutex_unlock (&module_scheme_guile_modules_mutex);
 }

 return 1;
}

SCM module_scheme_make_module (SCM ids, SCM name, SCM rest) {
 char *id_c, *name_c;
 struct smodule *sm;
 SCM id;
 uintptr_t rv;

 if (scm_is_false(scm_symbol_p (ids))) return SCM_BOOL_F;
 if (scm_is_false(scm_string_p (name))) return SCM_BOOL_F;

 sm = emalloc (sizeof (struct smodule));
 memset (sm, 0, sizeof (struct smodule));

 sm->mode = einit_module_generic;

 scm_dynwind_begin (0);

 id = scm_symbol_to_string(ids);
 id_c = scm_to_locale_string (id);
 scm_dynwind_unwind_handler (efree, id_c, SCM_F_WIND_EXPLICITLY);

 name_c = scm_to_locale_string (name);
 scm_dynwind_unwind_handler (efree, name_c, SCM_F_WIND_EXPLICITLY);

 /* don't quite trust guile's garbage collector yet... */
 sm->rid = estrdup (id_c);
 sm->name = estrdup (name_c);

 if (scm_is_true(scm_list_p (rest))) {
  SCM re = rest;

  while (!scm_is_null (re)) {
   SCM va = scm_car (re);

   if (scm_is_true (scm_list_p (va))) {
    SCM vare = va;
    char elec = 1;
    char *sym = NULL;
    char **vs = NULL;

    while (!scm_is_null (vare)) {
     SCM val = scm_car (vare);
     vare = scm_cdr (vare);

     if (elec == 1) {
      if (scm_is_true(scm_symbol_p (val))) {
       SCM vals = scm_symbol_to_string (val);
       sym = scm_to_locale_string (vals);

       scm_dynwind_unwind_handler (efree, sym, SCM_F_WIND_EXPLICITLY);
      }

      elec++;
     } else {
      if (scm_is_true(scm_string_p (val))) {
       char *da = scm_to_locale_string (val);

       scm_dynwind_unwind_handler (efree, da, SCM_F_WIND_EXPLICITLY);

       vs = (char **)setadd ((void **)vs, da, SET_TYPE_STRING);
      }
     }
    }

    if (sym && vs) {
     if (strmatch (sym, "provides")) {
      sm->si.provides = vs;
     } else if (strmatch (sym, "requires")) {
      sm->si.requires = vs;
     } else if (strmatch (sym, "after")) {
      sm->si.after = vs;
     } else if (strmatch (sym, "before")) {
      sm->si.before = vs;
     } else {
      fprintf (stderr, "ERROR: unexpected attribute: %s\n", sym);
      efree (vs);
     }
    } else {
     if (sym) {
      fprintf (stderr, "ERROR: symbol without vs: %s\n", sym);
     }
     if (vs) efree (vs);
    }
   } else {
    fprintf (stderr, "ERROR: list expected\n");
   }

   re = scm_cdr (re);
  }
 }

 rv = (uintptr_t)scm_without_guile ((void *(*)(void *))module_scheme_make_module_wo, sm);

 scm_dynwind_end ();

 return rv ? SCM_BOOL_T : SCM_BOOL_F;
}

void module_scheme_guile_notice_wo (char *n) {
 notice (5, n);
}

SCM module_scheme_guile_notice (SCM message) {
 char *msg;

 if (scm_is_false(scm_string_p (message))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 if ((msg = scm_to_locale_string (message))) {
  scm_dynwind_unwind_handler (efree, msg, SCM_F_WIND_EXPLICITLY);

  scm_without_guile ((void *(*)(void *))module_scheme_guile_notice_wo, msg);
 }

 scm_dynwind_end ();

 return SCM_BOOL_T;
}

void module_scheme_guile_critical_wo (char *n) {
 notice (2, n);
}

SCM module_scheme_guile_critical (SCM message) {
 char *msg;

 if (scm_is_false(scm_string_p (message))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 if ((msg = scm_to_locale_string (message))) {
  scm_dynwind_unwind_handler (efree, msg, SCM_F_WIND_EXPLICITLY);

  scm_without_guile ((void *(*)(void *))module_scheme_guile_critical_wo, msg);
 }

 scm_dynwind_end ();

 return SCM_BOOL_T;
}

struct scheme_feedback {
 struct einit_event *ev;
 char *message;
};

void module_scheme_guile_feedback_wo (struct scheme_feedback *n) {
 fbprintf (n->ev, n->message);
}

SCM module_scheme_guile_feedback (SCM id, SCM message) {
 SCM ids;
 char *msg, *id_c;
 struct scheme_feedback n; 
 struct scheme_action *na = NULL;
 struct stree *st;

 if (scm_is_false(scm_symbol_p (id))) return SCM_BOOL_F;
 if (scm_is_false(scm_string_p (message))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 ids = scm_symbol_to_string(id);
 id_c = scm_to_locale_string (ids);
 scm_dynwind_unwind_handler (efree, id_c, SCM_F_WIND_EXPLICITLY);

 msg = scm_to_locale_string (message);
 scm_dynwind_unwind_handler (efree, msg, SCM_F_WIND_EXPLICITLY);

 scm_pthread_mutex_lock (&module_scheme_guile_module_actions_mutex);
 st = streefind (module_scheme_guile_module_actions, id_c, tree_find_first);
 emutex_unlock (&module_scheme_guile_module_actions_mutex);

 if (st) {
  na = st->value;

  n.ev = na->status;
  n.message = msg;

  scm_without_guile ((void *(*)(void *))module_scheme_guile_feedback_wo, &n);
 }

 scm_dynwind_end ();

 return SCM_BOOL_T;
}

SCM module_scheme_guile_get_configuration (SCM id) {
 SCM ids;
 char *id_c;

 if (scm_is_false(scm_symbol_p (id))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 ids = scm_symbol_to_string(id);
 id_c = scm_to_locale_string (ids);
 scm_dynwind_unwind_handler (efree, id_c, SCM_F_WIND_EXPLICITLY);

 struct cfgnode *node = cfg_getnode (id_c, NULL);

 if (node && node->arbattrs) {
  SCM rv = scm_list_1 (scm_cons (scm_from_locale_string (node->arbattrs[0]),
                                 scm_from_locale_string (node->arbattrs[1])));
  int i;

  for (i = 2; node->arbattrs[i]; i+=2) {
   rv = scm_cons (scm_cons (scm_from_locale_string (node->arbattrs[i]),
                            scm_from_locale_string (node->arbattrs[i+1])),
                  rv);
  }

  return rv;
 }

 scm_dynwind_end ();

 return SCM_BOOL_F;
}

SCM module_scheme_guile_set_configuration (SCM id, SCM attributes) {
 SCM ids;
 char *id_c;
 int sindex = 0;

 if (scm_is_false(scm_symbol_p (id))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 ids = scm_symbol_to_string(id);
 id_c = scm_to_locale_string (ids);
 scm_dynwind_unwind_handler (efree, id_c, SCM_F_WIND_EXPLICITLY);

 struct cfgnode node;
 memset (&node, 0, sizeof (struct cfgnode));

 while (!scm_is_null (attributes)) {
  SCM v = scm_car (attributes);
  char *a = scm_to_locale_string (scm_car(v));
  scm_dynwind_unwind_handler (efree, a, SCM_F_WIND_EXPLICITLY);
  char *b = scm_to_locale_string (scm_cdr(v));
  scm_dynwind_unwind_handler (efree, b, SCM_F_WIND_EXPLICITLY);

  node.arbattrs = (char **)setadd ((void **)node.arbattrs, a, SET_TYPE_STRING);
  node.arbattrs = (char **)setadd ((void **)node.arbattrs, b, SET_TYPE_STRING);

  if (strmatch (a, "i")) {
   node.value = parse_integer (b);
  } else if (strmatch (a, "b")) {
   node.flag = parse_boolean (b);
  } else if (strmatch (a, "s")) {
   sindex = setcount ((const void **)node.arbattrs);
  }

  attributes = scm_cdr (attributes);
 }

 if (sindex) node.svalue = node.arbattrs[sindex];

 node.id = estrdup(id_c);

 cfg_addnode (&node);

 scm_dynwind_end ();

 return SCM_UNSPECIFIED;
}

SCM module_scheme_define_module_action (SCM rid, SCM action, SCM command) {
 SCM rids, actions;
 char *rid_c, *action_c, *index;
 size_t indexlen;
 struct stree *st = NULL;

 if (scm_is_false(scm_symbol_p (rid))) return SCM_BOOL_F;
 if (scm_is_false(scm_symbol_p (action))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 rids = scm_symbol_to_string(rid);
 rid_c = scm_to_locale_string (rids);
 scm_dynwind_unwind_handler (efree, rid_c, SCM_F_WIND_EXPLICITLY);

 actions = scm_symbol_to_string(action);
 action_c = scm_to_locale_string (actions);
 scm_dynwind_unwind_handler (efree, action_c, SCM_F_WIND_EXPLICITLY);

 indexlen = strlen (rid_c) + strlen (action_c) + 2;
 index = emalloc (indexlen);
 esprintf (index, indexlen, "%s#%s", rid_c, action_c);

 scm_pthread_mutex_lock (&module_scheme_guile_module_actions_mutex);
 st = streefind (module_scheme_guile_module_actions, index, tree_find_first);

 struct scheme_action *na = emalloc (sizeof (struct scheme_action));

 na->action = command;
 scm_gc_protect_object (na->action);

 if (!st) {
  module_scheme_guile_module_actions = streeadd (module_scheme_guile_module_actions, index, na, SET_NOALLOC, NULL);
 } else {
  struct scheme_action *sa = st->value;
  scm_gc_unprotect_object (sa->action);
  st->value = na;
  efree (sa);
 }
 emutex_unlock (&module_scheme_guile_module_actions_mutex);

 efree (index);

 scm_dynwind_end ();

 scm_remember_upto_here_1(command);

 return SCM_BOOL_T;
}

struct scheme_pexec_data {
 char *command;
 char *user;
 char *group;
 struct einit_event *status;
};

intptr_t module_scheme_guile_pexec_wo (struct scheme_pexec_data *p) {
 return pexec (p->command, NULL, 0, 0, p->user, p->group, NULL, p->status);
}

SCM module_scheme_guile_pexec (SCM command, SCM rest) {
 struct scheme_pexec_data p;
 char *nv = NULL;

 if (scm_is_false(scm_string_p (command))) {
  return SCM_BOOL_F;
 }

 memset (&p, 0, sizeof (struct scheme_pexec_data));

 scm_dynwind_begin (0);

 p.command = scm_to_locale_string (command);
 scm_dynwind_unwind_handler (efree, p.command, SCM_F_WIND_EXPLICITLY);

 while (!scm_is_null (rest)) {
  SCM r = scm_car (rest);

  if (!nv && scm_is_true(scm_symbol_p (r))) {
   SCM rvs;

   rvs = scm_symbol_to_string(r);
   nv = scm_to_locale_string (rvs);
   scm_dynwind_unwind_handler (efree, nv, SCM_F_WIND_EXPLICITLY);
  } else if (nv) {
   if (strmatch (nv, "user:") && scm_is_true(scm_string_p (r))) {
    p.user = scm_to_locale_string (r);
    scm_dynwind_unwind_handler (efree, p.user, SCM_F_WIND_EXPLICITLY);
   } else if (strmatch (nv, "group:") && scm_is_true(scm_string_p (r))) {
    p.group = scm_to_locale_string (r);
    scm_dynwind_unwind_handler (efree, p.group, SCM_F_WIND_EXPLICITLY);
   } else if (strmatch (nv, "feedback:") && scm_is_true(scm_symbol_p (r))) {
    char *sym = NULL;
    struct stree *st;
    SCM rvs;

    rvs = scm_symbol_to_string(r);
    sym = scm_to_locale_string (rvs);
    scm_dynwind_unwind_handler (efree, sym, SCM_F_WIND_EXPLICITLY);

    emutex_lock (&module_scheme_guile_module_actions_mutex);
    st = streefind (module_scheme_guile_module_actions, sym, tree_find_first);
    emutex_unlock (&module_scheme_guile_module_actions_mutex);

    if (st) {
     struct scheme_action *sa = st->value;
     p.status = sa->status;
    }
   }

   nv = NULL;
  }

  rest = scm_cdr (rest);
 }

 intptr_t rv = (intptr_t)scm_without_guile ((void *(*)(void *))module_scheme_guile_pexec_wo, &p);

 scm_dynwind_end ();

 return (rv == status_ok) ? SCM_BOOL_T : SCM_BOOL_F;
}

void module_scheme_guile_event_emit_wo (struct einit_event *ev) {
 event_emit (ev, einit_event_flag_broadcast);
}

/* stuff for the einit_event type */
SCM module_scheme_guile_event_emit (SCM event) {
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 scm_without_guile ((void *(*)(void *))module_scheme_guile_event_emit_wo, ev);

 scm_remember_upto_here_1 (event);
 return SCM_UNSPECIFIED;
}

SCM module_scheme_guile_event_p (SCM event) {
 if (SCM_SMOB_PREDICATE (einit_event_smob, event))
  return SCM_BOOL_T;

 return SCM_BOOL_F;
}

SCM module_scheme_guile_event_string (SCM event) {
 struct einit_event *ev;
 SCM rv = SCM_BOOL_F;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 if (ev->string) {
  rv = scm_from_locale_string (ev->string);
 }

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_string_set (SCM event) {
 SCM rv = SCM_BOOL_F;
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 if (ev->stringset) {
  rv = scm_c_make_vector (setcount ((const void **)ev->stringset), SCM_BOOL_F);
  int i = 0;

  for (; ev->stringset[i]; i++)
   SCM_SIMPLE_VECTOR_SET (rv, i, scm_from_locale_string(ev->stringset[i]));
 }

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_argv (SCM event) {
 SCM rv = SCM_BOOL_F;
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 if ((ev->type == einit_event_subsystem_ipc) && ev->argv) {
  rv = scm_c_make_vector (ev->argc, SCM_BOOL_F);
  int i = 0;

  for (; ev->stringset[i]; i++)
   SCM_SIMPLE_VECTOR_SET (rv, i, scm_from_locale_string(ev->argv[i]));
 }

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_type (SCM event) {
 struct einit_event *ev;
 SCM rv = SCM_BOOL_F;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 rv = scm_from_locale_symbol (event_code_to_string (ev->type));

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_command (SCM event) {
 struct einit_event *ev;
 SCM rv = SCM_BOOL_F;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 if (ev->command) {
  rv = scm_from_locale_string (ev->command);
 }

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_task (SCM event) {
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 SCM rv = scm_from_int (ev->task);

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_status (SCM event) {
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 SCM rv = scm_from_int (ev->status);

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_event_integer (SCM event) {
 struct einit_event *ev;

 scm_assert_smob_type (einit_event_smob, event);

 ev = (struct einit_event *) SCM_SMOB_DATA (event);

 SCM rv = scm_from_int (ev->integer);

 scm_remember_upto_here_1 (event);
 return rv;
}

SCM module_scheme_guile_make_einit_event (SCM type, SCM rest) {
 SCM smob;
 struct einit_event *ev;

 if (scm_is_false(scm_symbol_p (type))) return SCM_BOOL_F;

 if (!(ev = (struct einit_event *) scm_gc_malloc (sizeof (struct einit_event), "einit-event"))) return SCM_BOOL_F;

 memset (ev, 0, sizeof (struct einit_event));

 SCM_NEWSMOB (smob, einit_event_smob, ev);

 scm_dynwind_begin (0);

 SCM types = scm_symbol_to_string(type);
 char *type_c = scm_to_locale_string (types);
 scm_dynwind_unwind_handler (efree, type_c, SCM_F_WIND_EXPLICITLY);

 ev->type = event_string_to_code (type_c);

 char *nextpara = NULL;

 while (!scm_is_null (rest)) {
  SCM val = scm_car (rest);

  if (scm_is_true(scm_string_p (val))) {
   char *t = scm_to_locale_string(val);
   scm_dynwind_unwind_handler (efree, t, SCM_F_WIND_EXPLICITLY);

   ev->string = estrdup (t);
  } else if (scm_is_true(scm_symbol_p (val))) {
   SCM syms = scm_symbol_to_string(val);
   nextpara = scm_to_locale_string (syms);
   scm_dynwind_unwind_handler (efree, nextpara, SCM_F_WIND_EXPLICITLY);
  } else if (scm_is_true(scm_list_p (val))) {
   SCM list = val;

   while (!scm_is_null (list)) {
    SCM lval = scm_car (list);

    if (scm_is_true(scm_string_p (lval))) {
     char *t = scm_to_locale_string(lval);
     scm_dynwind_unwind_handler (efree, t, SCM_F_WIND_EXPLICITLY);

     ev->stringset = (char **)setadd ((void **)ev->stringset, t, SET_TYPE_STRING);
    }

    list = scm_cdr (list);
   }
  } else if (nextpara) {
   if (strmatch (nextpara, "task:")) {
    ev->task = scm_to_int (val);
   } else if (strmatch (nextpara, "status:")) {
    ev->status = scm_to_int (val);
   } else if (strmatch (nextpara, "integer:")) {
    ev->integer = scm_to_int (val);
   }
  }

  rest = scm_cdr (rest);
 }

 scm_dynwind_end ();

 return smob;
}

SCM module_scheme_guile_make_einit_event_from_struct (struct einit_event *evo) {
 SCM smob;
 struct einit_event *ev;

 if (!(ev = (struct einit_event *) scm_gc_malloc (sizeof (struct einit_event), "einit-event"))) return SCM_BOOL_F;

 memcpy (ev, evo, sizeof (struct einit_event));

 SCM_NEWSMOB (smob, einit_event_smob, ev);

 if (evo->type != einit_event_subsystem_ipc) {
  if (evo->string) ev->string = estrdup (evo->string);
  if (evo->stringset) ev->stringset = (char **)setdup ((const void **)evo->stringset, SET_TYPE_STRING);
 } else {
  if (evo->command) ev->command = estrdup (evo->command);
  if (evo->argv) ev->argv = (char **)setdup ((const void **)evo->argv, SET_TYPE_STRING);
 }

 return smob;
}

SCM module_scheme_guile_event_listen (SCM event_type, SCM event_handler) {
 if (scm_is_false(scm_symbol_p (event_type))) return SCM_BOOL_F;

 scm_dynwind_begin (0);

 SCM types = scm_symbol_to_string(event_type);
 char *type_c = scm_to_locale_string (types);
 scm_dynwind_unwind_handler (efree, type_c, SCM_F_WIND_EXPLICITLY);

 struct scheme_event_handler *handler = emalloc (sizeof (struct scheme_event_handler));
 handler->handler = event_handler;

 scm_gc_protect_object (handler->handler);

/* actually we don't need to lock a mutex around this, since we're always only adding */
/* + scheme code can't be interpreted while we're parsing a file, which is the only time we really
   do register event handlers */
// scm_pthread_mutex_lock (&module_scheme_guile_event_handlers_mutex);
 module_scheme_guile_event_handlers = streeadd (module_scheme_guile_event_handlers, type_c, handler, SET_NOALLOC, NULL);
// emutex_unlock (&module_scheme_guile_event_handlers_mutex);

 scm_dynwind_end ();

 scm_remember_upto_here_2 (event_type, event_handler);
 return SCM_BOOL_F;
}

static int module_scheme_guile_einit_event_print (SCM event, SCM port, scm_print_state *pstate) {
 struct einit_event *ev = (struct einit_event *) SCM_SMOB_DATA (event);
 char buffer[BUFFERSIZE];

 scm_puts ("#<einit-event type=", port);
 scm_puts (event_code_to_string (ev->type), port);

 if (ev->type != einit_event_subsystem_ipc) {
  if (ev->task) {
   esprintf (buffer, BUFFERSIZE, ", task=%i", ev->task);
   scm_puts (buffer, port);
  }

  if (ev->status) {
   esprintf (buffer, BUFFERSIZE, ", status=%i", ev->status);
   scm_puts (buffer, port);
  }

  if (ev->integer) {
   esprintf (buffer, BUFFERSIZE, ", integer=%i", ev->integer);
   scm_puts (buffer, port);
  }

  if (ev->string) {
   scm_puts (", string=", port);
   scm_puts (ev->string, port);
  }

  if (ev->stringset) {
   int i = 0;
   scm_puts (", stringset=#<", port);

   for (; ev->stringset[i]; i++) {
    if (i) scm_puts (", ", port);
    scm_puts (ev->stringset[i], port);
   }
   scm_puts (">", port);
  }
 }

 scm_puts (">", port);

 scm_remember_upto_here_1 (event);
 return 1;
}

SCM module_scheme_guile_einit_event_mark (SCM event) {
// struct einit_event *ev = (struct einit_event *) SCM_SMOB_DATA (event);

 return SCM_BOOL_F;
}

size_t module_scheme_guile_einit_event_free (SCM event) {
 struct einit_event *ev = (struct einit_event *) SCM_SMOB_DATA (event);

 if (ev->type != einit_event_subsystem_ipc) {
  if (ev->string) efree (ev->string);
  if (ev->stringset) efree (ev->stringset);
 }

 scm_gc_free (ev, sizeof (struct einit_event), "einit-event");

 return 0;
}

void init_einit_event_type (void) {
 einit_event_smob = scm_make_smob_type ("einit-event", sizeof (struct einit_event));
 scm_set_smob_mark (einit_event_smob, module_scheme_guile_einit_event_mark);
 scm_set_smob_free (einit_event_smob, module_scheme_guile_einit_event_free);
 scm_set_smob_print (einit_event_smob, module_scheme_guile_einit_event_print);

/* need to create the type in some way... */
 scm_c_define_gsubr ("make-event", 1, 0, 1, module_scheme_guile_make_einit_event);

/* predicate */
 scm_c_define_gsubr ("event?", 1, 0, 0, module_scheme_guile_event_p);

/* element access */
 scm_c_define_gsubr ("event-type", 1, 0, 0, module_scheme_guile_event_type);

 scm_c_define_gsubr ("event-task", 1, 0, 0, module_scheme_guile_event_task);
 scm_c_define_gsubr ("event-status", 1, 0, 0, module_scheme_guile_event_status);
 scm_c_define_gsubr ("event-integer", 1, 0, 0, module_scheme_guile_event_integer);

 scm_c_define_gsubr ("event-string", 1, 0, 0, module_scheme_guile_event_string);
 scm_c_define_gsubr ("event-string-set", 1, 0, 0, module_scheme_guile_event_string_set);

 scm_c_define_gsubr ("event-argv", 1, 0, 0, module_scheme_guile_event_argv);
 scm_c_define_gsubr ("event-command", 1, 0, 0, module_scheme_guile_event_command);

/* operations with the event type */
 scm_c_define_gsubr ("event-emit", 1, 0, 0, module_scheme_guile_event_emit);
 scm_c_define_gsubr ("event-listen", 2, 0, 0, module_scheme_guile_event_listen);
}

/* module initialisation -- there's two parts to this because we need some stuff to be defined in the
   scheme environment, so scm_with_guile() needs to be used... */

void module_scheme_guile_configure_scheme (void *n) {
// scm_init_threads();
// scm_threads_init_first_thread();

 scm_c_define_gsubr ("notice", 1, 0, 0, module_scheme_guile_notice);
 scm_c_define_gsubr ("critical", 1, 0, 0, module_scheme_guile_critical);

 scm_c_define_gsubr ("feedback", 2, 0, 0, module_scheme_guile_feedback);

 scm_c_define_gsubr ("shell", 1, 0, 1, module_scheme_guile_pexec);

 scm_c_define_gsubr ("make-module", 2, 0, 1, module_scheme_make_module);
 scm_c_define_gsubr ("define-module-action", 3, 0, 0, module_scheme_define_module_action);

 scm_c_define_gsubr ("get-configuration", 1, 0, 0, module_scheme_guile_get_configuration);
 scm_c_define_gsubr ("set-configuration!", 2, 0, 0, module_scheme_guile_set_configuration);

 init_einit_event_type();

 scm_c_primitive_load (EINIT_LIB_BASE "/einit.scm");
}

struct einit_event_call {
 struct scheme_event_handler **evh;
 struct einit_event *ev;
};

void module_scheme_guile_generic_event_handler_w (struct einit_event_call *c) {
 scm_dynwind_begin (0);

 SCM ev = module_scheme_guile_make_einit_event_from_struct (c->ev);
 int i;

 for (i = 0; c->evh[i]; i++) {
  scm_call_1 (c->evh[i]->handler, ev);
  scm_gc_unprotect_object (c->evh[i]->handler);
 }

 scm_remember_upto_here_1 (ev);

 scm_dynwind_end();
}

struct msg_event_queue {
 struct einit_event *ev;
 struct msg_event_queue *next;
};

struct msg_event_queue *module_scheme_guile_event_queue = NULL;

pthread_mutex_t
 module_scheme_guile_ping_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_scheme_guile_event_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
 module_scheme_guile_ping_cond = PTHREAD_COND_INITIALIZER;

void module_scheme_guile_ping_all_threads() {
#ifdef _POSIX_PRIORITY_SCHEDULING
 sched_yield();
#endif

 pthread_cond_broadcast (&module_scheme_guile_ping_cond);
}

void module_scheme_guile_wait_for_ping() {
 int e;

 emutex_lock (&module_scheme_guile_ping_mutex);
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
 struct timespec ts;

 if (clock_gettime(CLOCK_REALTIME, &ts))
  bitch (bitch_stdio, errno, "gettime failed!");

 ts.tv_sec += 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (&module_scheme_guile_ping_cond, &module_scheme_guile_ping_mutex, &ts);
#elif defined(DARWIN)
#if 1
 struct timespec ts;
 struct timeval tv;

 gettimeofday (&tv, NULL);

 ts.tv_sec = tv.tv_sec + 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (&module_scheme_guile_ping_cond, &module_scheme_guile_ping_mutex, &ts);
#else
// notice (2, "warning: un-timed lock.");
 e = pthread_cond_wait (&module_scheme_guile_ping_cond, &module_scheme_guile_ping_mutex);
#endif
#else
 notice (2, "warning: un-timed module_scheme_guile_ping_cond.");
 e = pthread_cond_wait (&ml_cond_service_update, &module_scheme_guile_ping_mutex);
#endif
 emutex_unlock (&module_scheme_guile_ping_mutex);

 if (e
#ifdef ETIMEDOUT
     && (e != ETIMEDOUT)
#endif
    ) {
  bitch (bitch_epthreads, e, "waiting on conditional variable...");
 }
}

void module_scheme_guile_generic_event_handler (struct einit_event *ev) {
 struct msg_event_queue *e = emalloc (sizeof (struct msg_event_queue));

 e->ev = evdup(ev);
 e->next = NULL;

 emutex_lock (&module_scheme_guile_event_queue_mutex);

 e->next = module_scheme_guile_event_queue;
 module_scheme_guile_event_queue = e;

 emutex_unlock (&module_scheme_guile_event_queue_mutex);

 module_scheme_guile_ping_all_threads();
}

void module_scheme_guile_event_dispatcher_thread (void *na) {
 while (1) {
  while (module_scheme_guile_event_queue) {
   struct msg_event_queue *e = NULL;
   struct einit_event *ev = NULL;

   emutex_lock (&module_scheme_guile_event_queue_mutex);

   if (module_scheme_guile_event_queue) {
    e = module_scheme_guile_event_queue;
    module_scheme_guile_event_queue = e->next;
   }

   emutex_unlock (&module_scheme_guile_event_queue_mutex);

   if (e && e->ev) {
    ev = e->ev;
    efree (e);

    struct scheme_event_handler **evh = NULL;
    struct stree *st = NULL;
    char *typename = event_code_to_string(ev->type);

    emutex_lock (&module_scheme_guile_event_handlers_mutex);
    st = streelinear_prepare(module_scheme_guile_event_handlers);

    while (st && module_scheme_guile_event_handlers) {
     struct scheme_event_handler *h = st->value;
     if (strmatch (st->key, "any") || strmatch (st->key, typename)) {
      scm_with_guile ((void *(*)(void *))module_scheme_guile_protect_event_handler, h);
      evh = (struct scheme_event_handler **)setadd ((void **)evh, h, SET_NOALLOC);
     }

     st = streenext (st);
    }
    emutex_unlock (&module_scheme_guile_event_handlers_mutex);

    if (evh) {
     struct einit_event_call c;

     c.evh = evh;
     c.ev = ev;

     scm_with_guile ((void *(*)(void *))module_scheme_guile_generic_event_handler_w, &c);

     efree (evh);
     evh = NULL;
    }

    evdestroy (ev);
   }
  }

  module_scheme_guile_wait_for_ping();
 }
}

int module_scheme_guile_cleanup (struct lmodule *pa) {
 exec_cleanup (pa);
 ipc_cleanup (pa);

 event_ignore (einit_event_subsystem_any, module_scheme_guile_generic_event_handler);

 return 0;
}

int module_scheme_guile_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);
 ipc_configure (pa);

 pa->scanmodules = module_scheme_guile_scanmodules;
 pa->cleanup = module_scheme_guile_cleanup;

 event_listen (einit_event_subsystem_any, module_scheme_guile_generic_event_handler);

 scm_with_guile ((void *(*)(void *))module_scheme_guile_configure_scheme, NULL);

 if (!module_scheme_guile_have_event_thread) {
  module_scheme_guile_have_event_thread = 1;
  ethread_spawn_detached ((void *(*)(void *))module_scheme_guile_event_dispatcher_thread, (void *)NULL);
 }

 return 0;
}
