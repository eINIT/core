/*
 *  event.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 25/06/2006.
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

#include <inttypes.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/config.h>
#include <einit/event.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/bitch.h>
#include <errno.h>
#include <einit/itree.h>

pthread_mutex_t evf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pof_mutex = PTHREAD_MUTEX_INITIALIZER;

struct itree *event_handlers = NULL;

uint32_t cseqid = 0;

struct wakeup_data {
 enum einit_event_code code;
 struct lmodule *module;
 struct wakeup_data *next;
};

pthread_mutex_t event_wakeup_mutex = PTHREAD_MUTEX_INITIALIZER;

struct wakeup_data *event_wd = NULL;

extern time_t event_snooze_time;

void event_do_wakeup_calls (enum einit_event_code c) {
 struct wakeup_data *d = event_wd;

 while (d) {
  if (d->code == c) {
   struct lmodule *m = NULL;
   emutex_lock (&event_wakeup_mutex);
   if (d->module && (d->module->status & status_suspended))
    m = d->module;
   emutex_unlock (&event_wakeup_mutex);

   if (m) {
    mod(einit_module_resume, m, NULL);
    time_t nt = time(NULL) + 60;
    emutex_lock (&event_wakeup_mutex);
    if (!event_snooze_time || ((event_snooze_time+30) < nt)) {
     event_snooze_time = nt;
     emutex_unlock (&event_wakeup_mutex);

     {
      struct einit_event ev = evstaticinit (einit_timer_set);
      ev.integer = event_snooze_time;

      event_emit (&ev, einit_event_flag_broadcast);

      evstaticdestroy (ev);
     }
    } else
     emutex_unlock (&event_wakeup_mutex);
   }
  }
  d = d->next;
 }
}

struct evt_wrapper_data {
  void (*handler) (struct einit_event *);
  struct einit_event *event;
};

void *event_thread_wrapper (struct evt_wrapper_data *d) {
 if (d) {
  d->handler (d->event);

  evdestroy (d->event);
  efree (d);
 }

 return NULL;
}

void event_subthread_a (struct einit_event *event) {
 uint32_t subsystem = event->type & EVENT_SUBSYSTEM_MASK;
 struct event_function **f = NULL;

 /* initialise sequence id and timestamp of the event */
 event->seqid = cseqid++;
 event->timestamp = time(NULL);

 emutex_lock (&evf_mutex);
 if (event_handlers) {
  struct itree *it = NULL;

  if (event->type != subsystem) {
   it = itreefind (event_handlers, event->type, tree_find_first);

   while (it) {
    f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

    it = itreefind (it, event->type, tree_find_next);
   }
  }

  it = itreefind (event_handlers, subsystem, tree_find_first);

  while (it) {
   f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

   it = itreefind (it, subsystem, tree_find_next);
  }

  it = itreefind (event_handlers, einit_event_subsystem_any, tree_find_first);

  while (it) {
   f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

   it = itreefind (it, einit_event_subsystem_any, tree_find_next);
  }
 }
 emutex_unlock (&evf_mutex);

 if (f) {
  int i = 0;
  for (; f[i]; i++) {
   f[i]->handler(event);
  }

  free (f);
 }

 if (event->chain_type) {
  event->type = event->chain_type;
  event->chain_type = 0;

  event_subthread_a (event);
 }
}

void *event_subthread (struct einit_event *event) {
 if (!event) return NULL;

 event_subthread_a (event);

 if ((event->type & EVENT_SUBSYSTEM_MASK) == einit_event_subsystem_ipc) {
  if (event->argv) efree (event->argv);
 } else {
  if (event->stringset) efree (event->stringset);
 }

 evdestroy (event);

 return NULL;
}

