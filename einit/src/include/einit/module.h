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

/*!@file einit/module.h
 * @brief Module-functions and structs.
 * @author Magnus Deininger
 *
 * This header file declares all structs and functions that deal with modules.
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

/*!@brief On-file module definition
 *
 * The static module definition that is kept in the module's .so-file.
*/
struct smodule {
 int eiversion;         /*!< The version of eINIT that the module was compiled with. */
 int version;           /*!< The module's version; this is currently ignored by eINIT. */
 int mode;              /*!< The module type; should be EINIT_MOD_EXEC for most modules. */
 uint32_t options;      /*!< Module options; this is currently ignored. */
 char *name;            /*!< The real name of the module. */
 char *rid;             /*!< The short ID of the module. */
 char **provides;       /*!< A list of services that this module provides. */
 char **requires;       /*!< A list of services that this module requires. */
 char **notwith;        /*!< A list of services that may not be loaded together with this module; ignored. */
};

/*!@brief In-memory module definition
 *
 * The dynamic module definition that is kept in memory after the module is loaded.
*/
struct lmodule {
 void *sohandle;                                /*!< .so file-handle */
 int (*enable)  (void *, struct einit_event *); /*!< Pointer to the module's enable()-function */
 int (*disable) (void *, struct einit_event *); /*!< Pointer to the module's disable()-function */
 int (*reset) (void *, struct einit_event *);   /*!< Pointer to the module's reset()-function */
 int (*reload) (void *, struct einit_event *);  /*!< Pointer to the module's reload()-function */
 int (*cleanup) (struct lmodule *);             /*!< Pointer to the module's cleanup()-function */
 uint32_t status;                               /*!< Current module status (enabled, disabled, ...) */
 void *param;                                   /*!< Parametre for state-changing functions */
 pthread_mutex_t mutex;	                        /*!< Module-mutex; is used by the mod()-function */
 pthread_mutex_t imutex;                        /*!< Internal module-mutex; to be used by the module */
 struct smodule *module;                        /*!< Pointer to the static module definition */
 struct lmodule *next;                          /*!< Pointer to the next module in the list */
 uint32_t fbseq;                                /*!< Feedback sequence-number */
};

/*!@brief Service-usage information.
 *
 * This struct is used as values for the service_usage hash.
*/
struct service_usage_item {
 struct lmodule **provider;  /*!< the modules that currently provide this service */
 struct lmodule **users;     /*!< the modules that currently use this service */
};

/*!@brief Service-usage information.
 *
 * This hash is used to figure out what services are currently being provided, and which services depend
 * on which services.
*/
struct uhash *service_usage;

/*!@brief Scan for modules
 *
 * This will scan the configuration file to find the path where module .so files can be found,
 * then load them.
*/
int mod_scanmodules ();

/*!@brief Clean up
 *
 * Free the chain of loaded modules and unload the .so-files
*/
int mod_freemodules ();

/*!@brief Register module
 * @todo Enhance this function so that one may specify reset() and reload() functions.
 *
 * This functions adds a module to the main chain of modules, so that it can be used in depency
 * calculations.
*/
struct lmodule *mod_add (void *, int (*)(void *, struct einit_event *), int (*)(void *, struct einit_event *), void *, struct smodule *);

/*!@brief Find a module
 * @deprecated This should not be used anymore. Functions that need to do this should know enough about
 *             the internal workings of the main module loader to know how to find modules themselves.
 *
 * This was originally intended to aid in finding specific modules.
*/
struct lmodule *mod_find (char *rid, unsigned int options);

/*!@brief Change module's state
 * @param[in]     task   What state the module should be put in.
 * @param[in,out] module The module that is to be manipulated.
 *
 * Use this to change the state of a module, i.e. enable it, disable it, reset it or reload it.
*/
int mod (unsigned int task, struct lmodule *module);

/*!@brief Query service-usage information.
 *
 * This function can be used to query/update certain service-usage information where the result can be
 * expressed as an integer.
*/
uint16_t service_usage_query (uint16_t, struct lmodule *, char *);

/*!@brief Query service-usage information.
 *
 * This function can be used to query certain service-usage information where the result can be expressed
 * as a set of strings.
*/
char **service_usage_query_cr (uint16_t, struct lmodule *, char *);

/*!@brief Query service-usage information.
 *
 * This function can be used to query/update certain service-group information where the result can be
 * expressed as an integer.
*/
uint16_t service_usage_query_group (uint16_t, struct lmodule *, char *);

/*!@brief The module loader's event-handler.
 *
 * This event-handler answers some on-line status-queries.
*/
void mod_event_handler(struct einit_event *);

/*!@brief Update status information.
 *
 * This macro should be used to provide any feedback-modules with updated status information.
*/
#define status_update(a) \
 event_emit(a, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE); \
 if (a->task & MOD_FEEDBACK_SHOW) a->task ^= MOD_FEEDBACK_SHOW; a->string = NULL; a->integer++

#endif
