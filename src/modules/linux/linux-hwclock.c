/*
 *  linux-hwclock.c
 *  einit
 *
 *  Created on 01/10/2007.
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

int linux_hwclock_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_hwclock_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Device Setup (Linux, Hardware Clock)",
 .rid       = "linux-hwclock",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_hwclock_configure
};

module_register(linux_hwclock_self);

#endif

char linux_hwclock_enabled = 0;

void linux_hwclock_run() {
 if (!linux_hwclock_enabled) {
  linux_hwclock_enabled = 1;
  char *options = cfg_getstring ("configuration-services-hwclock/options", NULL);
  if (!options) options = "--utc";
  char tmp [BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "/sbin/hwclock --hctosys %s", options);

  qexec (tmp);
 }
}

void linux_hwclock_shutdown() {
 if (linux_hwclock_enabled) {
   char *options = cfg_getstring ("configuration-services-hwclock/options", NULL);
  if (!options) options = "--utc";
  char tmp [BUFFERSIZE];

  esprintf (tmp, BUFFERSIZE, "/sbin/hwclock --systohc %s", options);

  qexec (tmp);

  linux_hwclock_enabled = 0;
 }
}

int linux_hwclock_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 event_listen (einit_boot_devices_available, linux_hwclock_run);
 event_listen (einit_power_down_scheduled, linux_hwclock_shutdown);
 event_listen (einit_power_reset_scheduled, linux_hwclock_shutdown);

 return 0;
}
