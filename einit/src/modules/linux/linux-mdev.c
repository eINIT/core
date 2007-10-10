/*
 *  linux-mdev.c
 *  einit
 *
 *  Created on 10/10/2007.
 *  Copyright 2007 Ryan Hope, Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Ryan Hope, Magnus Deininger
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

int linux_mdev_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_mdev_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Device Setup (Linux, MDEV)",
 .rid       = "linux-mdev",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_mdev_configure
};

module_register(linux_mdev_self);

#endif

char linux_mdev_enabled = 0;

void linux_mdev_load_kernel_extensions() {
 struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
 evstaticdestroy(eml);
}

int linux_mdev_run() {
 char *dm;

 if (!linux_mdev_enabled && (dm = cfg_getstring("configuration-system-device-manager", NULL)) && strmatch (dm, "mdev")) {
  linux_mdev_enabled = 1;

  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);

  system (EINIT_LIB_BASE "/modules-xml/mdev.sh enable");

  linux_mdev_load_kernel_extensions();

  return status_ok;
 } else
  return status_failed;
}

int linux_mdev_shutdown() {
 return status_ok;
}

void linux_mdev_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  linux_mdev_shutdown();
 }
}

void linux_mdev_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   if (linux_mdev_run() == status_ok) {
    struct einit_event eml = evstaticinit(einit_boot_devices_available);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }

   /* some code */
   break;

  default: break;
 }
}

int linux_mdev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_mdev_power_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_mdev_boot_event_handler);

 return 0;
}

int linux_mdev_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_mdev_cleanup;

 event_listen (einit_event_subsystem_boot, linux_mdev_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_mdev_power_event_handler);

 return 0;
}
