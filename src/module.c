/*
 *  module.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <wait.h>
#include <einit/exec.h>

struct lmodule *mlist = NULL;
extern char shutting_down;

pthread_mutex_t mlist_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t update_critical_phase_mutex = PTHREAD_MUTEX_INITIALIZER;

struct service_usage_item {
 struct lmodule **provider;
 struct lmodule **users;
};

struct stree *service_usage = NULL;
pthread_mutex_t service_usage_mutex = PTHREAD_MUTEX_INITIALIZER;

struct lmodule *mod_update (struct lmodule *module) {
 if (!module->module) return module;

 struct service_information *original_data = NULL;

 if (module->module->si.provides || module->module->si.requires || module->module->si.after || module->module->si.before) {
  struct service_information *new_data = ecalloc (1, sizeof (struct service_information));

  if (module->module->si.provides)
   new_data->provides = set_str_dup_stable(module->module->si.provides);
  if (module->module->si.requires)
   new_data->requires = set_str_dup_stable(module->module->si.requires);
  if (module->module->si.after)
   new_data->after = set_str_dup_stable(module->module->si.after);
  if (module->module->si.before)
   new_data->before = set_str_dup_stable(module->module->si.before);

  emutex_lock (&update_critical_phase_mutex);
  original_data = module->si;
  module->si = new_data;
  emutex_unlock (&update_critical_phase_mutex);
 } else {
  emutex_lock (&update_critical_phase_mutex);
  original_data = module->si;
  module->si = NULL;
  emutex_unlock (&update_critical_phase_mutex);
 }

 if (original_data) {
  if (original_data->provides) {
   efree (original_data->provides);
  }
  if (original_data->requires) {
   efree (original_data->requires);
  }
  if (original_data->after) {
   efree (original_data->after);
  }
  if (original_data->before) {
   efree (original_data->before);
  }

  efree (original_data);
 }

 struct einit_event ee = evstaticinit (einit_core_update_module);
 ee.para = (void *)module;
 event_emit (&ee, einit_event_flag_broadcast);
 evstaticdestroy(ee);

 return module;
}

char **mod_blocked_rids = NULL;
pthread_mutex_t mod_blocked_rids_mutex = PTHREAD_MUTEX_INITIALIZER;

struct lmodule *mod_add_or_update (void *sohandle, const struct smodule *module, enum mod_add_options options) {
 if (!module || !module->rid || !module->name) return NULL;

 emutex_lock (&mod_blocked_rids_mutex);
 if (inset ((const void **)mod_blocked_rids, module->rid, SET_TYPE_STRING)) {
  emutex_unlock (&mod_blocked_rids_mutex);
  return NULL;
 }
 emutex_unlock (&mod_blocked_rids_mutex);

 struct lmodule *nmod = mod_lookup_rid(module->rid);

 if (nmod) {
  if (nmod->si) {
   void *tmp;

   if ((tmp = nmod->si->provides)) {
    nmod->si->provides = NULL;
    efree (tmp);
   }
   if ((tmp = nmod->si->requires)) {
    nmod->si->requires = NULL;
    efree (tmp);
   }
   if ((tmp = nmod->si->before)) {
    nmod->si->before = NULL;
    efree (tmp);
   }
   if ((tmp = nmod->si->after)) {
    nmod->si->after = NULL;
    efree (tmp);
   }

   tmp = nmod->si;
   nmod->si = NULL;
   efree (tmp);
  }

  if (options & substitue_and_prune) {
   const struct smodule *orig = nmod->module;
   nmod->module = module;
   if (orig->si.provides) efree (orig->si.provides);
   if (orig->si.requires) efree (orig->si.requires);
   if (orig->si.before) efree (orig->si.before);
   if (orig->si.after) efree (orig->si.after);
   /* the other fields should always be added with str_stabilise()! */
   efree ((void *)orig);
  }

  return mod_update (nmod); /* this'll update everything */
 }

 return mod_add (sohandle, module); /* do a plain add if we don't have this rid already */
}

struct lmodule *mod_add (void *sohandle, const struct smodule *module) {
 struct lmodule *nmod;

/* if (module) {
  fprintf (stderr, " * mod_add(*, %s)\n", module->rid);
 }*/