void *event_emit (struct einit_event *event, enum einit_event_emit_flags flags) {
 pthread_t **threads = NULL;
 if (!event || !event->type) return NULL;

 event_do_wakeup_calls (event->type);

 if (flags & einit_event_flag_spawn_thread) {
  struct einit_event *ev = evdup(event);
  if (!ev) return NULL;

  ethread_spawn_detached_run ((void *(*)(void *))event_subthread, (void *)ev);

  return NULL;
 }

 struct event_function **f = NULL;
 uint32_t subsystem = event->type & EVENT_SUBSYSTEM_MASK;

/* initialise sequence id and timestamp of the event */
 event->seqid = cseqid++;
 event->timestamp = time(NULL);

 emutex_lock (&evf_mutex);
 if (event_handlers) {
  struct itree *it = NULL;

  if (event->type != subsystem) {
   it = itreefind (event_handlers, event->type, tree_find_first);

   while (it) {
    f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

    it = itreefind (it, event->type, tree_find_next);
   }
  }

  it = itreefind (event_handlers, subsystem, tree_find_first);

  while (it) {
   f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

   it = itreefind (it, subsystem, tree_find_next);
  }

  it = itreefind (event_handlers, einit_event_subsystem_any, tree_find_first);

  while (it) {
   f = (struct event_function **)setadd ((void **)f, it->value, sizeof (struct event_function));

   it = itreefind (it, einit_event_subsystem_any, tree_find_next);
  }
 }
 emutex_unlock (&evf_mutex);

 if (f) {
  int i = 0;
  for (; f[i]; i++) {
   if (flags & einit_event_flag_spawn_thread_multi_wait) {
    if (f[i+1]) {
     pthread_t *threadid = emalloc (sizeof (pthread_t));
     struct evt_wrapper_data *d = emalloc (sizeof (struct evt_wrapper_data));

     d->event = evdup(event);
     d->handler = f[i]->handler;

     ethread_create (threadid, NULL, (void *(*)(void *))event_thread_wrapper, d);
     threads = (pthread_t **)setadd ((void **)threads, threadid, SET_NOALLOC);
    } else {
/* do a shortcut so we don't create a thread for the last thing to spawn */
     f[i]->handler (event);
    }
   } else
    f[i]->handler (event);
  }

  free (f);
 }

 if (event->chain_type) {
  event->type = event->chain_type;
  event->chain_type = 0;
  event_emit (event, flags);
 }

 if (threads) {
  int i = 0;

  for (; threads[i]; i++) {
   pthread_join (*(threads[i]), NULL);

   efree (threads[i]);
  }

  efree (threads);
 }

 return NULL;
}

void event_listen (enum einit_event_subsystems type, void (* handler)(struct einit_event *)) {
 struct event_function f = { .handler = handler };

 emutex_lock (&evf_mutex);
 event_handlers = itreeadd (event_handlers, type, &f, sizeof(struct event_function));
 emutex_unlock (&evf_mutex);
}

void event_ignore (enum einit_event_subsystems type, void (* handler)(struct einit_event *)) {
 struct itree *it = NULL;
 emutex_lock (&evf_mutex);
 if (event_handlers) {
  it = itreefind (event_handlers, type, tree_find_first);

  while (it) {
   struct event_function *f = it->value;
   if (f->handler == handler) break;

   it = itreefind (it, type, tree_find_next);
  }
 }
 emutex_unlock (&evf_mutex);

 if (it) {
  emutex_lock (&evf_mutex);
  event_handlers = itreedel (it);
  emutex_unlock (&evf_mutex);
 }

 return;
}

void function_register_type (const char *name, uint32_t version, void const *function, enum function_type type, struct lmodule *module) {
 if (!name || !function) return;

 char added = 0;

 if (module) {
  emutex_lock (&pof_mutex);
  struct stree *ha = exported_functions;
  ha = streefind (exported_functions, name, tree_find_first);
  while (ha) {
   struct exported_function *ef = ha->value;
   if (ef && (ef->version == version) && (ef->type == type) && (ef->module == module)) {
    ef->function = function;
    added = 1;
    break;
   }

   ha = streefind (ha, name, tree_find_next);
  }
  emutex_unlock (&pof_mutex);
 }


 if (!added) {
  struct exported_function *fstruct = ecalloc (1, sizeof (struct exported_function));

  fstruct->type     = type;
  fstruct->version  = version;
  fstruct->function = function;
  fstruct->module   = module;

  emutex_lock (&pof_mutex);
   exported_functions = streeadd (exported_functions, name, (void *)fstruct, sizeof(struct exported_function), NULL);
  emutex_unlock (&pof_mutex);

  efree (fstruct);
 }
}

void function_unregister_type (const char *name, uint32_t version, void const *function, enum function_type type, struct lmodule *module) {
 if (!exported_functions) return;
 struct stree *ha = exported_functions;

 emutex_lock (&pof_mutex);
 ha = streefind (exported_functions, name, tree_find_first);
 while (ha) {
  struct exported_function *ef = ha->value;
  if (ef && (ef->version == version) && (ef->type == type) && (ef->module == module)) {
//   exported_functions = streedel (ha);
   ef->function = NULL;
   ha = streefind (exported_functions, name, tree_find_first);
  }

  ha = streefind (ha, name, tree_find_next);
 }
 emutex_unlock (&pof_mutex);

 return;
}

