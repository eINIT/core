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

/*!\file einit/module.h
 * \brief Module-functions and structs.
 * \author Magnus Deininger
 *
 * This header file declares all structs and functions that deal with modules.
*/

/*!\defgroup serviceusagequeries eINIT Internals: Service-Usage-Queries
 * \defgroup modulemanipulation Writing Modules: Manipulating Modules
 * \defgroup moduledefinition Writing Modules: Defining Modules
 * \defgroup statusinformation Writing Modules: Status-Information Macros and Constants
*/

/*!\dir src/modules
 * \brief Default Modules
 *
 * All the default modules are in this directory. Modules in this directory should build and work on
 * most posix-compliant systems.
*/

/*!\dir src/modules/linux
 * \brief Default Linux Modules
 *
 * All the default linux-specific modules are in this directory.
*/

/*!\dir src/modules/bsd
 * \brief Default BSD Modules
 *
 * All the default bsd-specific modules are in this directory.
*/

/*!\dir src/modules/efl
 * \brief Default EFL Modules
 *
 * Modules that depend on EFL-Libraries are in this directory. Specifically this will contain the evas-/edje-
 * based visualiser once i get to it.
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

/*!\ingroup modulemdefinition
 * \{ */
#define EINIT_MOD_LOADER 1    /*!< Module-type: Used for module-loaders, i.e. those with scanmodules()-functions. */
#define EINIT_MOD_FEEDBACK 2  /*!< Module-type: Feedback modules, i.e. those that tell users what's going down. */
#define EINIT_MOD_EXEC 4      /*!< Module-type: Regular modules, i.e. those that provide services. */
/*!\} */

/*!\ingroup modulemanipulation
 * \{ */
#define MOD_ENABLE 0x0001        /*!< Command for mod(): Enable specified module. */
#define MOD_DISABLE 0x0002       /*!< Command for mod(): Disable specified module. */
#define MOD_RELOAD 0x0004        /*!< Command for mod(): Reload specified module. */
#define MOD_RESET 0x0008         /*!< Command for mod(): Reset specified module. */
#define MOD_FEEDBACK_SHOW 0x0100 /*!< Option set by mod(): Show feedback. */
/*!\} */

#define MOD_SCHEDULER 0x1000                    /*!< Bitmask for scheduler-feedback-options. */
#define MOD_SCHEDULER_PLAN_COMMIT_START 0x1001  /*!< Scheduler-feedback-option: "New plan is now being executed.". */
#define MOD_SCHEDULER_PLAN_COMMIT_FINISH 0x1002 /*!< Scheduler-feedback-option: "New plan is now done executing.". */

#define MOD_LOCKED 0x8000        /*!< Module-option: Module is locked. */

/*!\ingroup statusinformation
 * \{ */
#define STATUS_IDLE 0x0000      /*!< Status Information: Object is currently idle. */
#define STATUS_OK 0x8003        /*!< Status Information: Last command executed OK. */
#define STATUS_ABORTED 0x8002   /*!< Status Information: Last command was aborted. */
#define STATUS_FAIL 0x0100      /*!< Status Information: Last command failed. */
#define STATUS_FAIL_REQ 0x0300  /*!< Status Information: Last command cannot be executed, because requirements are not met. */
#define STATUS_DONE 0x8000      /*!< Status Information: Bitmask: Last command is not executing anymore. */
#define STATUS_WORKING 0x0010   /*!< Status Information: Bitmask: Someone is working on this object just now. */
#define STATUS_ENABLING 0x0011  /*!< Status Information: Object is currently being enabled. */
#define STATUS_DISABLING 0x0012 /*!< Status Information: Object is currently being disabled. */
#define STATUS_RELOADING 0x0013 /*!< Status Information: Object is currently being reloaded. */
#define STATUS_ENABLED 0x0401   /*!< Status Information: Object is enabled. */
#define STATUS_DISABLED 0x0802  /*!< Status Information: Object is disabled. */
/*!\} */

/*!\ingroup serviceusagequeries
 * \{*/
#define SERVICE_NOT_IN_USE              0x0001 /*!< Service-usage-query: "Is this module not in use?" */
#define SERVICE_REQUIREMENTS_MET        0x0002 /*!< Service-usage-query: "Are this module's requirements met?" */
#define SERVICE_IS_REQUIRED             0x0004 /*!< Service-usage-query: "Is this currently required?" */
#define SERVICE_IS_PROVIDED             0x0008 /*!< Service-usage-query: "Is this currently provided?" */
#define SERVICE_UPDATE                  0x0100 /*!< Service-usage-query: "Update service information." */

