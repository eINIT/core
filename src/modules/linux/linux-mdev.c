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
#include <sys/stat.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#include <einit/exec.h>

#include <sys/mount.h>

#include <asm/types.h>
#include <sys/socket.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <fcntl.h>

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
 .mode      = einit_module,
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

#define NETLINK_BUFFER 1024*1024*64

void linux_mdev_post_load_kernel_extensions (struct einit_exec_data *xd) {
 mount ("usbfs", "/proc/bus/usb", "usbfs", 0, NULL);

 struct einit_event eml = evstaticinit(einit_boot_devices_available);
 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

void linux_mdev_post_execute (struct einit_exec_data *xd) {
 chmod ("/dev/null", 0777);
 chmod ("/dev/zero", 0666);
 chmod ("/dev/console", 0660);
 chmod ("/dev/ptmx", 0777);
 chmod ("/dev/random", 0777);
 chmod ("/dev/urandom", 0777);

 pid_t p = einit_fork (linux_mdev_post_load_kernel_extensions, NULL, thismodule->module->rid, thismodule);

 if (p == 0) {
  struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
  event_emit (&eml, 0);
  evstaticdestroy(eml);

  _exit (EXIT_SUCCESS);
 }
}

void linux_mdev_run() {
  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);
  mount ("mdev", "/dev", "tmpfs", 0, NULL);

  struct einit_event eml = evstaticinit(einit_boot_dev_writable);
  event_emit (&eml, 0);
  evstaticdestroy(eml);

  mkdir ("/dev/pts", 0777);
  mount ("devpts", "/dev/pts", "devpts", 0, NULL);

  mkdir ("/dev/shm", 0777);
  mount ("shm", "/dev/shm", "tmpfs", 0, NULL);

  symlink ("/proc/self/fd", "/dev/fd");
  symlink ("fd/0", "/dev/stdin");
  symlink ("fd/1", "/dev/stdout");
  symlink ("fd/2", "/dev/stderr");

  FILE *he = fopen ("/proc/sys/kernel/hotplug", "w");
  if (he) {
   char *hotplug_handler = cfg_getstring ("configuration-system-hotplug-handler", NULL);

   if (hotplug_handler) {
    fputs (hotplug_handler, he);
   } else {
    fputs ("", he);
   }

   fputs ("\n", he);
   fclose (he);
  }

  char *xtx[] = { "mdev", "-s", NULL };
  einit_exec_without_shell_with_function_on_process_death_options (xtx, linux_mdev_post_execute, thismodule, einit_exec_keep_stdin | einit_exec_daemonise);
}

int linux_mdev_configure (struct lmodule *pa) {
 module_init (pa);

 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 if (!dm || strcmp (dm, "mdev")) {
  return status_configure_failed | status_not_in_use;
 }

 event_listen (einit_boot_early, linux_mdev_run);

 return 0;
}
