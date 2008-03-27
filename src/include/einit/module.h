/*
 *  module.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

#ifdef __cplusplus
extern "C" {
#endif

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

#ifndef EINIT_MODULE_H
#define EINIT_MODULE_H

#define MAXMODULES 40

#include <pthread.h>
#include <sys/types.h>
#include <stdint.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>

/*!\ingroup modulemdefinition
 * \{ */
enum einit_module_options {
 einit_module               = 0x0000,
/*!< Module-type: Nothing special, move along... */
 einit_module_deprecated    = 0x0010,
/*!< Module-option: Deprecated module: only try if nothing else worked */

 einit_module_event_actions = 0x0100,
/*!< Module-type: Special module that doesn't have any regular enable/disable/whatever actions, but is
     instead relying on events for this purpose. */
 einit_module_fork_actions  = 0x0200,
/*!< Module-type: Special module that does have regular enable/disable/whatever actions, but they need
     to be executed in their own process since they need to block or similar... */

 einit_feedback_job         = 0x1000
/*!< Feedback-option: Say "OK" when this module is enabled, don't say anything when disabled */
};
/*!\} */

/*!\ingroup modulemanipulation
 * \{ */
enum einit_module_task {
 einit_module_enable              = 0x0001,
/*!< Command for mod(): Enable specified module. */
 einit_module_disable             = 0x0002,
/*!< Command for mod(): Disable specified module. */
 einit_module_custom              = 0x0004,

 einit_module_ignore_dependencies = 0x0800,
/*!< Option: Ignore dependencies on module status change with mod() */
};
/*!\} */

enum mod_add_options {
 substitue_and_prune              = 0x0001
};

/*!\ingroup statusinformation
 * \{ */
enum einit_module_status {
 status_idle      = 0x0000,
/*!< Status Information: Object is currently idle. */
 status_ok        = 0x0001,
/*!< Status Information: Last command executed OK. */
 status_aborted   = 0x0002,
/*!< Status Information: Last command was aborted. */
 status_failed    = 0x0004,
/*!< Status Information: Last command failed. */
 status_failed_requirement = 0x0014,
/*!< Status Information: Last command cannot be executed, because requirements are not met. */
 status_done      = 0x8000,
/*!< Status Information: Last command is not executing anymore. */
 status_working   = 0x4000,
/*!< Status Information: Someone is working on this object just now. */
 status_command_not_implemented = 0x0020,
/*!< Status Information: command not implemented*/
 status_enabled   = 0x0100,
/*!< Status Information: Object is enabled. */
 status_disabled  = 0x0200,
/*!< Status Information: Object is disabled. */

 status_deferred  = 0x1000,
/*!< Status Information: Hint: Object is scheduled, but deferred. */

 status_configure_failed = 0x100000,
/*!< Status Information: Configure: Module configuration has failed */
 status_not_ready        = 0x010000,
/*!< Status Information: Configure: Module would be used but can't work yet */
 status_not_in_use       = 0x020000,
/*!< Status Information: Configure: Module won't be used */
 status_block            = 0x040000,
/*!< Status Information: Configure: Module doesn't want to be loaded ever again */
 status_configure_done   = 0x200000,
/*!< Status Information: Configure: Module has already done all it needs to do */

 status_death_pending    = 0x400000
};
/*!\} */

struct service_information {
 char **provides;       /*!< A list of services that this module provides. */
 char **requires;       /*!< A list of services that this module requires. */
 char **after;          /*!< Load this module after these services, but only if they're loaded to begin with */
 char **before;         /*!< Load this module before these services, but only if they're loaded to begin with */

#if 0
 char **notwith;        /*!< A list of services that may not be loaded together with this module; ignored. */
#endif
};

struct lmodule;

/*!\brief Static (on-file) module definition
 * \ingroup moduledefinition
 *
 * The static module definition that is kept in the module's .so-file.
*/
struct smodule {
 uint32_t eiversion;    /*!< The API version of eINIT that the module was compiled with. */
 uint32_t eibuild;      /*!< The build number of eINIT that the module was compiled with. */
 uint32_t version;      /*!< The module's version; this is currently ignored by eINIT. */
 enum einit_module_options mode;
                        /*!< The module type; should be einit_module_generic for most modules. */
 char *name;            /*!< The real name of the module. */
 char *rid;             /*!< The short ID of the module. */
 struct service_information si;