void **function_find (const char *name, const uint32_t version, const char ** sub) {
 if (!exported_functions || !name) return NULL;
 void **set = NULL;
 struct stree *ha = exported_functions;

 emutex_lock (&pof_mutex);
 if (!sub) {
  ha = streefind (exported_functions, name, tree_find_first);
  while (ha) {
   struct exported_function *ef = ha->value;
   if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);
   ha = streefind (ha, name, tree_find_next);
  }
 } else {
  uint32_t i = 0, k = strlen (name)+1;
  char *n = emalloc (k+1);
  *n = 0;
  strcat (n, name);
  *(n + k - 1) = '-';

  for (; sub[i]; i++) {
   *(n + k) = 0;
   n = erealloc (n, k+1+strlen (sub[i]));
   strcat (n, sub[i]);

   ha = streefind (exported_functions, n, tree_find_first);

   while (ha) {

    struct exported_function *ef = ha->value;
    if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);

    ha = streefind (ha, n, tree_find_next);
   }
  }

  if (n) efree (n);
 }
 emutex_unlock (&pof_mutex);

 return set;
}

void *function_find_one (const char *name, const uint32_t version, const char ** sub) {
 void **t = function_find(name, version, sub);
 void *f = (t? t[0] : NULL);

 if (t) efree (t);

 return f;
}

struct exported_function **function_look_up (const char *name, const uint32_t version, const char **sub) {
 if (!exported_functions || !name) return NULL;
 struct exported_function **set = NULL;
 struct stree *ha = exported_functions;

 emutex_lock (&pof_mutex);
 if (!sub) {
  ha = streefind (exported_functions, name, tree_find_first);
  while (ha) {
   struct exported_function *ef = ha->value;

   if (!(ef->name)) ef->name = ha->key;

   if (ef && (ef->version == version)) set = (struct exported_function **)setadd ((void **)set, (struct exported_function *)ef, -1);
   ha = streefind (ha, name, tree_find_next);
  }
 } else {
  uint32_t i = 0, k = strlen (name)+1;
  char *n = emalloc (k+1);
  *n = 0;
  strcat (n, name);
  *(n + k - 1) = '-';

  for (; sub[i]; i++) {
   *(n + k) = 0;
   n = erealloc (n, k+1+strlen (sub[i]));
   strcat (n, sub[i]);

   ha = streefind (exported_functions, n, tree_find_first);

   while (ha) {
    struct exported_function *ef = ha->value;

    if (!(ef->name)) ef->name = ha->key;

    if (ef && (ef->version == version)) set = (struct exported_function **)setadd ((void **)set, (struct exported_function *)ef, -1);

    ha = streefind (ha, n, tree_find_next);
   }
  }

  if (n) efree (n);
 }
 emutex_unlock (&pof_mutex);

 return set;
}

struct exported_function *function_look_up_one (const char *name, const uint32_t version, const char **sub) {
 struct exported_function **t = function_look_up(name, version, sub);
 struct exported_function *f = (t? t[0] : NULL);

 if (t) efree (t);

 return f;
}