 emutex_lock (&mod_blocked_rids_mutex);
 if (inset ((const void **)mod_blocked_rids, module->rid, SET_TYPE_STRING)) {
  emutex_unlock (&mod_blocked_rids_mutex);
  return NULL;
 }
 emutex_unlock (&mod_blocked_rids_mutex);

 emutex_lock (&mlist_mutex);
 struct lmodule *m = mlist;

 while (m) {
  if (m->module && m->module->rid && module && module->rid && strmatch (m->module->rid, module->rid)) {
   break;
  }

  m = m->next;
 }
 emutex_unlock (&mlist_mutex);

 if (m) return m;

 nmod = ecalloc (1, sizeof (struct lmodule));

 nmod->sohandle = sohandle;
 nmod->module = module;
 emutex_init (&nmod->mutex, NULL);

 if (module->si.provides || module->si.requires || module->si.after || module->si.before) {
  nmod->si = ecalloc (1, sizeof (struct service_information));

  if (module->si.provides)
   nmod->si->provides = set_str_dup_stable(module->si.provides);
  if (module->si.requires)
   nmod->si->requires = set_str_dup_stable(module->si.requires);
  if (module->si.after)
   nmod->si->after = set_str_dup_stable(module->si.after);
  if (module->si.before)
   nmod->si->before = set_str_dup_stable(module->si.before);
 } else
  nmod->si = NULL;

 if (module->configure) {
  int rv = module->configure (nmod);
  if (rv & (status_configure_done | status_configure_failed)) {
   /* module doesn't want to be loaded for real */

   if (nmod->si) {
    if (nmod->si->provides) {
     efree (nmod->si->provides);
    }
    if (nmod->si->requires) {
     efree (nmod->si->requires);
    }
    if (nmod->si->after) {
     efree (nmod->si->after);
    }
    if (nmod->si->before) {
     efree (nmod->si->before);
    }

    efree (nmod->si);
   }

   efree (nmod);

   if (rv & status_block) {
    emutex_lock (&mod_blocked_rids_mutex);
    mod_blocked_rids = set_str_add_stable (mod_blocked_rids, module->rid);
    emutex_unlock (&mod_blocked_rids_mutex);
   }

   return NULL;
  }
 }

 emutex_lock (&mlist_mutex);
 nmod->next = mlist;
 mlist = nmod;
 emutex_unlock (&mlist_mutex);

 nmod = mod_update (nmod);

 return nmod;
}

struct einit_event *mod_initialise_feedback_event (struct lmodule *module, enum einit_module_task task) {
 struct einit_event *fb = evinit (einit_feedback_module_status);
  fb->task = task;
  fb->status = status_working;

 if (module && module->module && module->module->rid)
  fb->rid = module->module->rid;

 return fb;
}

void mod_emit_pre_hook_event (struct lmodule *module, enum einit_module_task task) {
 if (module->si && module->si->provides) {
  int i = 0;

  if (task & (einit_module_enable | einit_module_disable))
   for (i = 0; module->si->provides[i]; i++) {
    struct einit_event eei = evstaticinit ((task & einit_module_enable) ? einit_core_service_enabling : einit_core_service_disabling);
    eei.string = module->si->provides[i];
    event_emit (&eei, einit_event_flag_broadcast);
    evstaticdestroy (eei);
   }

  struct einit_event ees = evstaticinit (einit_core_service_update);
  ees.task = task;
  ees.status = status_working;
  ees.string = (module->module && module->module->rid) ? module->module->rid : module->si->provides[0];
  ees.set = (void **)module->si->provides;
  ees.para = (void *)module;
  event_emit (&ees, einit_event_flag_broadcast);
  evstaticdestroy (ees);
 }
}

void mod_completion_handler (struct lmodule *module, struct einit_event *fb, enum einit_module_task task) {
 fb->status = module->status;

 event_emit(fb, einit_event_flag_broadcast);

/* service status update */
 char *nserv[] = { "no-service", NULL };

 struct einit_event ees = evstaticinit (einit_core_service_update);
 ees.task = task;
 ees.status = fb->status;
 ees.string = (module->module && module->module->rid) ? module->module->rid : "??";
 ees.set = (void **)((module->si && module->si->provides) ? module->si->provides : nserv);
 ees.para = (void *)module;
 event_emit (&ees, einit_event_flag_broadcast);
 evstaticdestroy (ees);
}

