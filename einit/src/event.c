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
#include <einit/event.h>
#include <einit/utility.h>

struct event_function *event_functions = NULL;
struct function_list *posted_functions = NULL;
pthread_mutex_t evf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pof_mutex = PTHREAD_MUTEX_INITIALIZER;

void *event_emit (struct einit_event *event, uint16_t flags) {
 if (!event || !event->type) return;
 pthread_mutex_lock (&evf_mutex);
  struct event_function *cur = event_functions;
  while (cur) {
   if ((cur->type & event->type) && cur->handler)
    cur->handler (event);
   cur = cur->next;
  }
 pthread_mutex_unlock (&evf_mutex);
}

void event_listen (uint16_t type, void (*handler)(struct einit_event *)) {
 struct event_function *fstruct = ecalloc (1, sizeof (struct event_function));

 fstruct->type = type;
 fstruct->handler = handler;

 pthread_mutex_lock (&evf_mutex);
  if (event_functions)
   fstruct->next = event_functions;

  event_functions = fstruct;
 pthread_mutex_unlock (&evf_mutex);
}

void function_post (char *name, uint32_t version, void *function) {
 if (!name || !function) return;
 struct function_list *fstruct = ecalloc (1, sizeof (struct function_list));

 fstruct->name = name;
 fstruct->version = version;
 fstruct->function = function;

 pthread_mutex_lock (&pof_mutex);
  if (posted_functions)
   fstruct->next = posted_functions;

  posted_functions = fstruct;
 pthread_mutex_unlock (&pof_mutex);
}

void **function_find (char *name, uint32_t version) {
 if (!posted_functions) return NULL;
 void **set = NULL;

 pthread_mutex_lock (&pof_mutex);
  struct function_list *cur = posted_functions;
  while (cur) {
   if ((cur->version==version) && !strcmp (cur->name, name)) set = setadd (set, (void*)cur->function);
   cur = cur->next;
  }
 pthread_mutex_unlock (&pof_mutex);

 return set;
}