char *event_code_to_string (const uint32_t code) {
 switch (code) {
  case einit_core_panic:                       return "core/panic";
  case einit_core_service_update:              return "core/service-update";
  case einit_core_configuration_update:        return "core/configuration-update";
  case einit_core_plan_update:                 return "core/plan-update";
  case einit_core_module_list_update:          return "core/module-list-update";
  case einit_core_module_list_update_complete: return "core/module-list-update-complete";

  case einit_core_update_configuration:        return "core/update-configuration";
  case einit_core_change_service_status:       return "core/change-service-status";
  case einit_core_switch_mode:                 return "core/switch-mode";
  case einit_core_update_modules:              return "core/update-modules";
  case einit_core_update_module:               return "core/update-module";
  case einit_core_manipulate_services:         return "core/manipulate-services";

  case einit_core_mode_switching:              return "core/mode-switching";
  case einit_core_mode_switch_done:            return "core/mode-switch-done";
  case einit_core_switching:                   return "core/switching";
  case einit_core_done_switching:              return "core/done-switching";

  case einit_core_suspend_all:                 return "core/suspend-all";
  case einit_core_resume_all:                  return "core/resume-all";

  case einit_core_service_enabling:            return "core/service-enabling";
  case einit_core_service_enabled:             return "core/service-enabled";
  case einit_core_service_disabling:           return "core/service-disabling";
  case einit_core_service_disabled:            return "core/service-disabled";

  case einit_core_crash_data:                  return "core/crash-data";
  case einit_core_recover:                     return "core/recover";
  case einit_core_main_loop_reached:           return "core/main-loop-reached";

  case einit_mount_do_update:                  return "mount/do-update";
  case einit_mount_node_mounted:               return "mount/node-mounted";
  case einit_mount_node_unmounted:             return "mount/node-unmounted";
  case einit_mount_new_mount_level:            return "mount/new-mount-level";

  case einit_feedback_module_status:           return "feedback/module-status";
  case einit_feedback_notice:                  return "feedback/notice";
  case einit_feedback_register_fd:             return "feedback/register-fd";
  case einit_feedback_unregister_fd:           return "feedback/unregister-fd";

  case einit_feedback_broken_services:         return "feedback/broken-services";
  case einit_feedback_unresolved_services:     return "feedback/unresolved-services";

  case einit_feedback_switch_progress:         return "feedback/switch-progress";

  case einit_power_down_scheduled:             return "power/down-scheduled";
  case einit_power_down_imminent:              return "power/down-imminent";
  case einit_power_reset_scheduled:            return "power/reset-scheduled";
  case einit_power_reset_imminent:             return "power/reset-imminent";

  case einit_power_failing:                    return "power/failing";
  case einit_power_failure_imminent:           return "power/failure-imminent";
  case einit_power_restored:                   return "power/restored";

  case einit_timer_tick:                       return "timer/tick";
  case einit_timer_set:                        return "timer/set";
  case einit_timer_cancel:                     return "timer/cancel";

  case einit_network_do_update:                return "network/do-update";

  case einit_process_died:                     return "process/died";

  case einit_boot_early:                       return "boot/early";
  case einit_boot_load_kernel_extensions:      return "boot/load-kernel-extensions";
  case einit_boot_devices_available:           return "boot/devices-available";
  case einit_boot_root_device_ok:              return "boot/root-device-ok";

  case einit_hotplug_add:                      return "hotplug/add";
  case einit_hotplug_remove:                   return "hotplug/remove";
  case einit_hotplug_change:                   return "hotplug/change";
  case einit_hotplug_online:                   return "hotplug/online";
  case einit_hotplug_offline:                  return "hotplug/offline";
  case einit_hotplug_move:                     return "hotplug/move";
  case einit_hotplug_generic:                  return "hotplug/generic";
 }

 switch (code & EVENT_SUBSYSTEM_MASK) {
  case einit_event_subsystem_core:     return "core/{unknown}";
  case einit_event_subsystem_ipc:      return "ipc";
  case einit_event_subsystem_mount:    return "mount/{unknown}";
  case einit_event_subsystem_feedback: return "feedback/{unknown}";
  case einit_event_subsystem_power:    return "power/{unknown}";
  case einit_event_subsystem_timer:    return "timer/{unknown}";

  case einit_event_subsystem_network:  return "network/{unknown}";
  case einit_event_subsystem_process:  return "process/{unknown}";
  case einit_event_subsystem_boot:     return "boot/{unknown}";
  case einit_event_subsystem_hotplug:  return "hotplug/{unknown}";

  case einit_event_subsystem_any:      return "any";
  case einit_event_subsystem_custom:   return "custom";
 }

 return "unknown/custom";
}