void mod_completion_handler_no_change (struct lmodule *module, enum einit_module_task task) {
 char *nserv[] = { "no-service", NULL };

 struct einit_event ees = evstaticinit (einit_core_service_update);
 ees.task = task;
 ees.status = module->status;
 ees.string = (module->module && module->module->rid) ? module->module->rid : "??";
 ees.set = (void **)((module->si && module->si->provides) ? module->si->provides : nserv);
 ees.para = (void *)module;
 event_emit (&ees, einit_event_flag_broadcast);
 evstaticdestroy (ees);
}

struct completion_callback_data {
 struct einit_event *fb;
 enum einit_module_task task;
 struct lmodule *module;
 int status;
};

int mod_completion_callback (struct completion_callback_data *x) {
 x->module->status = x->status;

 mod_completion_handler (x->module, x->fb, x->task);
 evdestroy (x->fb);

 mod_update_usage_table(x->module);

 if (shutting_down) {
  if ((x->task & einit_module_disable) && (x->module->status & (status_enabled | status_failed))) {
   return mod (einit_module_custom, x->module, "zap");
  }
 }

 return x->module->status;
}

int mod_completion_callback_wrapper (struct einit_event *fb, enum einit_module_task task, struct lmodule *module, int status) {
 struct completion_callback_data x;

 x.fb = fb;
 x.task = task;
 x.module = module;
 x.status = status;

 return mod_completion_callback (&x);
}

void mod_completion_callback_dead_process(struct einit_exec_data *xd) {
 struct completion_callback_data *x = xd->custom;

 x->status = (WIFEXITED(xd->status) && (WEXITSTATUS(xd->status) == EXIT_SUCCESS)) ? status_ok : status_failed;

 if (x->task & einit_module_custom) {
  x->module->status = (x->module->status & (status_enabled | status_disabled)) | x->status;
 } else if (x->task & einit_module_enable) {
  if (x->status == status_ok) {
   x->module->status = status_ok | status_enabled;
  } else {
   x->module->status = status_failed | status_disabled;
  }
 } else if (x->task & einit_module_disable) {
  if (x->status == status_ok) {
   x->module->status = status_ok | status_disabled;
  } else {
   x->module->status = status_failed | status_enabled;
  }
 }

 x->status = x->module->status;

 mod_completion_callback (x);
 efree (xd->custom);
}

