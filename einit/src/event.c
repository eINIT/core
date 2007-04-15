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

pthread_mutex_t evf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pof_mutex = PTHREAD_MUTEX_INITIALIZER;

uint32_t cseqid = 0;

void *event_emit (struct einit_event *event, enum einit_event_emit_flags flags) {
 uint32_t subsystem;

 if (!event || !event->type) return NULL;
 subsystem = event->type & EVENT_SUBSYSTEM_MASK;

/* initialise sequence id and timestamp of the event */
  event->seqid = cseqid++;
  event->timestamp = time(NULL);

  struct event_function *cur = event_functions;
  while (cur) {
   if ((cur->type == subsystem) && cur->handler) {
    if (flags & einit_event_flag_spawn_thread) {
     pthread_t threadid;
     if (flags & einit_event_flag_duplicate) {
      struct einit_event *ev = evdup(event);
      ethread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, ev);
     } else
      ethread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, event);
    } else {
     if (flags & einit_event_flag_duplicate) {
      struct einit_event *ev = evdup(event);
      cur->handler (ev);
     } else
      cur->handler (event);
    }
   }
   cur = cur->next;
  }

 if (event->chain_type) {
  event->type = event->chain_type;
  event->chain_type = 0;
  event_emit (event, flags);
 }

 return NULL;
}

void event_listen (enum einit_event_subsystems type, void (* handler)(struct einit_event *)) {
 struct event_function *fstruct = ecalloc (1, sizeof (struct event_function));

 fstruct->type = type & EVENT_SUBSYSTEM_MASK;
 fstruct->handler = handler;

 emutex_lock (&evf_mutex);
  if (event_functions)
   fstruct->next = event_functions;

  event_functions = fstruct;
 emutex_unlock (&evf_mutex);
}

void event_ignore (enum einit_event_subsystems type, void (* handler)(struct einit_event *)) {
 if (!event_functions) return;

 uint32_t ltype = type & EVENT_SUBSYSTEM_MASK;

 emutex_lock (&evf_mutex);
  struct event_function *cur = event_functions;
  struct event_function *prev = NULL;
  while (cur) {
   if ((cur->type==ltype) && (cur->handler==handler)) {
    if (prev == NULL) {
     event_functions = cur->next;
     free (cur);
     cur = event_functions;
    } else {
     prev->next = cur->next;
     free (cur);
     cur = prev->next;
    }
   } else {
    prev = cur;
    cur = cur->next;
   }
  }
 emutex_unlock (&evf_mutex);

 return;
}

void function_register (const char *name, uint32_t version, void const *function) {
 if (!name || !function) return;
 struct exported_function *fstruct = ecalloc (1, sizeof (struct exported_function));

 fstruct->version = version;
 fstruct->function = function;

 emutex_lock (&pof_mutex);
  exported_functions = streeadd (exported_functions, name, (void *)fstruct, sizeof(struct exported_function), NULL);
 emutex_unlock (&pof_mutex);

 free (fstruct);
}

void **function_find (const char *name, const uint32_t version, const char ** sub) {
 if (!exported_functions || !name) return NULL;
 void **set = NULL;
 struct stree *ha = exported_functions;

 emutex_lock (&pof_mutex);
 if (!sub) {
  ha = streefind (exported_functions, name, TREE_FIND_FIRST);
  while (ha) {
   struct exported_function *ef = ha->value;
   if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);
   ha = streefind (ha, name, TREE_FIND_NEXT);
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

   ha = streefind (exported_functions, n, TREE_FIND_FIRST);

   while (ha) {

    struct exported_function *ef = ha->value;
    if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);

    ha = streefind (ha, n, TREE_FIND_NEXT);
   }
  }

  if (n) free (n);
 }
 emutex_unlock (&pof_mutex);

 return set;
}

void *function_find_one (const char *name, const uint32_t version, const char ** sub) {
 void **t = function_find(name, version, sub);
 void *f = (t? t[0] : NULL);

 if (t) free (t);

 return f;
}

void function_unregister (const char *name, uint32_t version, void const *function) {
 if (!exported_functions) return;
 struct stree *ha = exported_functions;

 emutex_lock (&pof_mutex);
 ha = streefind (exported_functions, name, TREE_FIND_FIRST);
 while (ha) {
  struct exported_function *ef = ha->value;
  if (ef && (ef->version == version)) {
   exported_functions = streedel (ha);
   ha = streefind (exported_functions, name, TREE_FIND_FIRST);
  } else
   ha = streefind (exported_functions, name, TREE_FIND_NEXT);
 }
 emutex_unlock (&pof_mutex);

 return;
}

char *event_code_to_string (const uint32_t code) {
 switch (code) {
  case einit_core_update_configuration:   return "core/update-configuration";
  case einit_core_module_update:          return "core/module-status-update";
  case einit_core_service_update:         return "core/service-status-update";
  case einit_core_configuration_update:   return "core/configuration-status-update";

  case einit_mount_do_update:              return "mount/update";

  case einit_feedback_module_status: return "feedback/module";
  case einit_feedback_plan_status:   return "feedback/plan";
  case einit_feedback_notice:        return "feedback/notice";
 }

 switch (code & EVENT_SUBSYSTEM_MASK) {
  case einit_event_subsystem_core:    return "core/{unknown}";
  case einit_event_subsystem_ipc:      return "ipc/{unknown}";
  case einit_event_subsystem_mount:    return "mount/{unknown}";
  case einit_event_subsystem_feedback: return "feedback/{unknown}";
  case einit_event_subsystem_power:    return "power/{unknown}";
  case einit_event_subsystem_timer:    return "timer/{unknown}";
  case einit_event_subsystem_custom:   return "custom/{unknown}";
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

  if (tcode[1])
   switch (ret) {
    case einit_event_subsystem_core:
     if (strmatch (tcode[1], "update-configuration")) ret = einit_core_update_configuration;
     else if (strmatch (tcode[1], "module-status-update")) ret = einit_core_module_update;
     else if (strmatch (tcode[1], "service-status-update")) ret = einit_core_service_update;
     else if (strmatch (tcode[1], "configuration-status-update")) ret = einit_core_configuration_update;
     break;
    case einit_event_subsystem_mount:
     if (strmatch (tcode[1], "update")) ret = einit_mount_do_update;
     break;
    case einit_event_subsystem_feedback:
     if (strmatch (tcode[1], "module")) ret = einit_feedback_module_status;
     else if (strmatch (tcode[1], "plan")) ret = einit_feedback_plan_status;
     else if (strmatch (tcode[1], "notice")) ret = einit_feedback_notice;
     break;
    case einit_event_subsystem_power:
     if (strmatch (tcode[1], "reset-scheduled")) ret = einit_power_reset_scheduled;
     else if (strmatch (tcode[1], "reset-imminent")) ret = einit_power_reset_imminent;
     else if (strmatch (tcode[1], "mps-down-scheduled")) ret = einit_power_down_scheduled;
     else if (strmatch (tcode[1], "mps-down-imminent")) ret = einit_power_down_imminent;
     break;
   }

  free (tcode);
 }

 return ret;
}