uint32_t event_string_to_code (const char *code) {
 char **tcode = str2set ('/', code);
 uint32_t ret = einit_event_subsystem_custom;

 if (tcode) {
  if (strmatch (tcode[0], "core"))          ret = einit_event_subsystem_core;
  else if (strmatch (tcode[0], "ipc"))      ret = einit_event_subsystem_ipc;
  else if (strmatch (tcode[0], "mount"))    ret = einit_event_subsystem_mount;
  else if (strmatch (tcode[0], "feedback")) ret = einit_event_subsystem_feedback;
  else if (strmatch (tcode[0], "power"))    ret = einit_event_subsystem_power;
  else if (strmatch (tcode[0], "timer"))    ret = einit_event_subsystem_timer;
  else if (strmatch (tcode[0], "network"))  ret = einit_event_subsystem_network;
  else if (strmatch (tcode[0], "process"))  ret = einit_event_subsystem_process;
  else if (strmatch (tcode[0], "boot"))     ret = einit_event_subsystem_boot;
  else if (strmatch (tcode[0], "hotplug"))  ret = einit_event_subsystem_hotplug;
  else if (strmatch (tcode[0], "any"))      ret = einit_event_subsystem_any;
  else if (strmatch (tcode[0], "custom"))   ret = einit_event_subsystem_custom;

  if (tcode[1])
   switch (ret) {
    case einit_event_subsystem_core:
     if (strmatch (tcode[1], "panic"))                            ret = einit_core_panic;
     else if (strmatch (tcode[1], "service-update"))              ret = einit_core_service_update;
     else if (strmatch (tcode[1], "configuration-update"))        ret = einit_core_configuration_update;
     else if (strmatch (tcode[1], "plan-update"))                 ret = einit_core_plan_update;
     else if (strmatch (tcode[1], "module-list-update"))          ret = einit_core_module_list_update;
     else if (strmatch (tcode[1], "module-list-update-complete")) ret = einit_core_module_list_update_complete;

     else if (strmatch (tcode[1], "update-configuration"))        ret = einit_core_update_configuration;
     else if (strmatch (tcode[1], "change-service-status"))       ret = einit_core_change_service_status;
     else if (strmatch (tcode[1], "switch-mode"))                 ret = einit_core_switch_mode;
     else if (strmatch (tcode[1], "update-modules"))              ret = einit_core_update_modules;
     else if (strmatch (tcode[1], "update-module"))               ret = einit_core_update_module;
     else if (strmatch (tcode[1], "manipulate-services"))         ret = einit_core_manipulate_services;

     else if (strmatch (tcode[1], "mode-switching"))              ret = einit_core_mode_switching;
     else if (strmatch (tcode[1], "mode-switch-done"))            ret = einit_core_mode_switch_done;
     else if (strmatch (tcode[1], "switching"))                   ret = einit_core_switching;
     else if (strmatch (tcode[1], "done-switching"))              ret = einit_core_done_switching;

     else if (strmatch (tcode[1], "suspend-all"))                 ret = einit_core_suspend_all;
     else if (strmatch (tcode[1], "resume-all"))                  ret = einit_core_resume_all;

     else if (strmatch (tcode[1], "service-enabling"))            ret = einit_core_service_enabling;
     else if (strmatch (tcode[1], "service-enabled"))             ret = einit_core_service_enabled;
     else if (strmatch (tcode[1], "service-disabling"))           ret = einit_core_service_disabling;
     else if (strmatch (tcode[1], "service-disabled"))            ret = einit_core_service_disabled;

     else if (strmatch (tcode[1], "crash-data"))                  ret = einit_core_crash_data;
     else if (strmatch (tcode[1], "recover"))                     ret = einit_core_recover;
     else if (strmatch (tcode[1], "main-loop-reached"))           ret = einit_core_main_loop_reached;
     break;
    case einit_event_subsystem_mount:
     if (strmatch (tcode[1], "do-update"))                        ret = einit_mount_do_update;
     else if (strmatch (tcode[1], "node-mounted"))                ret = einit_mount_node_mounted;
     else if (strmatch (tcode[1], "node-unmounted"))              ret = einit_mount_node_unmounted;
     else if (strmatch (tcode[1], "new-mount-level"))             ret = einit_mount_new_mount_level;
     break;
    case einit_event_subsystem_feedback:
     if (strmatch (tcode[1], "module-status"))                    ret = einit_feedback_module_status;
     else if (strmatch (tcode[1], "notice"))                      ret = einit_feedback_notice;
     else if (strmatch (tcode[1], "register-fd"))                 ret = einit_feedback_register_fd;
     else if (strmatch (tcode[1], "unregister-fd"))               ret = einit_feedback_unregister_fd;

     else if (strmatch (tcode[1], "broken-services"))             ret = einit_feedback_broken_services;
     else if (strmatch (tcode[1], "unresolved-services"))         ret = einit_feedback_unresolved_services;

     else if (strmatch (tcode[1], "switch-progress"))             ret = einit_feedback_switch_progress;
     break;
    case einit_event_subsystem_power:
     if (strmatch (tcode[1], "down-scheduled"))                   ret = einit_power_down_scheduled;
     else if (strmatch (tcode[1], "down-imminent"))               ret = einit_power_down_imminent;
     else if (strmatch (tcode[1], "reset-scheduled"))             ret = einit_power_reset_scheduled;
     else if (strmatch (tcode[1], "reset-imminent"))              ret = einit_power_reset_imminent;

     else if (strmatch (tcode[1], "failing"))                     ret = einit_power_failing;
     else if (strmatch (tcode[1], "failure-imminent"))            ret = einit_power_failure_imminent;
     else if (strmatch (tcode[1], "restored"))                    ret = einit_power_restored;
     break;
    case einit_event_subsystem_timer:
     if (strmatch (tcode[1], "tick"))                             ret = einit_timer_tick;
     else if (strmatch (tcode[1], "set"))                         ret = einit_timer_set;
     else if (strmatch (tcode[1], "cancel"))                      ret = einit_timer_cancel;
     break;
    case einit_event_subsystem_network:
     if (strmatch (tcode[1], "do-update"))                        ret = einit_network_do_update;
     break;
    case einit_event_subsystem_process:
     if (strmatch (tcode[1], "died"))                             ret = einit_process_died;
     break;
    case einit_event_subsystem_boot:
     if (strmatch (tcode[1], "early"))                            ret = einit_boot_early;
     else if (strmatch (tcode[1], "load-kernel-extensions"))      ret = einit_boot_load_kernel_extensions;
     else if (strmatch (tcode[1], "devices-available"))           ret = einit_boot_devices_available;
     else if (strmatch (tcode[1], "root-device-ok"))              ret = einit_boot_root_device_ok;
     break;
    case einit_event_subsystem_hotplug:
     if (strmatch (tcode[1], "add"))                              ret = einit_hotplug_add;
     else if (strmatch (tcode[1], "remove"))                      ret = einit_hotplug_remove;
     else if (strmatch (tcode[1], "change"))                      ret = einit_hotplug_change;
     else if (strmatch (tcode[1], "online"))                      ret = einit_hotplug_online;
     else if (strmatch (tcode[1], "offline"))                     ret = einit_hotplug_offline;
     else if (strmatch (tcode[1], "move"))                        ret = einit_hotplug_move;
     else if (strmatch (tcode[1], "generic"))                     ret = einit_hotplug_generic;
     break;
   }

  efree (tcode);
 }

 return ret;
}

