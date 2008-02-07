/*
 *  einit.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 24/07/2007.
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

/*!\file einit/einit.h
 * \brief eINIT C Client Library
 * \author Magnus Deininger
 *
 * The Client Library for eINIT in C (libeinit)
 */

#include <einit/module.h>
#include <einit/tree.h>

#ifndef EINIT_EINIT_H
#define EINIT_EINIT_H

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Descriptor for an eINIT Module
  *
  * This is a descriptor that is retrieved from 
  * either einit_get_module_status(), or through einit_get_all_modules() (in the
  * latter case, it's the stree's value's type).
  */
struct einit_module {
 char *id;                        /*!< The unique ID of the module; Use this for handling the module internally. */
 char *name;                      /*!< The name of the module; Display this to the user. */
 enum einit_module_status status; /*!< This represents the status of the module (enabled, disabled, error conditions, ...). */

 char **provides;   /*!< The services this module provides. */
 char **requires;   /*!< The services (and module IDs) this module requires. */
 char **after;      /*!< Regular expressions to define after what other modules and services this one will be scheduled. */
 char **before;     /*!< Regular expressions to define before what other modules and services this one will be scheduled. */

 char **functions;  /*!< All the functions that this module can be called with. */
};

/*!\brief Descriptor for a Service's Status
 *
 * This enum holds all the possible status flags for a service.
 */
enum einit_service_status {
 service_idle     = 0x0000,  /*!< This is the default, "null mask" */
 service_provided = 0x0001   /*!< This bit means "the service is provided". */
};

/*!\brief Descriptor for a Service Group
 *
 * This is a descriptor that is (indirectly) retrieved from 
 * either einit_get_service_status(), or through einit_get_all_services() (in 
 * the latter case, it's part of the stree's value's type).
 */
struct einit_group {
 char **services; /*!< All the service and module IDs in this group */
 char *seq;       /*!< The group's type: should be 'all', 'most', 'any' or 'any-iop' */
};

/*!\brief Descriptor for a Service
 *
 * This is a descriptor that is retrieved from either 
 * einit_get_service_status(), or through einit_get_all_services() (in the 
 * latter case, it's the stree's value's type).
 */
struct einit_service {
 char *name;                       /*!< This is the name of the service. */
 enum einit_service_status status; /*!< This represents the status of the services (either idle or provided). */
 char **used_in_mode;              /*!< A list of modes that this service occurs in. */

 struct einit_group *group; /*!< If there's a service group with this name, this is a pointer to an appriate descriptor. */
 struct stree *modules;     /*!< If there's one or more modules that provide this service, this points to it/them. */
};

/*!\brief Descriptor for a Mode
 *
 * This is a descriptor that is retrieved from einit_get_all_modes() (in the 
 * latter case, it's the stree's value's type).
 */
struct einit_mode_summary {
 char *id;        /*!< The mode's unique ID. */
 char **base;     /*!< The mode's base-modes (if applicable). */
 char **services; /*!< The services to enable in this mode. */
 char **critical; /*!< Critical services in this mode. */
 char **disable;  /*!< The services to disable in this mode. */
};

/*!\brief Descriptor for an Event
 *
 * This is a summary for an event retrieved in one way or another through 
 * libeinit. Even in client programs, einit events are asynchronous. This means 
 * you need to einit_remote_event_listen() for them and 
 * einit_remote_event_ignore() them once you're done.
 */
struct einit_remote_event {
 enum einit_event_code type;       /*!< the event or subsystem to watch */

 char *string;                 /*!< a string */
 int32_t integer,              /*!< generic integer */
         status,               /*!< generic integer */
         task;                 /*!< generic integer */
 unsigned char flag;           /*!< flags */

 char **stringset;             /*!< a (string-)set that should make sense in combination with the event type */

 uint32_t seqid;   /*!< This is the event's unique sequence-ID. */
 time_t timestamp; /*!< Timestamp of the event as a unix-timestamp (seconds since the Epoch). Do note that this is the timestamp of when the event was received, not necessarily of when it was transmitted/emitted. */
};

/* functions */

/*!\brief Connect to eINIT
 *
 * Connect to eINIT, via whatever Method is deemed appropriate. Use this before 
 * using any of the einit*_ipc*() functions.
 * 
 * \param[in] argc idk what this is
 * \param[in] argv idk what this is either
 *
 * \param[out] char either zero for "that didn't work" or non-zero for "go ahead, 
 * you're good to go
 */
char einit_connect(int *argc, char **argv);

/*!\brief Spawn a private instance of eINIT
 *
 * Connect to eINIT by spawning a new eINIT core. Use this (or einit_connect()) 
 * before using any of the einit*_ipc*() functions.
 *
 * \param[in] argc idk what this is
 * \param[in] argv idk what this is either
 *
 * \param[out] char either zero for "that didn't work" or non-zero for "go ahead, 
 * you're good to go
 */
char einit_connect_spawn(int *argc, char **argv);

/*!\brief Disonnect from eINIT
 *
 * Disconnect from eINIT. Use this right before terminating your program. 
 * You shouldn't call anything after this.
 *
 * \param[out] char either zero for "that didn't work" or non-zero for "go ahead, 
 * you're good to go
 */
char einit_disconnect();

/*!\brief Tell the Core you want to receive Events
 *
 * Call this before registering any Events you wish to listen for. It's fairly 
 * important that you call this.
 */
void einit_receive_events();

/*!\brief Retrive all of eINIT's modules
 *
 * Retrieve an stree of all of eINIT's modules. The stree's keys are the 
 * module-RIDs, and the value is a struct einit_module*.
 */
struct stree *einit_get_all_modules ();

/*!\brief Retrive a specific module's data
 *
 * Retrieve Data for a specific module.
 *
 * \param[in] module The RID of the module you're interested in.
 */
struct einit_module *einit_get_module_status (char *module);

/*!\brief Retrive all of eINIT's services
 *
 * Retrieve an stree of all of eINIT's services. The stree's keys are the 
 * service-ids, and the value is a struct einit_service*.
 */
struct stree *einit_get_all_services ();

/*!\brief Retrive a specific module's data
 *
 * Retrieve Data for a specific service.
 *
 * \param[in] service The Service-ID of the service you need data on.
 */
struct einit_service *einit_get_service_status (char *service);

/*!\brief Free an einit_module*'s data
 *
 * Use this to trash data you received from einit_get_module_status(). Don't use
 * it on individual value's of the stree returned from einit_get_all_modules().
 *
 * \param[in] data A return value from einit_get_module_status() to free()
 */
void einit_module_free (struct einit_module *data);

/*!\brief Free an einit_service*'s data
 *
 * Use this to trash data you received from einit_get_service_status(). Don't 
 * use it on individual value's of the stree returned from 
 * einit_get_all_services().
 *
 * \param[in] data A return value from einit_get_service_status() to free()
 */
void einit_service_free (struct einit_service *data);

/*!\brief Free return value from einit_get_all_modules()
 *
 * Use this to trash data you received from einit_get_all_modules(). streefree()
 * alone won't do the job. You don't need to call streefree() afterwards, and 
 * you better not try to, either.
 * 
 * \param[in] modulestree A return value from einit_get_all_modules() to free()
 */
void modulestree_free(struct stree *modulestree);

/*!\brief Free return value from einit_get_all_services()
 *
 * Use this to trash data you received from einit_get_all_services(). 
 * streefree() alone won't do the job. You don't need to call streefree() 
 * afterwards, and you better not try to, either.
 *
 * \param[in] servicestree A return value from einit_get_all_services() to free()
 */
void servicestree_free(struct stree *servicestree);

/*!\brief Power Down the System
 *
 * Tell eINIT to initiate a system shutdown. You're likely to die soon after 
 * this, so better start cleaning up ASAP.
 */
void einit_power_down ();

/*!\brief Reset the System
 *
 * Tell eINIT to initiate a system reboot. You're likely to die soon after this,
 * so better start cleaning up ASAP.
 */
void einit_power_reset ();

/*!\brief Call a custom Action on a Service
 *
 * Tell eINIT to do <action> with <service>. It's like calling:
 * 'einit-control rc <service> <action>'.
 *
 * \param[in] service A service's ID to call
 * \param[in] action  What the service should do
 */
void einit_service_call (const char *service, const char *action);

/*!\brief Enable a Service
 *
 * Tell eINIT to do enable <service>. It's like calling:
 * 'einit-control rc <service> enable'.
 *
 * \param[in] service A service's ID to enable
 */
void einit_service_enable (const char *service);

/*!\brief Disable a Service
 *
 * Tell eINIT to do disable <service>. It's like calling:
 * 'einit-control rc <service> disable'.
 *
 * \param[in] service A service's ID to disable
 */
void einit_service_disable (const char *service);

/*!\brief Call a custom Action on a Module
 *
 * Tell eINIT to do <action> with <module>. It's like calling:
 * 'einit-control rc <module> <action>'.
 *
 * \param[in] module A module's ID to call
 * \param[in] action  What the service should do
 */
void einit_module_id_call (const char *, const char *);

/*!\brief Enable a Module
 *
 * Tell eINIT to do enable <module>. It's like calling:
 * 'einit-control rc <module> enable'.
 *
 * \param[in] module A module's ID to enable
 */
void einit_module_id_enable (const char *module);

/*!\brief Disable a Module
 *
 * Tell eINIT to do disable <module>. It's like calling:
 * 'einit-control rc <module> disable'.
 *
 * \param[in] module A module's ID to disable
 */
void einit_module_id_disable (const char *module);

/*!\brief Switch to a specific Mode
 *
 * Tell eINIT to switch to <mode>. It's like calling:
 * 'einit-control rc switch-mode <mode>'.
 *
 * \param[in] mode The mode to switch to.
 */
void einit_switch_mode (const char *mode);

/*!\brief Reload Configuration
 *
 * Tell eINIT to switch reload its configuration. It's like calling:
 * 'einit-control update configuration'.
 */
void einit_reload_configuration ();

/*!\brief Retrive all of eINIT's modes
 *
 * Retrieve an stree of all of eINIT's modes. The stree's keys are the 
 * service-ids, and the value is a struct einit_mode_summary*.
 */
struct stree *einit_get_all_modes();

/*!\brief Free return value from einit_get_all_modes()
 *
 * Use this to trash data you received from einit_get_all_modes(). streefree() 
 * alone won't do the job. You don't need to call streefree() afterwards, and 
 * you better not try to, either.
 *
 * \param[in] modestree A return value from einit_get_all_modes() to free()
 */
void modestree_free(struct stree *modestree);


/*!\brief Listen in on remote events
 *
 * Tell the core you wish to receive a specific <type> of events. If an 
 * appropriate event is received, your <callback> is called. Note that you can't
 * change any elements of an event using this callback, as opposed to the core, 
 * where you can do this. Changes are not propagated back to the core.
 *
 * \param[in] type     A subsystem-ID to listen in on.
 * \param[in] callback The function you wish called
 */
void einit_remote_event_listen (enum einit_event_subsystems type, 
								void (*callback)(struct einit_remote_event *));

/*!\brief Stop listening in on remote events
 *
 * Tell the core you you no longer wish to receive a specific <type> of events 
 * using <callback>.
 *
 * \param[in] type     A subsystem-ID that the callback is registered with.
 * \param[in] callback The function
 */
void einit_remote_event_ignore (enum einit_event_subsystems type, 
								void (*callback)(struct einit_remote_event *));

/*!\brief Emit an Event
 *
 * Submit an <event> to the core (and all other processes listening for events).
 * Do specify einit_event_flag_broadcast for <flags>! Also note that the <event>
 * attribute may be changed by the core/client lib in order to allow for 
 * 'return values' to an event, as is the case in the core.
 *
 * \param[in] event The event to submit to the core
 * \param[in] flags Flags for the event.
 */
void einit_remote_event_emit (struct einit_remote_event *event, 
							  enum einit_event_emit_flags flags);

/*!\brief Create an Event
 *
 * Create an event of type <type>. Creating it does not equal submitting it, it 
 * just creates the appropriate buffer to hold its data until you decide to 
 * einit_remote_event_emit() it.
 *
 * \param[in] type The type of event to create
 */
struct einit_remote_event *einit_remote_event_create (uint32_t type);

/*!\brief Destroy an Event
 *
 * Scrap <event>. This is ideally something returned from 
 * einit_remote_event_create(). Scrapping is not automatically done by calling 
 * einit_remote_event_emit(), and that's on purpose, so call this after you'red
 * one with it.
 *
 * \param[in] event The event to destroy.
 */
void einit_remote_event_destroy (struct einit_remote_event *event);

/*!\brief Get all nodes on some path
 *
 * This will return a list of all files and directories at some path. You need 
 * to free() the return value once you're done.
 *
 * \param[in] path The path to ls
 */
char **einit_ls (char **path);

/*!\brief Read a file
 *
 * This will read a file at some location and return a string with the file's 
 * contents.
 *
 * \param[in] path The path to read
 */
char *einit_read (char **path);

/*!\brief Read a file (with a callback)
 *
 * This will read a file at some location and call the provided callback on each
 * fragment that is read.
 *
 * \param[in] path The path to read
 * \param[in] callback A pointer to a callback functions
 */
int einit_read_callback (char **path, int (*callback)(char *, size_t, void *), void *);

/*!\brief Write to a file
 *
 * This will write the provided data to the provided path.
 *
 * \param[in] path The path to write to
 * \param[in] data The data to write
 */
int einit_write (char **path, char *data);

#ifdef __cplusplus
}
#endif

#endif
