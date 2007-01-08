/*
 *  configuration-secondary-sh-style.c
 *  einit
 *
 *  Created by Magnus Deininger on 01/08/2006.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <einit-modules/parse-sh.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

const struct smodule self = {
    .eiversion    = EINIT_VERSION,
    .version      = 1,
    .mode         = 0,
    .options      = 0,
    .name         = "Secondary Configuration Module: SH-Style Files",
    .rid          = "configuration-secondary-sh-style",
    .si           = {
        .provides = NULL,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

void einit_event_handler (struct einit_event *);
void ipc_event_handler (struct einit_event *);

/* functions that module tend to need */
int configure (struct lmodule *irr) {
 parse_sh_configure (irr);

 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *irr) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 parse_sh_cleanup (irr);
}

void sh_configuration_callback (char **data, uint8_t status) {

}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_UPDATE_CONFIGURATION) {
//  char *data = readfile ("/etc/profile.env");
  char *data = NULL;

  if (data) {
   parse_sh (data, sh_configuration_callback);

   free (data);
  }
 }
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d/path", NULL)) {
   fdputs ("NOTICE: CV \"configuration-compatibility-sysv-distribution-gentoo-init.d/path\":\n  Not found: Gentoo Init Scripts will not be processed. (not a problem)\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}