int mod (enum einit_module_task task, struct lmodule *module, char *custom_command) {
 struct einit_event *fb;
 unsigned int ret;

 if (!module || !module->module) return 0;

 emutex_lock (&module->mutex);

 if (task & einit_module_custom) {
  if (!custom_command) {
   emutex_unlock (&module->mutex);

   return status_failed;
  }

  goto skipdependencies;
 }

 if (task & einit_module_ignore_dependencies) {
  notice (2, "module: skipping dependency-checks");
  task ^= einit_module_ignore_dependencies;
  goto skipdependencies;
 }

 module->status |= status_working;

 /* check if the task requested has already been done (or if it can be done at all) */
 if ((task & einit_module_enable) && ((!(module->module->mode & einit_module_event_actions) && !module->enable) || (module->status & status_enabled))) {
  wontload:
    module->status ^= status_working;

  mod_completion_handler_no_change (module, task);

  emutex_unlock (&module->mutex);
  return status_idle;
 }

 if ((task & einit_module_disable) && ((!(module->module->mode & einit_module_event_actions) && !module->disable) || (module->status & status_disabled) || (module->status == status_idle)))
  goto wontload;

 if (task & einit_module_enable) {
  if (!mod_service_requirements_met(module))
   goto wontload;
 } else if (task & einit_module_disable) {
  if (!mod_service_not_in_use(module))
   goto wontload;
 }

 skipdependencies:

 if (module->module->mode & einit_module_event_actions) {
  if ((task & einit_module_custom) && (strmatch (custom_command, "zap"))) {
   module->status = status_disabled | (module->status & status_failed);

   return mod_complete (module->module->rid, einit_module_custom, module->status);
  }

  char *action = custom_command;

  if (task & einit_module_enable) {
   action = "enable";
  } else if (task & einit_module_disable) {
   action = "disable";
  }

  if (!action)
   goto wontload;

  struct einit_event e = evstaticinit (einit_core_module_action_execute);
  e.rid = module->module->rid;
  e.string = (char *)str_stabilise(action);

  event_emit (&e, einit_event_flag_broadcast);

  evstaticdestroy (e);

  return status_working;
 }

/* inform everyone about what's going to happen */
 mod_emit_pre_hook_event (module, task);

/* actual loading bit */
 fb = mod_initialise_feedback_event (module, task);
 event_emit(fb, einit_event_flag_broadcast);

 if ((task & einit_module_custom) && (strmatch (custom_command, "zap"))) {
  module->status = status_disabled | (module->status & status_failed);
  fb->string = "module ZAP'd.";

  return mod_completion_callback_wrapper (fb, task, module, module->status);
 }

 char in_fork = 0;

 if (module->module->mode & einit_module_fork_actions) {
  struct completion_callback_data *x = emalloc (sizeof (struct completion_callback_data));

  x->fb = fb;
  x->task = task;
  x->module = module;

  pid_t p = einit_fork (mod_completion_callback_dead_process, x, module->module->rid, module);

  fprintf (stderr, "einit_fork(): %i\n", p);
  if (p > 0) return status_working;
  if (p < 0) {
   perror ("something bad just happened");
   return status_failed;
  }

  in_fork = 1;
 }

 if (task & einit_module_custom) {
  if (module->custom) {
   module->status = module->custom(module->param, custom_command, fb);
  } else {
   module->status = (module->status & (status_enabled | status_disabled)) | status_failed | status_command_not_implemented;
  }
 } else if (task & einit_module_enable) {
  ret = module->enable (module->param, fb);
  if (ret & status_ok) {
   module->status = status_ok | status_enabled;
  } else {
   module->status = status_failed | status_disabled;
  }
 } else if (task & einit_module_disable) {
  ret = module->disable (module->param, fb);
  if (ret & status_ok) {
   module->status = status_ok | status_disabled;
  } else {
   module->status = status_failed | status_enabled;
  }
 }

 fprintf (stderr, "exit?\n");

 if (in_fork) {
  fprintf (stderr, "exiting\n");

  if (module->status & status_ok) _exit (EXIT_SUCCESS);

  _exit (EXIT_FAILURE);
 } else {
  fprintf (stderr, "not exiting\n");

  return mod_completion_callback_wrapper (fb, task, module, module->status);
 }
}

struct lmodule *mod_lookup_rid (const char *rid) {
 if (!rid) return NULL;

 struct lmodule *module = mlist;
 while (module) {
  if (module->module && module->module->rid && strmatch (module->module->rid, rid)) {
   break;
  }

  module = module->next;
 }

 return module;
}

char *mod_lookup_pid (pid_t pid) {
 struct lmodule *module = mlist;
 while (module) {
  if (module->pid == pid) {
   break;
  }

  module = module->next;
 }

 return (module && module->module) ? module->module->rid : NULL;
}

void mod_update_pids () {
 struct lmodule *module = mlist;
 while (module) {
  char *buf;
  if (module->pidfile && (buf = readfile (module->pidfile))) {
   module->pid = parse_integer (buf);

   fprintf (stderr, "%s now has pid %i", module->module->rid, module->pid);
   efree (buf);
  }

  module = module->next;
 }
}

struct lmodule *mod_lookup_source (const char *source) {
 if (!source) return NULL;

 struct lmodule *module = mlist;
 while (module) {
  if (module->source && strmatch (module->source, source)) {
   break;
  }

  module = module->next;
 }

 return module;
}

int mod_update_sources (char **source) {
 int rv = 0;
 if (!source) return rv;

 struct lmodule *module = mlist;
 while (module) {
  if (module->source && inset ((const void **)source, module->source, SET_TYPE_STRING)) {
   mod_update (module);
   rv++;
  }

  module = module->next;
 }

 return rv;
}

int mod_update_source (const char *source) {
 int rv = 0;
 if (!source) return rv;

 struct lmodule *module = mlist;
 while (module) {
  if (module->source && strmatch (source, module->source)) {
   mod_update (module);
   rv++;
  }

  module = module->next;
 }

 return rv;
}