 int (*configure)(struct lmodule *);
                        /*!< function used to initialise the module. */
};

/*!\brief In-memory module definition
 * \ingroup moduledefinition
 *
 * The dynamic module definition that is kept in memory after the module is loaded.
*/
struct lmodule {
 char *source;                                  /*!< module source (filename, etc..) */
 void *sohandle;                                /*!< .so file-handle */
 int (*enable)  (void *, struct einit_event *); /*!< Pointer to the module's enable()-function */
 int (*disable) (void *, struct einit_event *); /*!< Pointer to the module's disable()-function */
 int (*custom) (void *, char *, struct einit_event *); /*!< Pointer to the module's custom()-function */

 enum einit_module_status status;               /*!< Current module status (enabled, disabled, ...) */
 void *param;                                   /*!< Parameter for state-changing functions */
 pthread_mutex_t mutex;	                        /*!< Module-mutex; is used by the mod()-function */
 const struct smodule *module;                  /*!< Pointer to the static module definition */
 struct lmodule *next;                          /*!< Pointer to the next module in the list */
 struct service_information *si;

 char **functions;                              /*!< field for custom functions */

 pid_t pid;
 char *pidfile;
};

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

struct lmodule *mod_update (struct lmodule *);

/*!\brief Register module
 * \ingroup moduledefinition
 * \param[in] sohandle Handle for the module's .so file
 * \param[in] module   Pointer to the module's static (on-file) module definition
 *
 * This functions adds a module to the main chain of modules, so that it can be used in dependency
 * calculations.
*/
struct lmodule *mod_add (void *sohandle, const struct smodule *module);

struct lmodule *mod_add_or_update (void *sohandle, const struct smodule *module, enum mod_add_options options);

/*!\brief Change module's state
 * \param[in]     task   What state the module should be put in.
 * \param[in,out] module The module that is to be manipulated.
 * \ingroup modulemanipulation
 *
 * Use this to change the state of a module, i.e. enable it, disable it, etc...
*/
int mod (enum einit_module_task task, struct lmodule *module, char *custom_command);

int mod_complete (char *rid, enum einit_module_task task, enum einit_module_status status);

struct lmodule *mod_lookup_rid (const char *rid);
struct lmodule *mod_lookup_source (const char *source);
int mod_update_sources (char **source);
int mod_update_source (const char *source);
void mod_update_pids ();
char *mod_lookup_pid (pid_t pid);

/*!\ingroup serviceusagequeries
 * \{ */
void mod_update_usage_table (struct lmodule *module);
char mod_service_is_in_use (char *service);
char mod_service_is_provided (char *service);
char mod_service_requirements_met (struct lmodule *module);
char mod_service_not_in_use (struct lmodule *module);
char **mod_list_all_provided_services ();
struct lmodule **mod_list_all_enabled_modules ();
struct lmodule **mod_get_all_users (struct lmodule *module);
struct lmodule **mod_get_all_users_of_service (char *service);
struct lmodule **mod_get_all_providers (char *service);

/*!\} */

void einit_add_fd_prepare_function (int (*fdprep)(fd_set *));
void einit_add_fd_handler_function (void (*fdhandle)(fd_set *));

#define fbprintf(statusvar, ...) if (statusvar) {\
 char _fbprintf_buffer[BUFFERSIZE];\
 snprintf (_fbprintf_buffer, BUFFERSIZE, __VA_ARGS__);\
 statusvar->string = _fbprintf_buffer;\
 event_emit(statusvar, einit_event_flag_broadcast); \
 statusvar->string = NULL; }

extern const struct smodule **coremodules[MAXMODULES];

#if defined(EINIT_MODULE)

struct lmodule *thismodule;

#ifndef __cplusplus
const struct smodule *self;
#endif

#define module_register(smod) const struct smodule *self = &smod
#define module_init(lmod) thismodule = lmod;

#endif

#if defined(EINIT_CORE) && defined(thismodule) && defined(self)

#define EINIT_MODULE_HEADER

struct lmodule * thismodule;
const struct smodule * self;

#define module_register(smod) const struct smodule * self = &smod;
#define module_init(lmod) thismodule = lmod;
#endif

#endif

#ifdef __cplusplus
}
#endif
