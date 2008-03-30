/*
 *  linux-udev.c
 *  einit
 *
 *  Created on 18/09/2007.
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

#include <einit/exec.h>

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
 .mode      = einit_module,
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

void linux_udev_post_secondary_vgchange (struct einit_exec_data *xd) {
 struct einit_event eml = evstaticinit(einit_boot_devices_available);
 event_emit (&eml, 0);
 evstaticdestroy(eml);
}

void linux_udev_post_secondary_udevsettle (struct einit_exec_data *xd) {
 struct stat st;

 if (!stat ("/sbin/vgchange", &st)) {
  char *xtx[] = { "/sbin/vgchange", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_secondary_vgchange, thismodule);
 } else {
  linux_udev_post_secondary_vgchange (xd);
 }
}

void linux_udev_post_evmsactivate (struct einit_exec_data *xd) {
 struct stat st;

 if (stat ("/sbin/udevadm", &st)) {
  char *xtx[] = { "/sbin/udevsettle", "--timeout=60", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_secondary_udevsettle, thismodule);
 } else {
  char *xtx[] = { "/sbin/udevadm", "settle", "--timeout=60", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_secondary_udevsettle, thismodule);
 }
}

void linux_udev_post_vgscan (struct einit_exec_data *xd) {
 struct stat st;

 if (!stat ("/sbin/evms_activate", &st)) {
  char *xtx[] = { "/sbin/evms_activate", "-q", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_evmsactivate, thismodule);
 } else {
  linux_udev_post_evmsactivate(xd);
 }
}

void linux_udev_post_udevsettle (struct einit_exec_data *xd) {
 struct stat st;
 mount ("usbfs", "/proc/bus/usb", "usbfs", 0, NULL);

 /* let's not forget about raid setups and the like here... */
 if (!stat ("/sbin/lvm", &st)) {
  char *xtx[] = { "/sbin/lvm", "vgscan", "-P", "--mkdnodes", "--ignorelockingfailure", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_vgscan, thismodule);
 } else {
  linux_udev_post_vgscan(xd);
 }
}

void linux_udev_post_load_kernel_extensions (struct einit_exec_data *xd) {
 struct stat st;

 fputs ("waiting for udev to process all events...\n", stderr);

 if (stat ("/sbin/udevadm", &st)) {
  char *xtx[] = { "/sbin/udevsettle", "--timeout=60", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevsettle, thismodule);
 } else {
  char *xtx[] = { "/sbin/udevadm", "settle", "--timeout=60", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevsettle, thismodule);
 }
}

void linux_udev_post_udevtrigger (struct einit_exec_data *xd) {
 fputs ("loading kernel extensions...\n", stderr);

 pid_t p = einit_fork (linux_udev_post_load_kernel_extensions, NULL, thismodule->module->rid, thismodule);

 if (p == 0) {
  struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
  event_emit (&eml, 0);
  evstaticdestroy(eml);

  _exit (EXIT_SUCCESS);
 }
}

void linux_udev_post_execute (struct einit_exec_data *xd) {
 struct stat st;
 struct cfgnode *n = cfg_getnode ("configuration-system-coldplug", NULL);
 /* again, i should check for an appropriate kernel version... */
 /* this should be all nodes that'll be needed... */

 fputs ("populating /dev with udevtrigger...\n", stderr);

 if (stat ("/sbin/udevadm", &st)) {
  if (n && n->flag) {
   char *xtx[] = { "/sbin/udevtrigger", NULL };
   einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevtrigger, thismodule);
  } else {
   char *xtx[] = { "/sbin/udevtrigger", "--attr-match=dev", NULL };
   einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevtrigger, thismodule);
  }
 } else {
  if (n && n->flag) {
   char *xtx[] = { "/sbin/udevadm", "trigger", NULL };
   einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevtrigger, thismodule);
  } else {
   char *xtx[] = { "/sbin/udevadm", "trigger", "--attr-match=dev", NULL };
   einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_post_udevtrigger, thismodule);
  }
 }
}

/* TODO: maybe... maybe not, anyway...
   * using tarballs
   * copy devices from /lib/udev/devices...
   * the /dev/root rule (not sure if that makes terribly much sense, it never did run properly before anyway)
   * coldplug support
   * re-implement udevsettle in C */
void linux_udev_run() {
 struct stat st;

 mount ("proc", "/proc", "proc", 0, NULL);
 mount ("sys", "/sys", "sysfs", 0, NULL);
 mount ("udev", "/dev", "tmpfs", 0, NULL);

 mkdir ("/dev/pts", 0777);
 mount ("devpts", "/dev/pts", "devpts", 0, NULL);

 mkdir ("/dev/shm", 0777);
 mount ("shm", "/dev/shm", "tmpfs", 0, NULL);

 dev_t ldev = (5 << 8) | 1;
 mknod ("/dev/console", S_IFCHR, ldev);
 ldev = (5 << 8) | 0;
 mknod ("/dev/tty", S_IFCHR, ldev);
 ldev = (1 << 8) | 3;
 mknod ("/dev/null", S_IFCHR, ldev);

 char min = 0;
 for (; min < 24; min++) {
  ldev = (4 << 8) | min;
  char buffer[BUFFERSIZE];
  esprintf (buffer, BUFFERSIZE, "/dev/tty%d", min);
  mknod (buffer, S_IFCHR, ldev);
 }

 symlink ("/proc/self/fd", "/dev/fd");
 symlink ("fd/0", "/dev/stdin");
 symlink ("fd/1", "/dev/stdout");
 symlink ("fd/2", "/dev/stderr");

 struct einit_event eml = evstaticinit(einit_boot_dev_writable);
 event_emit (&eml, 0);
 evstaticdestroy(eml);

 if (!stat ("/proc/kcore", &st)) {
  /* create kernel core symlink */
  symlink ("/proc/kcore", "/dev/core");
 }

 if (!stat ("/proc/sys/kernel/hotplug", &st)) {
 /* i should check for an appropriate kernel version here... 2.6.14 methinks */
 /* set netlink handler */
  FILE *f = fopen ("/proc/sys/kernel/hotplug", "w");
  if (f) {
   fputs ("\n", f);
   fclose (f);
  }
 }

 fputs ("starting udev...\n", stderr);

 char *xtx[] = { "/sbin/udevd", NULL };
 einit_exec_without_shell_with_function_on_process_death_options (xtx, linux_udev_post_execute, thismodule, einit_exec_keep_stdin | einit_exec_daemonise);
}

void linux_udev_shutdown_vgchange (struct einit_exec_data *xd) {
}

void linux_udev_shutdown_imminent() {
 struct stat st;

 if (!stat ("/sbin/vgchange", &st)) {
  char *xtx[] = { "/sbin/vgchange", "-a", "n", NULL };
  einit_exec_without_shell_with_function_on_process_death (xtx, linux_udev_shutdown_vgchange, thismodule);
 }
}

int linux_udev_configure (struct lmodule *pa) {
 module_init (pa);

 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 if (strcmp (dm, "udev")) {
  return status_configure_failed | status_not_in_use;
 }

 event_listen (einit_boot_early, linux_udev_run);
 event_listen (einit_power_down_imminent, linux_udev_shutdown_imminent);
 event_listen (einit_power_reset_imminent, linux_udev_shutdown_imminent);

 return 0;
}