int mod_complete (char *rid, enum einit_module_task task, enum einit_module_status status) {
 fprintf (stderr, "mod_complete(%s)\n", rid);

 struct lmodule *module = mod_lookup_rid(rid);

 if (!module) return status_failed;

 struct einit_event *fb = mod_initialise_feedback_event (module, task);

 fb->status = status;
 module->status = status;

 fprintf (stderr, "mod_update_usage_table(%s)\n", rid);
 mod_update_usage_table (module);

 if (shutting_down) {
  if ((task & einit_module_disable) && (module->status & (status_enabled | status_failed))) {
   fprintf (stderr, "mod(%s, zap)\n", rid);
   mod (einit_module_custom, module, "zap");
  }
 }

 fprintf (stderr, "mod_completion_handler(%s)\n", rid);
 mod_completion_handler (module, fb, task);
 evdestroy (fb);

 fprintf (stderr, "mod_completion(%s, done)\n", rid);
 return status;
}

void mod_update_usage_table (struct lmodule *module) {
 struct stree *ha;
 char **t;
 uint32_t i;
 struct service_usage_item *item;

 char **disabled = NULL;
 char **enabled = NULL;

 emutex_lock (&service_usage_mutex);

 if (module->status & status_enabled) {
  if (module->si) {
   if ((t = module->si->requires)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      item->users = (struct lmodule **)set_noa_add ((void **)item->users, (void *)module);
     }
    }
   }
   if ((t = module->si->provides)) {
    for (i = 0; t[i]; i++) {
     if (service_usage && (ha = streefind (service_usage, t[i], tree_find_first)) && (item = (struct service_usage_item *)ha->value)) {
      if (!item->provider) {
       enabled = set_str_add_stable (enabled, t[i]);
      }

      item->provider = (struct lmodule **)set_noa_add ((void **)item->provider, (void *)module);
     } else {
      struct service_usage_item nitem;
      memset (&nitem, 0, sizeof (struct service_usage_item));
      nitem.provider = (struct lmodule **)set_noa_add ((void **)nitem.provider, (void *)module);
      service_usage = streeadd (service_usage, t[i], &nitem, sizeof (struct service_usage_item), NULL);

      enabled = set_str_add_stable (enabled, t[i]);
     }
    }
   }
  }
 }

/* more cleanup code */
 ha = streelinear_prepare(service_usage);
 while (ha) {
  item = (struct service_usage_item *)ha->value;

  if (!(module->status & status_enabled)) {
   char wasprovider = inset ((const void **)item->provider, module, SET_NOALLOC);

   item->provider = (struct lmodule **)setdel ((void **)item->provider, (void *)module);
   item->users = (struct lmodule **)setdel ((void **)item->users, (void *)module);

   if (wasprovider && !item->provider) {
    disabled = set_str_add_stable (disabled, ha->key);
   }
  }

#if 0
  if (!item->provider && !item->users) {
   service_usage = streedel (ha);
   if (!service_usage) break;

   ha = streelinear_prepare(service_usage);
  } else
#endif
  ha = streenext (ha);
 }

 emutex_unlock (&module->mutex);
 emutex_unlock (&service_usage_mutex);

 if (enabled) {
  struct einit_event eei = evstaticinit (einit_core_service_enabled);

  for (i = 0; enabled[i]; i++) {
   eei.string = enabled[i];
   event_emit (&eei, einit_event_flag_broadcast);
  }

  evstaticdestroy (eei);
  efree (enabled);
 }

 if (disabled) {
  struct einit_event eei = evstaticinit (einit_core_service_disabled);

  for (i = 0; disabled[i]; i++) {
   eei.string = disabled[i];
   event_emit (&eei, einit_event_flag_broadcast);
  }

  evstaticdestroy (eei);
  efree (disabled);
 }
}

char mod_service_is_provided (char *service) {
 struct stree *ha;
 struct service_usage_item *item;
 char rv = 0;

 emutex_lock (&service_usage_mutex);

 if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->provider))
  rv = 1;

 emutex_unlock (&service_usage_mutex);

 return rv;
}

char mod_service_is_in_use (char *service) {
 struct stree *ha;
 struct service_usage_item *item;
 char rv = 0;

 emutex_lock (&service_usage_mutex);

 if (service_usage && (ha = streefind (service_usage, service, tree_find_first)) && (item = (struct service_usage_item *)ha->value) && (item->users))
  rv = 1;

 emutex_unlock (&service_usage_mutex);

 return rv;
}

char mod_service_requirements_met (struct lmodule *module) {
 char ret = 1;
 struct stree *ha;
 uint32_t i;
 char **t;

 emutex_lock (&service_usage_mutex);

 if (module->si && (t = module->si->requires)) {
  for (i = 0; t[i]; i++) {
   if (!service_usage || !(ha = streefind (service_usage, t[i], tree_find_first)) ||
       !((struct service_usage_item *)(ha->value))->provider) {
    ret = 0;
    break;
   }
  }
 }

 emutex_unlock (&service_usage_mutex);

 return ret;
}

