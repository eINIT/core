/*
 *  einit-log.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/03/2007.
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

#define _MODULE

#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int _einit_log_configure (struct lmodule *);

#if defined(_EINIT_MODULE) || defined(_EINIT_MODULE_HEADER)

const struct smodule _einit_log_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = EINIT_MOD_FEEDBACK,
 .options   = 0,
 .name      = "eINIT Core Log Module",
 .rid       = "einit-log",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = _einit_log_configure
};

module_register(_einit_log_self);

#endif

void _einit_log_feedback_event_handler(struct einit_event *);
void _einit_log_ipc_event_handler(struct einit_event *);

void _einit_log_ipc_event_handler (struct einit_event *ev) {
}

int _einit_log_cleanup (struct lmodule *this) {
 event_ignore (EVENT_SUBSYSTEM_IPC, _einit_log_ipc_event_handler);
 event_ignore (EVENT_SUBSYSTEM_FEEDBACK, _einit_log_feedback_event_handler);

 return 0;
}

void _einit_log_feedback_event_handler(struct einit_event *ev) {
 if (ev->type == EVE_FEEDBACK_PLAN_STATUS) {
 } else if (ev->type == EVE_FEEDBACK_NOTICE) {
 }

 return;
}

int _einit_log_configure (struct lmodule *r) {
 module_init (r);

 r->cleanup = _einit_log_cleanup;

 event_listen (EVENT_SUBSYSTEM_IPC, _einit_log_ipc_event_handler);
 event_listen (EVENT_SUBSYSTEM_FEEDBACK, _einit_log_feedback_event_handler);

 return 0;
}
