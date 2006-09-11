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
#define MOD_DISABLE_UNSPEC 0x1000
#define MOD_DISABLE_UNSPEC_FEEDBACK 0x2000
#define MOD_FEEDBACK_SHOW 0x0100
#define MOD_SCHEDULER 0x1000
#define MOD_SCHEDULER_PLAN_COMMIT_START 0x1001
#define MOD_SCHEDULER_PLAN_COMMIT_FINISH 0x1002

#define MOD_PLAN_GROUP_SEQ_ANY 0x0010
#define MOD_PLAN_GROUP_SEQ_ANY_IOP 0x0020
#define MOD_PLAN_GROUP_SEQ_MOST 0x0040
#define MOD_PLAN_GROUP_SEQ_ALL 0x0080
#define MOD_PLAN_GROUP 0x0100
#define MOD_PLAN_OK 0x0001
#define MOD_PLAN_FAIL 0x0002
#define MOD_PLAN_IDLE 0x0004

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

#define MOD_P2H_PROVIDES 0x0001
#define MOD_P2H_PROVIDES_NOBACKUP 0x0002
#define MOD_P2H_REQUIRES 0x0003
#define MOD_P2H_LIST 0x0004

#ifdef __cplusplus
extern "C"
{
#endif

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
 struct smodule *module;
 struct lmodule *next;
};

struct mloadplan {
 uint32_t task;
 struct lmodule *mod;
 struct mloadplan **group;  /* elements of the tree that form this group */
 struct mloadplan **left;  /* elements of the tree to load on failure */
 struct mloadplan **right; /* elements of the tree to load on success */
 struct mloadplan **orphaned; /* orphaned elements */
 char **unsatisfied;
 char **unavailable;
 char **requires;
 char **provides;
 uint32_t position, options;
 pthread_mutex_t mutex;
};

//struct mloadplan_node {
// uint32_t task;
// struct lmodule *mod;
// struct mloadplan_node **group;  /* elements of the tree that form this group */
// struct mloadplan_node **left;  /* elements of the tree to load on failure */
// struct mloadplan_node **right; /* elements of the tree to load on success */
// char **unsatisfied;
// char **unavailable;
// char **requires;
// char **provides;
// uint32_t position, options;
// pthread_mutex_t mutex;
//};

extern struct lmodule mdefault;

// scans for modules (i.e. load their .so and add it to the list)
int mod_scanmodules ();

// frees the chain of loaded modules and unloads the .so-files
int mod_freemodules ();

// adds a module to the main chain of modules
struct lmodule *mod_add (void *, int (*)(void *, struct einit_event *), int (*)(void *, struct einit_event *), void *, struct smodule *);

// find a module
struct lmodule *mod_find (char *rid, unsigned int options);

// do something to a module
int mod (unsigned int, struct lmodule *);

#define mod_enable(lname) mod (MOD_ENABLE, lname)
#define mod_disable(lname) mod (MOD_DISABLE, lname)

struct uhash *mod_plan2hash (struct mloadplan *, struct uhash *, int);

// create a plan for loading a set of atoms
struct mloadplan *mod_plan (struct mloadplan *, char **, unsigned int);

// actually do what the plan says
unsigned int mod_plan_commit (struct mloadplan *);
// free all of the resources of the plan
int mod_plan_free (struct mloadplan *);

// event handler
void mod_event_handler(struct einit_event *);

#ifdef DEBUG
// write out a plan-struct to stdout
void mod_plan_ls (struct mloadplan *);
#endif

// use this to tell einit that there is new feedback-information
// don't rely on this to be a macro!
#define status_update(a) \
 event_emit(a, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE); \
 if (a->task & MOD_FEEDBACK_SHOW) a->task ^= MOD_FEEDBACK_SHOW; a->string = NULL

#ifdef __cplusplus
}
#endif

#endif /* _MODULE_H */
