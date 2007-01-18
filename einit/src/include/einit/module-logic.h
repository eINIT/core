/*
 *  module-logic.h
 *  einit
 *
 *  Created by Magnus Deininger on 11/09/2006.
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

/*!\file einit/module-logic.h
 * \brief Module-planning logic.
 * \author Magnus Deininger
 *
 * This header file declares all structs and functions that deal with planning and executing mode-switches
 * and enabling / disabling of modules. Client-modules might want to use these functions if they need
 * to perform mode-switches and similar things without using the scheduler.
*/

#ifndef _MODULE_LOGIC_H
#define _MODULE_LOGIC_H

#include <pthread.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/tree.h>

/*!\name Option-Constants: Module-Groups.
 * \{
*/
#define MOD_PLAN_GROUP_SEQ_ANY 0x0010      /*!< Module-Group: Any service needs to get up */
#define MOD_PLAN_GROUP_SEQ_ANY_IOP 0x0020  /*!< Module-Group: Any service needs to get up, services are in order of preference */
#define MOD_PLAN_GROUP_SEQ_MOST 0x0040     /*!< Module-Group: Most services to get up */
#define MOD_PLAN_GROUP_SEQ_ALL 0x0080      /*!< Module-Group: All services need to get up */

#define STATUS_WORKING_FINISH_GROUP 0x0200 /*!< Status Information: Used to tell the recursive plan-node functions that they're a secondary thread spawned to complete loading a group. */
/*! \}*/

/*!\brief Plan
 *
 * This struct is used to keep all the information that is needed for a mode-switch, or a module-status-change.
*/
struct mloadplan {
 struct stree *services;   /*!< Hash of all services that are to be used. */
 char **enable;            /*!< Set of all services that are to be enabled. */
 char **disable;           /*!< Set of all services that are to be disabled. */
 char **reset;             /*!< Set of all services that are to be reset. */
 char **critical;          /*!< Set of critical services: If one of these is not up after the switch, a panic event is emitted */
 char **unavailable;       /*!< Set of services that were to be changed but were not available. */
 char **locked;            /*!< Set of services that were to be changed, but it's not possible to do so. */
 uint32_t options;         /*!< Plan-options; reserved */
 struct cfgnode *mode;     /*!< Pointer to the mode that was used to construct this plan, if applicable. */
 pthread_mutex_t mutex;    /*!< Mutex for this plan. */
 pthread_mutex_t vizmutex; /*!< Mutex for this plan. */

 pthread_t **subthreads;   /*!< Subthreads of this plan that need to be joined. */
 pthread_mutex_t st_mutex; /*!< Mutex for subthread-modifications. */
};

/*!\brief Plan, Node
 *
 * Data node for the mloadplan struct.
*/
struct mloadplan_node {
 char **service;         /*!< Name(s) of this node's service(s). */
 uint32_t status;        /*!< This service's status. */
 uint32_t pos;           /*!< Current position */
 struct lmodule **mod;   /*!< Modules that are to be used. */
 char **group;           /*!< List of other services that form a group to provide this service. */
 uint32_t options;       /*!< Node-options; reserved */
 struct mloadplan *plan; /*!< Pointer to this node's plan. */
 pthread_mutex_t *mutex; /*!< This node's mutex.*/

 char **si_after;        /*!< same as a module's si.after, needed to implement before="" .*/
 char changed;           /*!< indicate whether someone is currently working on this */
// pthread_t thread;       /*!< the thread ID that node got */
};

/*!\brief Create a plan
 *
 * Create a plan for a set of atoms or a mode.
*/
struct mloadplan *mod_plan (struct mloadplan *, char **, unsigned int, struct cfgnode *);

/*!\brief Execute a plan
 *
 * Actually do what the plan says.
*/
unsigned int mod_plan_commit (struct mloadplan *);

/*!\brief Free a plan
 *
 * Free all of the resources that had been associated with a plan.
*/
int mod_plan_free (struct mloadplan *);

#endif
