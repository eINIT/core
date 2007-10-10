/*
 *  linux-udev.c
 *  einit
 *
 *  Created on 18/09/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#include <einit-modules/exec.h>

#include <sys/mount.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_udev_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_udev_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Device Setup (Linux, UDEV)",
 .rid       = "linux-udev",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_udev_configure
};

module_register(linux_udev_self);

#endif

char linux_udev_enabled = 0;

void linux_udev_load_kernel_extensions() {
 struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
 evstaticdestroy(eml);
}

int linux_udev_run() {
 char *dm;

 if (!linux_udev_enabled && (dm = cfg_getstring("configuration-system-device-manager", NULL)) && strmatch (dm, "udev")) {
  linux_udev_enabled = 1;

  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);

  system (EINIT_LIB_BASE "/modules-xml/udev.sh enable");

  linux_udev_load_kernel_extensions();
 }

 return status_ok;
}

int linux_udev_shutdown() {
 if (linux_udev_enabled) {
  system (EINIT_LIB_BASE "/modules-xml/udev.sh on-shutdown");
  system (EINIT_LIB_BASE "/modules-xml/udev.sh disable");

  linux_udev_enabled = 0;
 }

 return status_ok;
}

void linux_udev_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  linux_udev_shutdown();
 }
}

void linux_udev_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   if (linux_udev_run() == status_ok) {
    struct einit_event eml = evstaticinit(einit_boot_devices_available);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }

   /* some code */
   break;

  default: break;
 }
}

int linux_udev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_udev_power_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_udev_boot_event_handler);

 return 0;
}

int linux_udev_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_udev_cleanup;

 event_listen (einit_event_subsystem_boot, linux_udev_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_udev_power_event_handler);

 return 0;
}
