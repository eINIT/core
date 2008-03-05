/*
 *  einit.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 24/07/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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

/*!\brief Switch to a specific Mode
 *
 * Tell eINIT to switch to <mode>. It's like calling:
 * 'einit-control rc switch-mode <mode>'.
 *
 * \param[in] mode The mode to switch to.
 */
void einit_switch_mode (const char *mode);

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
int einit_read_callback_limited (char **path, int (*callback)(char *, size_t, void *), void *, int);

/*!\brief Write to a file
 *
 * This will write the provided data to the provided path.
 *
 * \param[in] path The path to write to
 * \param[in] data The data to write
 */
int einit_write (char **path, const char *data);

/*!\brief Manipulate a Service
 *
 * This will call the specified 'action' on the specified 'service'
 *
 * \param[in] service the service to manipulate
 * \param[in] action the action to call
 */
void einit_service_call (const char *service, const char *action);

/*!\brief Manipulate a Module
 *
 * This will call the specified 'action' on the specified 'module'
 *
 * \param[in] rid the module to manipulate (in 'rid' form)
 * \param[in] action the action to call
 */
void einit_module_call (const char *rid, const char *action);

const char *einit_event_encode (struct einit_event *ev);
int einit_event_loop_decoder (char *fragment, size_t size, void *data);

/*!\brief Grab and Handle Events
 *
 * This will run an event loop that will grab events from eINIT and emit them locally.
 */
void einit_event_loop ();

/*!\brief Grab and Handle old Events
 *
 * This will run an event loop that will grab events from eINIT and emit them locally,
 * but unlike einit_event_loop(), it will stop and return once all the current events
 * have been processed, instead of waiting for new events to come in.
 */
void einit_replay_events ();

char * einit_module_get_attribute (const char *rid, const char *attribute);

char * einit_module_get_name (const char *rid);

char ** einit_module_get_provides (const char *rid);

char ** einit_module_get_requires (const char *rid);

char ** einit_module_get_after (const char *rid);

char ** einit_module_get_before (const char *rid);

char ** einit_module_get_status (const char *rid);

char ** einit_module_get_options (const char *rid);

#ifdef __cplusplus
}
#endif

#endif
