/*
 *  event.h
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

#ifndef _EINIT_EVENT_H
#define _EINIT_EVENT_H

#include <inttypes.h>
#include <pthread.h>

#define EINIT_EVENT_FLAG_BROADCAST	0x00000001
 /* this should always be specified, although just now it's being ignored */
#define EINIT_EVENT_FLAG_SPAWN_THREAD	0x00000002
 /* use this to tell einit that you don't wish/need to wait for this to return */
#define EINIT_EVENT_FLAG_DUPLICATE	0x00000004
 /* duplicate event data block. important with SPAWN_THREAD */

#define EINIT_EVENT_TYPE_IPC		0x00000001
 /* incoming IPC request */
#define EINIT_EVENT_TYPE_NEED_MODULE	0x00000002
 /* this is to be used in case a module finds out it doesn't have everything it needs, yet */
#define EINIT_EVENT_TYPE_MOUNT_UPDATE	0x00000004
 /* update mount status */
#define EINIT_EVENT_TYPE_FEEDBACK	0x00000008
 /* the para field specifies a module that caused the feedback */
#define EINIT_EVENT_TYPE_NOTICE		0x00000010
 /* use the flag field to specify a severity, 0+ is critical, 6+ is important, etc...  */
#define EINIT_EVENT_TYPE_POWER		0x00000020
 /* notify others that the power is failing, has been restored or similar */
#define EINIT_EVENT_TYPE_TIMER		0x00000040
 /* set/receive timer. integer is interpreted as absolute callback time, task as relative */

#define EINIT_EVENT_TYPE_PANIC		0x00000080
/*!< put everyone in the cast range into a state of panic/calm everyone down; status contains a reason */

#define EINIT_EVENT_MODULE_STATUS_UPDATE	0x00000100
/*!< Module status changing; use the task and status fields to find out just what happened */
#define EINIT_EVENT_SERVICE_UPDATE		0x00000200
/*!< Service availability changing; use the task and status fields to find out just what happened */

#define EINIT_EVENT_TYPE_CUSTOM		0x80000000
/*!< custom events; not yet implemented */

#define evstaticinit(ttype) { .type = ttype, .type_custom = NULL, .set = NULL, .string = NULL, .integer = 0, .status = 0, .task = 0, .flag = 0, .para = NULL, .mutex = PTHREAD_MUTEX_INITIALIZER }
#define evstaticdestroy(ev) { pthread_mutex_destroy (&(ev.mutex)); }

struct einit_event {
 uint32_t type;                  /* OR some EINIT_EVENT_TYPE_* constants to specify the event type*/
 char *type_custom;              /* not yet implemented; reserved for custom events */
 void **set;                     /* a set that should make sense in combination with the event type */
 char *string;                   /* a string */
 int32_t integer, status, task;  /* integers */
 unsigned char flag;             /* flags */
 void *para;                     /* additional parametres */
 pthread_mutex_t mutex;          /* mutex for this event to be used by handlers */
};

struct event_function {
 uint16_t type;                          /* type of function */
 void (*handler)(struct einit_event *);  /* handler function */
 struct event_function *next;            /* next function */
};

struct exported_function {
 uint32_t version;                       /* API version (for internal use) */
 void *function;                         /* pointer to the function */
};

struct event_function *event_functions;

void *event_emit (struct einit_event *, uint16_t);
void event_listen (uint16_t, void (*)(struct einit_event *));
void event_ignore (uint16_t, void (*)(struct einit_event *));

void function_register (char *, uint32_t, void *);
void function_unregister (char *, uint32_t, void *);
void **function_find (char *, uint32_t, char **);

#define event_emit_flag(a, b) {\
	struct einit_event *c = ecalloc (1, sizeof(struct einit_event));\
	c->type = a;\
	c->flag = b;\
	event_emit (c, EINIT_EVENT_FLAG_BROADCAST);\
	//free (c);\
	}

#endif