#define SERVICE_GET_ALL_PROVIDED        0x0010 /*!< Service-usage-query: "What services are currently provided?". */
#define SERVICE_GET_SERVICES_THAT_USE   0x0020 /*!< Service-usage-query: "What services use this?" */
#define SERVICE_GET_SERVICES_USED_BY    0x0040 /*!< Service-usage-query: "What services are used by this?" */

#define SERVICE_ADD_GROUP_PROVIDER      0x0200 /*!< Service-usage-query: "This module provides this service" */
/*!\} */

/*!\brief Static (on-file) module definition
 * \ingroup moduledefinition
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

/*!\brief In-memory module definition
 * \ingroup moduledefinition
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

/*!\brief Service-usage information.
 * \ingroup serviceusagequeries
 *
 * This struct is used as values for the service_usage hash.
*/
struct service_usage_item {
 struct lmodule **provider;  /*!< the modules that currently provide this service */
 struct lmodule **users;     /*!< the modules that currently use this service */
};

/*!\brief Service-usage information.
 * \ingroup serviceusagequeries
 *
 * This hash is used to figure out what services are currently being provided, and which services depend
 * on which services.
*/
struct uhash *service_usage;

/*!\brief Scan for modules
 *
 * This will scan the configuration file to find the path where module .so files can be found,
 * then load them.
*/
int mod_scanmodules ( void );

/*!\brief Clean up
 *
 * Free the chain of loaded modules and unload the .so-files
*/
int mod_freemodules ( void );

/*!\brief Register module
 * \ingroup moduledefinition
 * \todo Enhance this function so that one may specify reset() and reload() functions.
 * \param[in] sohandle Handle for the module's .so file
 * \param[in] enable   Pointer to the module's enable()-function
 * \param[in] disable  Pointer to the module's disable()-function
 * \param[in] param    A (void*)Pointer that will be passed to the module's mode-switching functions as the first parametre
 * \param[in] module   Pointer to the module's static (on-file) module definition
 *
 * This functions adds a module to the main chain of modules, so that it can be used in dependency
 * calculations.
*/
struct lmodule *mod_add (void *sohandle, int (*enable)(void *, struct einit_event *), int (*disable)(void *, struct einit_event *), void *param, struct smodule *module);

/*!\brief Change module's state
 * \param[in]     task   What state the module should be put in.
 * \param[in,out] module The module that is to be manipulated.
 * \ingroup modulemanipulation
 *
 * Use this to change the state of a module, i.e. enable it, disable it, reset it or reload it.
*/
int mod (unsigned int task, struct lmodule *module);

/*!\ingroup serviceusagequeries
 * \{ */
/*!\brief Query service-usage information.
 * \param[in] task    This tells the function what to do. Use the SERVICE_* defines.
 * \param[in] module  Depending on the command that you pass to this function, a module may be required.
 * \param[in] service Depending on the command that you pass to this function, a service-name may be required.
 *
 * This function can be used to query/update certain service-usage information where the result can be
 * expressed as an integer.
*/
uint16_t service_usage_query (uint16_t task, struct lmodule *module, char *service);

/*!\brief Query service-usage information.
 * \param[in] task    This tells the function what to do. Use the SERVICE_* defines.
 * \param[in] module  Depending on the command that you pass to this function, a module may be required.
 * \param[in] service Depending on the command that you pass to this function, a service-name may be required.
 *
 * This function can be used to query certain service-usage information where the result can be expressed
 * as a set of strings.
*/
char **service_usage_query_cr (uint16_t task, struct lmodule *module, char *service);

/*!\brief Query service-usage information.
 * \param[in] task    This tells the function what to do. Use the SERVICE_* defines.
 * \param[in] module  Depending on the command that you pass to this function, a module may be required.
 * \param[in] service Depending on the command that you pass to this function, a service-name may be required.
 *
 * This function can be used to query/update certain service-group information where the result can be
 * expressed as an integer.
*/
uint16_t service_usage_query_group (uint16_t task, struct lmodule *module, char *service);
/*! \} */

/*!\brief The module loader's event-handler.
 * \param[in] event Information that will be passed to this function by the event-system.
 *
 * This event-handler answers some on-line status-queries.
*/
void mod_event_handler(struct einit_event *event);

/*!\brief Update status information.
 * \ingroup statusinformation
 * \param[in] a The status information that is to be updated. Use your module-function's second parametre here.
 *
 * This macro should be used to provide any feedback-modules with updated status information.
*/
#define status_update(a) \
 event_emit(a, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE); \
 if (a->task & MOD_FEEDBACK_SHOW) a->task ^= MOD_FEEDBACK_SHOW; a->string = NULL; a->integer++

#endif
