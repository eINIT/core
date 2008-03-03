/*
 *  linux-splash-invoker.c
 *  einit
 *
 *  Created on 28/02/2008.
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

int linux_splash_invoker_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

struct smodule linux_splash_invoker_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Linux Boot Splash Invoker",
 .rid       = "linux-splash-invoker",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_splash_invoker_configure
};

module_register(linux_splash_invoker_self);

#endif

char linux_splash_invoker_psplash_mode = 0;
char linux_splash_invoker_usplash_mode = 0;
char linux_splash_invoker_exquisite_mode = 0;

void linux_splash_invoker_psplash_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-psplash");
 eml.task = einit_module_enable;

 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
 evstaticdestroy(eml);
}

void linux_splash_invoker_usplash_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-usplash");
 eml.task = einit_module_enable;

 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
 evstaticdestroy(eml);
}

void linux_splash_invoker_exquisite_boot_devices_ok () {
 struct einit_event eml = evstaticinit(einit_core_manipulate_services);
 eml.stringset = set_str_add (NULL, "einit-exquisite");
 eml.task = einit_module_enable;

 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
 evstaticdestroy(eml);
}

int linux_splash_invoker_cleanup (struct lmodule *pa) {
 if (linux_splash_invoker_psplash_mode) {
  event_ignore (einit_boot_load_kernel_extensions, linux_splash_invoker_psplash_boot_devices_ok);
 }
 if (linux_splash_invoker_usplash_mode) {
  event_ignore (einit_boot_load_kernel_extensions, linux_splash_invoker_usplash_boot_devices_ok);
 }
 if (linux_splash_invoker_exquisite_mode) {
  event_ignore (einit_boot_root_device_ok, linux_splash_invoker_exquisite_boot_devices_ok);
 }

 return 0;
}

int linux_splash_invoker_configure (struct lmodule *pa) {
 module_init (pa);
 pa->cleanup = linux_splash_invoker_cleanup;

 if (einit_initial_environment) {
  int i = 0;
  for (; einit_initial_environment[i]; i++) {
   if (strprefix (einit_initial_environment[i], "splash=") && (einit_initial_environment[i] + 7)) {
    if (strmatch ((einit_initial_environment[i] + 7), "psplash")) {
     linux_splash_invoker_psplash_mode = 1;
    } else if (strmatch ((einit_initial_environment[i] + 7), "usplash")) {
     linux_splash_invoker_usplash_mode = 1;
    } else if (strmatch ((einit_initial_environment[i] + 7), "exquisite")) {
     linux_splash_invoker_usplash_mode = 1;
    }
   }
  }
 }

 if (linux_splash_invoker_psplash_mode) {
  event_listen (einit_boot_load_kernel_extensions, linux_splash_invoker_psplash_boot_devices_ok);
 } else if (linux_splash_invoker_usplash_mode) {
  event_listen (einit_boot_load_kernel_extensions, linux_splash_invoker_usplash_boot_devices_ok);
 } else if (linux_splash_invoker_exquisite_mode) {
  event_listen (einit_boot_root_device_ok, linux_splash_invoker_exquisite_boot_devices_ok);
 }

 return 0;
}
