/*
 *  dispatcher.c
 *  einit
 *
 *  Created on 28/02/2008.
 *  Renamed from linux-splash-invoker.c on 30/03/2008
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int dispatcher_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

struct smodule dispatcher_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "eINIT Daemon Dispatcher",
 .rid       = "dispatcher",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = dispatcher_configure
};

module_register(dispatcher_self);

#endif

char dispatcher_psplash = 0;
char dispatcher_usplash = 0;
char dispatcher_exquisite = 0;
char dispatcher_bootchart = 0;

void dispatcher_psplash_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-psplash");
 eml.task = einit_module_enable;

 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

void dispatcher_bootchart_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "bootchartd");
 eml.task = einit_module_enable;

 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

void dispatcher_bootchart_switch (struct einit_event *ev) {
 if (strmatch (ev->string, "default")) {
  struct einit_event eml = evstaticinit(einit_core_manipulate_services);
  eml.stringset = set_str_add (NULL, "bootchartd");
  eml.task = einit_module_disable;

  event_emit (&eml, 0);
  evstaticdestroy(eml);
 }
}

void dispatcher_usplash_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-usplash");
 eml.task = einit_module_enable;

 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

void dispatcher_exquisite_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-exquisite");
 eml.task = einit_module_enable;

 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

int dispatcher_configure (struct lmodule *pa) {
 module_init (pa);

 if (einit_initial_environment) {
  int i = 0;
  for (; einit_initial_environment[i]; i++) {
   if (strprefix (einit_initial_environment[i], "splash=") && (einit_initial_environment[i] + 7)) {
    if (strmatch ((einit_initial_environment[i] + 7), "psplash")) {
     dispatcher_psplash = 1;
    } else if (strmatch ((einit_initial_environment[i] + 7), "usplash")) {
     dispatcher_usplash = 1;
    } else if (strmatch ((einit_initial_environment[i] + 7), "exquisite")) {
     dispatcher_usplash = 1;
    }
   }
  }

  struct cfgnode *node;

  dispatcher_bootchart = ((node = cfg_getnode ("configuration-bootchart-active", NULL)) ? node->flag : 0);
 }

 if (dispatcher_psplash) {
  event_listen (einit_boot_dev_writable, dispatcher_psplash_boot_devices_ok);
 } else if (dispatcher_usplash) {
  event_listen (einit_boot_dev_writable, dispatcher_usplash_boot_devices_ok);
 } else if (dispatcher_exquisite) {
  event_listen (einit_boot_root_device_ok, dispatcher_exquisite_boot_devices_ok);
 }

 if (dispatcher_bootchart) {
  event_listen (einit_boot_dev_writable, dispatcher_bootchart_boot_devices_ok);
  event_listen (einit_core_mode_switch_done, dispatcher_bootchart_switch);
 }

 return 0;
}
