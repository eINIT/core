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
#include <sys/stat.h>

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

struct dexecinfo linux_udev_dexec = {
 .id = "daemon-udev",
 .command = "/sbin/udevd",
 .prepare = NULL,
 .cleanup = NULL,
 .is_up = NULL,
 .is_down = NULL,
 .variables = NULL,
 .uid = 0,
 .gid = 0,
 .user = NULL, .group = NULL,
 .restart = 1,
 .cb = NULL,
 .environment = NULL,
 .pidfile = NULL,
 .need_files = NULL,
 .oattrs = NULL,

 .options = 0,

 .pidfiles_last_update = 0,

 .script = NULL,
 .script_actions = NULL
};

#if 0
void linux_udev_ping_for_uevents(char *dir, char depth) {
 struct stat st;

 if (!dir || lstat (dir, &st)) return;

 if (S_ISLNK (st.st_mode)) {
  return;
 }

 if (S_ISDIR (st.st_mode)) {
  DIR *d;
  struct dirent *e;

  d = eopendir (dir);
  if (d != NULL) {
   while ((e = ereaddir (d))) {
    if (strmatch (e->d_name, ".") || strmatch (e->d_name, "..")) {
     continue;
    }

    char *f = joinpath ((char *)dir, e->d_name);

    if (f) {
     if (!lstat (f, &st) && !S_ISLNK (st.st_mode) && S_ISDIR (st.st_mode)) {
      if (depth > 0) {
       linux_udev_ping_for_uevents (f, depth - 1);
      }
     }

     efree (f);
    }
   }

   eclosedir(d);
  }
 }

 char *x = joinpath (dir, "uevent");
 FILE *uev = fopen (x, "w");

 if (uev) {
  fputs ("add", uev);
  fclose (uev);
 }

 efree (x);
}
#endif

/* TODO: maybe... maybe not, anyway...
   * using tarballs
   * copy devices from /lib/udev/devices...
   * the /dev/root rule (not sure if that makes terribly much sense, it never did run properly before anyway)
   * coldplug support
   * re-implement udevsettle in C */

int linux_udev_run() {
 if (!linux_udev_enabled) {
  linux_udev_enabled = 1;
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
  ldev = (4 << 8) | 1;
  mknod ("/dev/tty1", S_IFCHR, ldev);
  ldev = (1 << 8) | 3;
  mknod ("/dev/null", S_IFCHR, ldev);

  symlink ("/proc/self/fd", "/dev/fd");
  symlink ("fd/0", "/dev/stdin");
  symlink ("fd/1", "/dev/stdout");
  symlink ("fd/2", "/dev/stderr");

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

  startdaemon(&linux_udev_dexec, NULL);

  struct cfgnode *n = cfg_getnode ("configuration-system-coldplug", NULL);
/* again, i should check for an appropriate kernel version... */
/* this should be all nodes that'll be needed... */

/*
  if (n && n->flag) {
   linux_udev_ping_for_uevents("/sys", 5);
  } else {
   linux_udev_ping_for_uevents("/sys/class", 4);
   linux_udev_ping_for_uevents("/sys/block", 3);
  }
*/

  if (n && n->flag) {
   qexec ("/sbin/udevadm trigger");
  } else {
   qexec ("/sbin/udevadm trigger --attr-match=dev");
  }

//  qexec (EINIT_LIB_BASE "/modules-xml/udev.sh enable");

  linux_udev_load_kernel_extensions();

  sleep (1);

  qexec ("/sbin/udevadm settle --timeout=60");

  mount ("usbfs", "/proc/bus/usb", "usbfs", 0, NULL);

/* let's not forget about raid setups and the like here... */
  if (!stat ("/sbin/lvm", &st)) {
   qexec ("/sbin/lvm vgscan -P --mknodes --ignorelockingfailure");
  }
  if (!stat ("/sbin/evms_activate", &st)) {
   qexec ("/sbin/evms_activate -q");
  }

  qexec ("/sbin/udevsettle --timeout=60");

  return status_ok;
 } else
  return status_failed;
}

void linux_udev_shutdown() {
 if (linux_udev_enabled) {
/*  qexec (EINIT_LIB_BASE "/modules-xml/udev.sh on-shutdown");
  qexec (EINIT_LIB_BASE "/modules-xml/udev.sh disable");*/
  stopdaemon (&linux_udev_dexec, NULL);
 }
}

void linux_udev_shutdown_imminent() {
 struct stat st;

 if (linux_udev_enabled) {
  if (!stat ("/sbin/vgchange", &st)) {
   qexec ("/sbin/vgchange -a n");
  }

  linux_udev_enabled = 0;
 }
}

void linux_udev_boot_event_handler (struct einit_event *ev) {
 if (linux_udev_run() == status_ok) {
  struct stat st;

  struct einit_event eml = evstaticinit(einit_boot_devices_available);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);

  if (!stat ("/sbin/vgchange", &st)) {
   qexec ("/sbin/vgchange -a y");
  }
 }
}

int linux_udev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_boot_early, linux_udev_boot_event_handler);
 event_ignore (einit_power_down_scheduled, linux_udev_shutdown);
 event_ignore (einit_power_reset_scheduled, linux_udev_shutdown);
 event_ignore (einit_power_down_imminent, linux_udev_shutdown_imminent);
 event_ignore (einit_power_reset_imminent, linux_udev_shutdown_imminent);

 return 0;
}

int linux_udev_configure (struct lmodule *pa) {
 module_init (pa);

 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 if (strcmp (dm, "udev")) {
  return status_configure_failed | status_not_in_use;
 }

 exec_configure(pa);

 pa->cleanup = linux_udev_cleanup;

 event_listen (einit_boot_early, linux_udev_boot_event_handler);
 event_listen (einit_power_down_scheduled, linux_udev_shutdown);
 event_listen (einit_power_reset_scheduled, linux_udev_shutdown);
 event_listen (einit_power_down_imminent, linux_udev_shutdown_imminent);
 event_listen (einit_power_reset_imminent, linux_udev_shutdown_imminent);

 return 0;
}
