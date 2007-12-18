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
struct stree *module_logic_module_preferences = NULL;

struct stree **module_logic_free_on_idle_stree = NULL;

struct lmodule **module_logic_broken_modules = NULL; /* this will need to be cleared upon turning idle */
struct lmodule **module_logic_active_modules = NULL;

char **module_logic_list_enable = NULL;
char **module_logic_list_disable = NULL;
char **module_logic_list_enable_workers = NULL;
char **module_logic_list_disable_workers = NULL;

pthread_mutex_t
 module_logic_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_free_on_idle_stree_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_broken_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_enable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_list_disable_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_active_modules_mutex = PTHREAD_MUTEX_INITIALIZER,
 module_logic_module_preferences_mutex = PTHREAD_MUTEX_INITIALIZER;


void module_logic_einit_event_handler_core_configuration_update (struct einit_event *);

void module_logic_ipc_event_handler (struct einit_event *ev) {
/* update init.d */
/* examine configuration */
/* list services */
}

/* callers of the following two functions need to lock the appropriate mutex on their own! */

struct lmodule **module_logic_find_things_to_enable() {
 if (!module_logic_list_enable) return NULL;

 struct lmodule **rv = NULL;

 return rv;
}

struct lmodule **module_logic_find_things_to_disable() {
 if (!module_logic_list_disable) return NULL;

 struct lmodule **rv = NULL;

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

 /* updating the list of modules does mean we also need to update the list of preferences */
 module_logic_einit_event_handler_core_configuration_update(NULL);
}

/* what we also need is a list of preferences... */

void module_logic_einit_event_handler_core_configuration_update (struct einit_event *ev) {
 struct stree *new_preferences = NULL;

 emutex_lock (&module_logic_module_preferences_mutex);
 struct stree *old_preferences = module_logic_module_preferences;
 module_logic_module_preferences = new_preferences;

 emutex_unlock (&module_logic_module_preferences_mutex);

 emutex_lock (&module_logic_free_on_idle_stree_mutex);
 module_logic_free_on_idle_stree = (struct stree **)setadd ((void **)module_logic_free_on_idle_stree, old_preferences, SET_NOALLOC);
 emutex_unlock (&module_logic_free_on_idle_stree_mutex);
}

/* the following three events are used to make the module logics core do something */

void module_logic_einit_event_handler_core_switch_mode (struct einit_event *ev) {
}

void module_logic_einit_event_handler_core_manipulate_services (struct einit_event *ev) {
}

void module_logic_einit_event_handler_core_change_service_status (struct einit_event *ev) {
}

/* the next two event are feedback from the core, which we use to advance our... plans */

void module_logic_einit_event_handler_core_service_enabled (struct einit_event *ev) {
 emutex_lock (&module_logic_list_enable_mutex);
 module_logic_list_enable = strsetdel (module_logic_list_enable, ev->string);
 module_logic_list_enable_workers = strsetdel (module_logic_list_enable_workers, ev->string);

 struct lmodule **spawn = module_logic_find_things_to_enable();
 emutex_unlock (&module_logic_list_enable_mutex);

 if (spawn)
  module_logic_spawn_set_enable (spawn);
}

void module_logic_einit_event_handler_core_service_disabled (struct einit_event *ev) {
 emutex_lock (&module_logic_list_disable_mutex);
 module_logic_list_disable = strsetdel (module_logic_list_disable, ev->string);
 module_logic_list_disable_workers = strsetdel (module_logic_list_disable_workers, ev->string);

 struct lmodule **spawn = module_logic_find_things_to_disable();
 emutex_unlock (&module_logic_list_disable_mutex);

 if (spawn)
  module_logic_spawn_set_disable (spawn);
}

void module_logic_idle_actions () {
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

/* this is the event we use to "unblock" modules for use in future switches */

void module_logic_einit_event_handler_core_service_update (struct einit_event *ev) {
 if (!(ev->status & status_working)) {
  emutex_lock (&module_logic_active_modules_mutex);
  module_logic_active_modules = (struct lmodule **)setdel ((void **)module_logic_active_modules, ev->para);

  if (!module_logic_active_modules) {
   module_logic_idle_actions();
  }
  emutex_unlock (&module_logic_active_modules_mutex);
 }

 if (ev->status & status_failed) {
  emutex_lock (&module_logic_broken_modules_mutex);
  module_logic_broken_modules = (struct lmodule **)setdel ((void **)module_logic_broken_modules, ev->para);
  emutex_unlock (&module_logic_broken_modules_mutex);
 }
}



int einit_module_logic_v4_cleanup (struct lmodule *this) {
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
// function_register ("module-logic-get-plan-progress", 1, mod_get_plan_progress_f);

 return 0;
}
