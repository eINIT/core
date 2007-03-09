/*
 *  event.h
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

#ifndef _EINIT_EVENT_H
#define _EINIT_EVENT_H

#include <inttypes.h>
#include <pthread.h>
#include <einit/tree.h>

#define EVENT_SUBSYSTEM_MASK           0xfffff000
#define EVENT_CODE_MASK                0x00000fff

#define EINIT_EVENT_FLAG_BROADCAST     0x0001
/*!< this should always be specified, although just now it's being ignored */
#define EINIT_EVENT_FLAG_SPAWN_THREAD  0x0002
/*!< use this to tell einit that you don't wish/need to wait for this to return */
#define EINIT_EVENT_FLAG_DUPLICATE     0x0004
/*!< duplicate event data block. important with SPAWN_THREAD */

#define EVENT_SUBSYSTEM_EINIT          0x00001000
#define EVE_PANIC                      0x00001001
/*!< put everyone in the cast range into a state of panic/calm everyone down; status contains a reason */
#define EVE_MODULE_UPDATE              0x00001002
/*!< Module status changing; use the task and status fields to find out just what happened */
#define EVE_SERVICE_UPDATE             0x00001003
/*!< Service availability changing; use the task and status fields to find out just what happened */
#define EVE_CONFIGURATION_UPDATE       0x00001004
/*!< notification of configuration update */
#define EVE_PLAN_UPDATE                0x00001005
/*!< Plan status update */
#define EVE_MODULE_LIST_UPDATE         0x00001006
/*!< notification of module-list updates */

/*!< updated core information: new configuration elements */
#define EVE_UPDATE_CONFIGURATION       0x00001101
/*!< update the configuration */
#define EVE_CHANGE_SERVICE_STATUS      0x00001102
/*!< change status of a service */
#define EVE_SWITCH_MODE                0x00001103
/*!< switch to a different mode */
#define EVE_UPDATE_MODULES             0x00001104
/*!< update the modules */
#define EVE_UPDATE_MODULE              0x00001105
/*!< update this module (in ->para) */


#define EVE_SWITCHING_MODE             0x00001201
#define EVE_MODE_SWITCHED              0x00001202

#define EVENT_SUBSYSTEM_IPC            0x00002000
/*!< incoming IPC request */
#define EVENT_SUBSYSTEM_MOUNT                   0x00003000
#define EVE_DO_UPDATE                           0x00003001
#define EVENT_NODE_MOUNTED                      0x00003002
#define EVE_NEW_MOUNT_LEVEL                     0x00003003
#define EVENT_NODE_UNMOUNTED                    0x00003004
/*!< update mount status */

#define EVENT_SUBSYSTEM_FEEDBACK                0x00004000
#define EVE_FEEDBACK_MODULE_STATUS              0x00004001
/*!< the para field specifies a module that caused the feedback */
#define EVE_FEEDBACK_PLAN_STATUS                0x00004002
#define EVE_FEEDBACK_NOTICE                     0x00004003
#define EVENT_FEEDBACK_REGISTER_FD              0x00004004
#define EVENT_FEEDBACK_UNREGISTER_FD            0x00004005

#define EVENT_FEEDBACK_BROKEN_SERVICES          0x00004010
#define EVENT_FEEDBACK_UNRESOLVED_SERVICES      0x00004010

#define EVENT_SUBSYSTEM_POWER			0x00005000
/*!< notify others that the power is failing, has been restored or similar */
#define EVENT_POWER_DOWN_SCHEDULED		0x00005001
/*!< shutdown scheduled */
#define EVENT_POWER_DOWN_IMMINENT		0x00005002
/*!< shutdown going to happen after this event */
#define EVENT_POWER_RESET_SCHEDULED		0x00005003
/*!< reboot scheduled */
#define EVENT_POWER_RESET_IMMINENT		0x00005004
/*!< reboot going to happen after this event */

#define EVENT_POWER_FAILING				0x00005005
/*!< power is failing */
#define EVENT_POWER_FAILURE_IMMINENT	0x00005006
/*!< power is failing NOW */
#define EVENT_POWER_RESTORED			0x00005007
/*!< power was restored */

#define EVENT_SUBSYSTEM_TIMER		0x00006000
/*!< set/receive timer. integer is interpreted as absolute callback time, task as relative */

#define EVENT_SUBSYSTEM_NETWORK		0x00007000
#define EVE_NETWORK_DO_UPDATE		0x00007001

#define EVENT_SUBSYSTEM_CUSTOM		0xfffff000
/*!< custom events; not yet implemented */

/* (generic) IPC-event flags */
#define EIPC_OUTPUT_XML             0x0001
#define EIPC_OUTPUT_ANSI            0x0004
#define EIPC_ONLY_RELEVANT          0x1000
#define EIPC_HELP                   0x0002
#define EIPC_DETACH                 0x0010

#define evstaticinit(ttype) { .type = ttype, .chain_type = 0, /* .type_custom = NULL, */ .set = NULL, .string = NULL, .integer = 0, .status = 0, .task = 0, .flag = 0, .para = NULL, .mutex = PTHREAD_MUTEX_INITIALIZER }
#define evstaticdestroy(ev) { pthread_mutex_destroy (&(ev.mutex)); }

struct einit_event {
 uint32_t type;                  /*!< the event or subsystem to watch */
// char *type_custom;
 /*!< not yet implemented; reserved for custom events */
 void **set;                     /*!< a set that should make sense in combination with the event type */
 char *string;                   /*!< a string */
 int32_t integer, status, task;  /*!< integers */
 unsigned char flag;             /*!< flags */

 uint32_t seqid;
 time_t timestamp;

 uint32_t chain_type;            /*!< the event to be called right after this one */
// char *chain_type_custom;
 /*!< not yet implemented; reserved for custom events */

 void *para;                     /*!< additional parametres */
 pthread_mutex_t mutex;          /*!< mutex for this event to be used by handlers */
};

struct event_function {
 uint32_t type;                          /*!< type of function */
 void (*handler)(struct einit_event *);  /*!< handler function */
 struct event_function *next;            /*!< next function */
};

struct exported_function {
 uint32_t version;                       /*!< API version (for internal use) */
 void const *function;                   /*!< pointer to the function */
};

#ifdef DEBUG
// we don't need this for normal operation...
struct event_ringbuffer_node {
 uint32_t type;
 char *type_custom;
 void **set; /* watch out when trying to use this one, can't duplicate it */
 char *string;
 int32_t integer, status, task;
 unsigned char flag;

 uint32_t seqid;
 time_t timestamp;

 void *para; /* watch out when trying to use this one */

 struct event_ringbuffer_node *previous, *next;
};

struct event_ringbuffer_node *event_logbuffer;
pthread_mutex_t event_logbuffer_mutex;
#endif

void *event_emit (struct einit_event *, const uint16_t);
void event_listen (const uint32_t, void (*)(struct einit_event *));
void event_ignore (const uint32_t, void (*)(struct einit_event *));

void function_register (const char *, uint32_t, void const *);
void function_unregister (const char *, uint32_t, void const *);
void **function_find (const char *, const uint32_t, const char **);
void *function_find_one (const char *, const uint32_t, const char **);

struct event_function *event_functions;
struct stree *exported_functions;

char *event_code_to_string (const uint32_t);
uint32_t event_string_to_code (const char *);

#endif
