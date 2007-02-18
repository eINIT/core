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

pthread_mutex_t evf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pof_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
pthread_mutex_t event_logbuffer_mutex = PTHREAD_MUTEX_INITIALIZER;
struct event_ringbuffer_node *event_logbuffer = NULL;
#endif

uint32_t cseqid = 0;

void *event_emit (struct einit_event *event, uint16_t flags) {
 uint32_t subsystem;
#ifdef DEBUG
 struct event_ringbuffer_node *new_logbuffer_node;
#endif

 if (!event || !event->type) return NULL;
 subsystem = event->type & EVENT_SUBSYSTEM_MASK;

/* initialise sequence id and timestamp of the event */
  event->seqid = cseqid++;
  event->timestamp = time(NULL);

#ifdef DEBUG
/* initialise copy for the log-ringbuffer */
  new_logbuffer_node = emalloc (sizeof(struct event_ringbuffer_node));

  new_logbuffer_node->type = event->type;
  new_logbuffer_node->set = event->set;
  new_logbuffer_node->string = (event->string ? estrdup (event->string) : NULL);
  new_logbuffer_node->integer = event->integer;
  new_logbuffer_node->status = event->status;
  new_logbuffer_node->task = event->task;
  new_logbuffer_node->flag = event->flag;
  new_logbuffer_node->seqid = event->seqid;
  new_logbuffer_node->timestamp = event->timestamp;
  new_logbuffer_node->para = event->para;

  if (event_logbuffer) {
   new_logbuffer_node->next = event_logbuffer->next;
   new_logbuffer_node->previous = event_logbuffer;

   event_logbuffer->next->previous = new_logbuffer_node;
   event_logbuffer->next = new_logbuffer_node;
  } else {
   new_logbuffer_node->previous = new_logbuffer_node;
   new_logbuffer_node->next = new_logbuffer_node;
  }
  event_logbuffer = new_logbuffer_node;
#endif

  struct event_function *cur = event_functions;
  while (cur) {
   if ((cur->type == subsystem) && cur->handler) {
    if (flags & EINIT_EVENT_FLAG_SPAWN_THREAD) {
     pthread_t threadid;
     if (flags & EINIT_EVENT_FLAG_DUPLICATE) {
      struct einit_event *ev = evdup(event);
      pthread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, ev);
     } else
      pthread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, event);
    } else {
     if (flags & EINIT_EVENT_FLAG_DUPLICATE) {
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

void event_listen (uint32_t type, void (*handler)(struct einit_event *)) {
 struct event_function *fstruct = ecalloc (1, sizeof (struct event_function));

 fstruct->type = type & EVENT_SUBSYSTEM_MASK;
 fstruct->handler = handler;

 pthread_mutex_lock (&evf_mutex);
  if (event_functions)
   fstruct->next = event_functions;

  event_functions = fstruct;
 pthread_mutex_unlock (&evf_mutex);
}

void event_ignore (uint32_t type, void (*handler)(struct einit_event *)) {
 if (!event_functions) return;

 type &= EVENT_SUBSYSTEM_MASK;

 pthread_mutex_lock (&evf_mutex);
  struct event_function *cur = event_functions;
  struct event_function *prev = NULL;
  while (cur) {
   if ((cur->type==type) && (cur->handler==handler)) {
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
 pthread_mutex_unlock (&evf_mutex);

 return;
}

void function_register (char *name, uint32_t version, void *function) {
 if (!name || !function) return;
 struct exported_function *fstruct = ecalloc (1, sizeof (struct exported_function));

 fstruct->version = version;
 fstruct->function = function;

 pthread_mutex_lock (&pof_mutex);
  exported_functions = streeadd (exported_functions, name, (void *)fstruct, sizeof(struct exported_function), NULL);
 pthread_mutex_unlock (&pof_mutex);

 free (fstruct);
}

void **function_find (char *name, uint32_t version, char **sub) {
 if (!exported_functions || !name) return NULL;
 void **set = NULL;
 struct stree *ha = exported_functions;

 pthread_mutex_lock (&pof_mutex);
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
 pthread_mutex_unlock (&pof_mutex);

 return set;
}

void *function_find_one (char *name, uint32_t version, char **sub) {
 void **t = function_find(name, version, sub);
 void *f = (t? t[0] : NULL);

 if (t) free (t);

 return f;
}

void function_unregister (char *name, uint32_t version, void *function) {
 if (!exported_functions) return;
 struct stree *ha = exported_functions;

 pthread_mutex_lock (&pof_mutex);
 ha = streefind (exported_functions, name, TREE_FIND_FIRST);
 while (ha) {
  struct exported_function *ef = ha->value;
  if (ef && (ef->version == version)) {
   exported_functions = streedel (ha);
   ha = streefind (exported_functions, name, TREE_FIND_FIRST);
  } else
   ha = streefind (exported_functions, name, TREE_FIND_NEXT);
 }
 pthread_mutex_unlock (&pof_mutex);

 return;
}

char *event_code_to_string (uint32_t code) {
 switch (code) {
  case EVE_UPDATE_CONFIGURATION:   return "core/update-configuration";
  case EVE_MODULE_UPDATE:          return "core/module-status-update";
  case EVE_SERVICE_UPDATE:         return "core/service-status-update";
  case EVE_CONFIGURATION_UPDATE:   return "core/configuration-status-update";

  case EVE_DO_UPDATE:              return "mount/update";

  case EVE_FEEDBACK_MODULE_STATUS: return "feedback/module";
  case EVE_FEEDBACK_PLAN_STATUS:   return "feedback/plan";
  case EVE_FEEDBACK_NOTICE:        return "feedback/notice";
 }

 switch (code & EVENT_SUBSYSTEM_MASK) {
  case EVENT_SUBSYSTEM_EINIT:    return "core/{unknown}";
  case EVENT_SUBSYSTEM_IPC:      return "ipc/{unknown}";
  case EVENT_SUBSYSTEM_MOUNT:    return "mount/{unknown}";
  case EVENT_SUBSYSTEM_FEEDBACK: return "feedback/{unknown}";
  case EVENT_SUBSYSTEM_POWER:    return "power/{unknown}";
  case EVENT_SUBSYSTEM_TIMER:    return "timer/{unknown}";
  case EVENT_SUBSYSTEM_CUSTOM:   return "custom/{unknown}";
 }

 return "unknown/custom";
}

uint32_t event_string_to_code (char *code) {
 char **tcode = str2set ('/', code);
 uint32_t ret = EVENT_SUBSYSTEM_CUSTOM;

 if (tcode) {
  if (!strcmp (tcode[0], "core"))          ret = EVENT_SUBSYSTEM_EINIT;
  else if (!strcmp (tcode[0], "ipc"))      ret = EVENT_SUBSYSTEM_IPC;
  else if (!strcmp (tcode[0], "mount"))    ret = EVENT_SUBSYSTEM_MOUNT;
  else if (!strcmp (tcode[0], "feedback")) ret = EVENT_SUBSYSTEM_FEEDBACK;
  else if (!strcmp (tcode[0], "power"))    ret = EVENT_SUBSYSTEM_POWER;
  else if (!strcmp (tcode[0], "timer"))    ret = EVENT_SUBSYSTEM_TIMER;

  if (tcode[1])
   switch (ret) {
    case EVENT_SUBSYSTEM_EINIT:
     if (!strcmp (tcode[1], "update-configuration")) ret = EVE_UPDATE_CONFIGURATION;
     else if (!strcmp (tcode[1], "module-status-update")) ret = EVE_MODULE_UPDATE;
     else if (!strcmp (tcode[1], "service-status-update")) ret = EVE_SERVICE_UPDATE;
     else if (!strcmp (tcode[1], "configuration-status-update")) ret = EVE_CONFIGURATION_UPDATE;
     break;
    case EVENT_SUBSYSTEM_MOUNT:
     if (!strcmp (tcode[1], "update")) ret = EVE_DO_UPDATE;
     break;
    case EVENT_SUBSYSTEM_FEEDBACK:
     if (!strcmp (tcode[1], "module")) ret = EVE_FEEDBACK_MODULE_STATUS;
     else if (!strcmp (tcode[1], "plan")) ret = EVE_FEEDBACK_PLAN_STATUS;
     else if (!strcmp (tcode[1], "notice")) ret = EVE_FEEDBACK_NOTICE;
     break;
    case EVENT_SUBSYSTEM_POWER:
     if (!strcmp (tcode[1], "reset-scheduled")) ret = EVENT_POWER_RESET_SCHEDULED;
     else if (!strcmp (tcode[1], "reset-imminent")) ret = EVENT_POWER_RESET_IMMINENT;
     else if (!strcmp (tcode[1], "mps-down-scheduled")) ret = EVENT_POWER_DOWN_SCHEDULED;
     else if (!strcmp (tcode[1], "mps-down-imminent")) ret = EVENT_POWER_DOWN_IMMINENT;
     break;
   }

  free (tcode);
 }

 return ret;
}

// event-system ipc-handler
void event_ipc_handler(struct einit_event *event) {
 if (!event || !event->set) return;
 char **argv = (char **) event->set;
 int argc = setcount (event->set);

 if (argc >= 2) {
  if (!strcmp (argv[0], "update") && !strcmp (argv[1], "configuration")) {
   struct einit_event nev = evstaticinit(EVE_UPDATE_CONFIGURATION);
   nev.string = argv[2];

   if (nev.string)
    fprintf (stderr, "event-subsystem: updating configuration with file %s\n", argv[2]);
   else
    fprintf (stderr, "event-subsystem: updating configuration\n");
   event_emit (&nev, EINIT_EVENT_FLAG_BROADCAST);

   evstaticdestroy(nev);

   if (!event->flag) event->flag = 1;
  } else if (!strcmp (argv[0], "emit-event")) {
   struct einit_event nev = evstaticinit(event_string_to_code(argv[1]));
   nev.string = argv[2];
   nev.set = (void **)(argv+2);

   event_emit (&nev, EINIT_EVENT_FLAG_BROADCAST);

   evstaticdestroy(nev);

   if (!event->flag) event->flag = 1;
  }
#ifdef DEBUG
   else if (!strcmp (argv[0], "list")) {
   if (!strcmp (argv[1], "event-log")) {
    char textbuffer[2048];

    if (event_logbuffer) {
     struct event_ringbuffer_node *cnode = event_logbuffer;

     do {
      char print = 0, *vstring;
      if (cnode->string) {
       strtrim(cnode->string);
       vstring = cnode->string;

       print++;
      } else {
       vstring = "- no verbose message -";
       if (!(options & EIPC_ONLY_RELEVANT)) print++;
      }

      if (print)
       fprintf ((FILE *)event->para, "[%6i] %s: %s\n", cnode->seqid, event_code_to_string(cnode->type), vstring);

      cnode = cnode->next;
     } while (cnode != event_logbuffer);
    } else {
     fputs (" - log buffer empty -\n", (FILE *)event->para);
    }

    if (!event->flag) event->flag = 1;
   }
  }
#endif
 }
}