char mod_service_not_in_use(struct lmodule *module) {
 char ret = 1;
 struct stree *ha;

 emutex_lock (&service_usage_mutex);

 ha = streelinear_prepare(service_usage);

/* a service is "in use" if
  * it's the provider for something, and
  * it's the only provider for that for that specific service, and
  * all of the users of that service use the */

  while (ha) {
   if (((struct service_usage_item *)(ha->value))->users &&
       (((struct service_usage_item *)(ha->value))->provider) &&
       ((((struct service_usage_item *)(ha->value))->provider)[0]) &&
       (!((((struct service_usage_item *)(ha->value))->provider)[1])) &&
       inset ((const void **)(((struct service_usage_item *)(ha->value))->provider), module, -1)) {

/* this one might be a culprit */
    uint32_t i = 0, r = 0;

    for (; (((struct service_usage_item *)(ha->value))->users)[i]; i++) {
     if ((((struct service_usage_item *)(ha->value))->users)[i]->si &&
         (((struct service_usage_item *)(ha->value))->users)[i]->si->requires) {

      if (inset ((const void **)((((struct service_usage_item *)(ha->value))->users)[i]->si->requires), ha->key, SET_TYPE_STRING)) {
       r++;
      }

     }
    }

/* yep, really is in use */
    if (i == r) {
     ret = 0;
     break;
    }
   }
   ha = streenext (ha);
  }


 emutex_unlock (&service_usage_mutex);

 return ret;
}

char **mod_list_all_provided_services () {
 emutex_lock (&service_usage_mutex);

 struct stree *ha = streelinear_prepare(service_usage);
 char **ret = NULL;
 struct service_usage_item *item;

 while (ha) {
  item = ha->value;
  if (item->provider)
   ret = set_str_add_stable (ret, ha->key);

  ha = streenext(ha);
 }

 emutex_unlock (&service_usage_mutex);

 return ret;
}

struct lmodule **mod_get_all_users (struct lmodule *module) {
 struct lmodule **ret = NULL;

 emutex_lock (&service_usage_mutex);
 struct stree *ha = streelinear_prepare(service_usage);

 while (ha) {
  struct service_usage_item *item = ha->value;

  if (item->provider && item->users && inset ((const void **)item->provider, module, SET_NOALLOC)) {
   int i = 0;
   for (; item->users[i]; i++) {
    ret = (struct lmodule **)set_noa_add ((void **)ret, item->users[i]);
   }
  }
  ha = streenext (ha);
 }
 emutex_unlock (&service_usage_mutex);

 return ret;
}

struct lmodule **mod_get_all_users_of_service (char *service) {
 struct lmodule **ret = NULL;

 emutex_lock (&service_usage_mutex);
 struct stree *ha = streefind (service_usage, service, tree_find_first);

 if (ha) {
  struct service_usage_item *item = ha->value;

  if (item->users) {
   int i = 0;
   for (; item->users[i]; i++) {
    ret = (struct lmodule **)set_noa_add ((void **)ret, item->users[i]);
   }
  }
 }
 emutex_unlock (&service_usage_mutex);

 return ret;
}

struct lmodule **mod_get_all_providers (char *service) {
 struct lmodule **ret = NULL;

 emutex_lock (&service_usage_mutex);
 struct stree *ha = streefind (service_usage, service, tree_find_first);

 if (ha) {
  struct service_usage_item *item = ha->value;

  if (item->provider) {
   int i = 0;
   for (; item->provider[i]; i++) {
    ret = (struct lmodule **)set_noa_add ((void **)ret, item->provider[i]);
   }
  }
 }
 emutex_unlock (&service_usage_mutex);

 return ret;
}

struct lmodule **mod_list_all_enabled_modules () {
 struct lmodule **ret = NULL;
 struct lmodule *m;

 emutex_lock (&mlist_mutex);
 m = mlist;
 while (m) {
  if (m->status & status_enabled) {
   ret = (struct lmodule **)set_noa_add ((void **)ret, m);
  }

  m = m->next;
 }
 emutex_unlock (&mlist_mutex);

 return ret;
}
