/*
 *  event.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 25/06/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <einit/config.h>
#include <einit/event.h>
#include <einit/utility.h>

struct event_function *event_functions = NULL;
struct uhash *exported_functions = NULL;
pthread_mutex_t evf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pof_mutex = PTHREAD_MUTEX_INITIALIZER;

void *event_emit (struct einit_event *event, uint16_t flags) {
 uint32_t subsystem;
 if (!event || !event->type) return;
 subsystem = event->type & EVENT_SUBSYSTEM_MASK;

 pthread_mutex_lock (&evf_mutex);
  struct event_function *cur = event_functions;
  while (cur) {
   if ((cur->type == subsystem) && cur->handler) {
    if (flags & EINIT_EVENT_FLAG_SPAWN_THREAD) {
     pthread_t threadid;
     if (flags & EINIT_EVENT_FLAG_DUPLICATE) {
      struct einit_event *ev = evdup(event);
      pthread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, ev);
//      evdestroy (ev);
     } else
      pthread_create (&threadid, &thread_attribute_detached, (void *(*)(void *))cur->handler, event);
    } else {
     if (flags & EINIT_EVENT_FLAG_DUPLICATE) {
      struct einit_event *ev = evdup(event);
      cur->handler (ev);
//      evdestroy (ev);
     } else
      cur->handler (event);
    }
   }
   cur = cur->next;
  }
 pthread_mutex_unlock (&evf_mutex);
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
  exported_functions = hashadd (exported_functions, name, (void *)fstruct, sizeof(struct exported_function), NULL);
 pthread_mutex_unlock (&pof_mutex);

 free (fstruct);
}

void **function_find (char *name, uint32_t version, char **sub) {
 if (!exported_functions || !name) return NULL;
 void **set = NULL;
 struct uhash *ha = exported_functions;

/* char tmp[2048];
 snprintf (tmp, 2048, "looking for %s in %zx", name, ha);
 puts (tmp); */

 pthread_mutex_lock (&pof_mutex);
 if (!sub) {
  while (ha = hashfind (ha, name)) {
/*   char tmp[2048];
   snprintf (tmp, 2048, "returned with %zx", ha);
   puts (tmp);*/
   struct exported_function *ef = ha->value;
   if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);
   ha = hashnext (ha);
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
   ha = exported_functions;
   while (ha = hashfind (ha, n)) {
/*    char tmp[2048];
    snprintf (tmp, 2048, "returned with %zx", ha);
    puts (tmp);*/

    struct exported_function *ef = ha->value;
    if (ef && (ef->version == version)) set = setadd (set, (void*)ef->function, -1);

    ha = hashnext (ha);
   }
  }

  if (n) free (n);
 }
 pthread_mutex_unlock (&pof_mutex);

 return set;
}

void function_unregister (char *name, uint32_t version, void *function) {
 if (!exported_functions) return;
 struct uhash *ha = exported_functions;

 pthread_mutex_lock (&pof_mutex);
 while (ha = hashfind (ha, name)) {
  struct exported_function *ef = ha->value;
  if (ef && (ef->version == version)) {
   exported_functions = hashdel (exported_functions, ha);
   ha = exported_functions;
   if (!ha) break;
  } else
   ha = hashnext (ha);
 }
 pthread_mutex_unlock (&pof_mutex);

 return;
}
