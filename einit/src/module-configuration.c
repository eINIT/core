/*
 *  module-configuration.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 22/10/2006.
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

#include <einit-modules/configuration.h>
#include <einit/event.h>

#define EVENT_FUNCTIONS_PTR NULL

/* event handler for the expat-based XML parser */
#if ( EINIT_MODULES_XML_EXPAT == 'y' )
void einit_config_xml_expat_event_handler (struct einit_event *);

struct event_function einit_config_xml_expat_event_handler_ef = {
 .type = EVENT_SUBSYSTEM_EINIT,
 .handler = einit_config_xml_expat_event_handler,
 .next = EVENT_FUNCTIONS_PTR
};

#undef EVENT_FUNCTIONS_PTR
#define EVENT_FUNCTIONS_PTR &einit_config_xml_expat_event_handler_ef

#endif


/* event handler for the default scheduler */
void sched_event_handler(struct einit_event *);

struct event_function einit_sched_event_handler_handler_ef = {
 .type = EVENT_SUBSYSTEM_IPC,
 .handler = sched_event_handler,
 .next = EVENT_FUNCTIONS_PTR
};

#undef EVENT_FUNCTIONS_PTR
#define EVENT_FUNCTIONS_PTR &einit_sched_event_handler_handler_ef

/* event handlers for the default module loader */
void mod_event_handler(struct einit_event *);
void module_loader_einit_event_handler (struct einit_event *);

struct event_function einit_mod_event_handler_ef = {
 .type = EVENT_SUBSYSTEM_IPC,
 .handler = mod_event_handler,
 .next = EVENT_FUNCTIONS_PTR
};

struct event_function module_loader_einit_event_handler_ef = {
 .type = EVENT_SUBSYSTEM_EINIT,
 .handler = module_loader_einit_event_handler,
 .next = &einit_mod_event_handler_ef
};

#undef EVENT_FUNCTIONS_PTR
#define EVENT_FUNCTIONS_PTR &module_loader_einit_event_handler_ef

/* ipc-handler for the default event system manager */
void event_ipc_handler(struct einit_event *);

struct event_function einit_event_ipc_handler_ef = {
 .type = EVENT_SUBSYSTEM_IPC,
 .handler = event_ipc_handler,
 .next = EVENT_FUNCTIONS_PTR
};

#undef EVENT_FUNCTIONS_PTR
#define EVENT_FUNCTIONS_PTR &einit_event_ipc_handler_ef

struct event_function *event_functions = EVENT_FUNCTIONS_PTR;
