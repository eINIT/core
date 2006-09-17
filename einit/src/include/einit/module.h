/*
 *  module.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#ifndef _MODULE_H
#define _MODULE_H

#include <pthread.h>
#include <sys/types.h>
#include <stdint.h>
#include <einit/config.h>
#include <einit/scheduler.h>
#include <einit/utility.h>
#include <einit/event.h>

#define EINIT_OPT_WAIT 8
#define EINIT_OPT_ONCE 16
#define EINIT_OPT_KEEPALIVE 32

#define EINIT_MOD_LOADER 1
#define EINIT_MOD_FEEDBACK 2
#define EINIT_MOD_EXEC 4

#define MOD_ENABLE 0x0001
#define MOD_DISABLE 0x0002
#define MOD_RELOAD 0x0004
#define MOD_RESET 0x0008
#define MOD_FEEDBACK_SHOW 0x0100
#define MOD_SCHEDULER 0x1000
#define MOD_SCHEDULER_PLAN_COMMIT_START 0x1001
#define MOD_SCHEDULER_PLAN_COMMIT_FINISH 0x1002

#define MOD_LOCKED 0x8000

#define STATUS_IDLE 0x0000
#define STATUS_OK 0x8003
#define STATUS_ABORTED 0x8002
#define STATUS_FAIL 0x0100
#define STATUS_FAIL_REQ 0x0300
#define STATUS_DONE 0x8000
#define STATUS_WORKING 0x0010
#define STATUS_ENABLING 0x0011
#define STATUS_DISABLING 0x0012
#define STATUS_RELOADING 0x0013
#define STATUS_ENABLED 0x0401
#define STATUS_DISABLED 0x0802

#define SERVICE_NOT_IN_USE              0x0001
#define SERVICE_REQUIREMENTS_MET        0x0002
#define SERVICE_IS_REQUIRED             0x0004
#define SERVICE_IS_PROVIDED             0x0008
#define SERVICE_UPDATE                  0x0100

#define SERVICE_GET_ALL_PROVIDED        0x0010
#define SERVICE_GET_SERVICES_THAT_USE   0x0020
#define SERVICE_GET_SERVICES_USED_BY    0x0040

#define SERVICE_ADD_GROUP_PROVIDER      0x0200

struct smodule {
 int eiversion;
 int version;
 int mode;
 uint32_t options;
 char *name;
 char *rid;
 char **provides;
 char **requires;
 char **notwith;
};

struct lmodule {
 void *sohandle;
 int (*enable)  (void *, struct einit_event *);
 int (*disable) (void *, struct einit_event *);
 int (*reset) (void *, struct einit_event *);
 int (*reload) (void *, struct einit_event *);
 int (*cleanup) (struct lmodule *);
 uint32_t status;
 void *param;
 pthread_mutex_t mutex;
 pthread_mutex_t imutex;
 struct smodule *module;
 struct lmodule *next;
 uint32_t fbseq;
};

struct service_usage_item {
 struct lmodule **provider;  /* the modules that currently provide this service */
 struct lmodule **users;     /* the modules that currently use this service */
};

struct uhash *service_usage;

// scan for modules (i.e. load their .so and add it to the list)
int mod_scanmodules ();

// free the chain of loaded modules and unload the .so-files
int mod_freemodules ();

// add a module to the main chain of modules
struct lmodule *mod_add (void *, int (*)(void *, struct einit_event *), int (*)(void *, struct einit_event *), void *, struct smodule *);

// find a module
struct lmodule *mod_find (char *rid, unsigned int options);

// do something to a module
int mod (unsigned int, struct lmodule *);

#define mod_enable(lname) mod (MOD_ENABLE, lname)
#define mod_disable(lname) mod (MOD_DISABLE, lname)

// event handler
void mod_event_handler(struct einit_event *);

// service usage
uint16_t service_usage_query (uint16_t, struct lmodule *, char *);
char **service_usage_query_cr (uint16_t, struct lmodule *, char *);
uint16_t service_usage_query_group (uint16_t, struct lmodule *, char *);

// use this to tell einit that there is new feedback-information
// don't rely on this to be a macro!
#define status_update(a) \
 event_emit(a, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE); \
 if (a->task & MOD_FEEDBACK_SHOW) a->task ^= MOD_FEEDBACK_SHOW; a->string = NULL; a->integer++

#endif
