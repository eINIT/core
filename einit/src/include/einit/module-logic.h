/*
 *  module-logic.h
 *  einit
 *
 *  Created by Magnus Deininger on 11/09/2006.
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

#ifndef _MODULE_LOGIC_H
#define _MODULE_LOGIC_H

#include <pthread.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>

#define MOD_PLAN_GROUP_SEQ_ANY 0x0010
#define MOD_PLAN_GROUP_SEQ_ANY_IOP 0x0020
#define MOD_PLAN_GROUP_SEQ_MOST 0x0040
#define MOD_PLAN_GROUP_SEQ_ALL 0x0080
#define MOD_PLAN_GROUP 0x0100
#define MOD_PLAN_OK 0x0001
#define MOD_PLAN_FAIL 0x0002
#define MOD_PLAN_IDLE 0x0004

#define MOD_P2H_PROVIDES 0x0001
#define MOD_P2H_PROVIDES_NOBACKUP 0x0002
#define MOD_P2H_REQUIRES 0x0003
#define MOD_P2H_LIST 0x0004

struct mloadplan {
 struct uhash *services;
 char **enable;
 char **disable;
 char **reset;
 char **unavailable;
 char **locked;
 uint32_t options;
 struct cfgnode *mode;
 pthread_mutex_t mutex;
};

struct mloadplan_node {
 char *service;
 uint32_t task, status;
 struct lmodule **mod;      /* modules */
 char **group;
 uint32_t options;
 struct mloadplan *plan;
 pthread_mutex_t *mutex;
};

// create a plan for loading a set of atoms
struct mloadplan *mod_plan (struct mloadplan *, char **, unsigned int, struct cfgnode *);

// actually do what the plan says
unsigned int mod_plan_commit (struct mloadplan *);

// free all of the plan's resources
int mod_plan_free (struct mloadplan *);

#ifdef DEBUG
// write out a plan-struct to stdout
void mod_plan_ls (struct mloadplan *);
#endif

#endif