time_t event_timer_register (struct tm *t) {
 struct einit_event ev = evstaticinit (einit_timer_set);
 time_t tr = timegm (t);

 ev.integer = tr;

 event_emit (&ev, einit_event_flag_broadcast);

 evstaticdestroy (ev);

 return tr;
}

time_t event_timer_register_timeout (time_t t) {
 struct einit_event ev = evstaticinit (einit_timer_set);
 time_t tr = time (NULL) + t;

 ev.integer = tr;

 event_emit (&ev, einit_event_flag_broadcast);

 evstaticdestroy (ev);

 return tr;
}

void event_timer_cancel (time_t t) {
 struct einit_event ev = evstaticinit (einit_timer_cancel);

 ev.integer = t;

 event_emit (&ev, einit_event_flag_broadcast);

 evstaticdestroy (ev);
}

void event_wakeup (enum einit_event_code c, struct lmodule *m) {
 struct wakeup_data *d = event_wd;

 while (d) {
  if (d->code == c) {
   if (d->module == m) {
    return;
   } else {
    emutex_lock (&event_wakeup_mutex);
    if (d->module == NULL) {
     d->module = m;
    }
    emutex_unlock (&event_wakeup_mutex);

    if (d->module == m)
     return;
   }
  }
  d = d->next;
 }

 struct wakeup_data *nd = ecalloc (1, sizeof (struct wakeup_data));

 nd->code = c;
 nd->module = m;

 emutex_lock (&event_wakeup_mutex);
 nd->next = event_wd;
 event_wd = nd;
 emutex_unlock (&event_wakeup_mutex);
}

void event_wakeup_cancel (enum einit_event_code c, struct lmodule *m) {
 struct wakeup_data *d = event_wd;

 while (d) {
  if (d->code == c) {
   emutex_lock (&event_wakeup_mutex);
   if (d->module == m)
    d->module = NULL;
   emutex_unlock (&event_wakeup_mutex);
  }
  d = d->next;
 }
}
