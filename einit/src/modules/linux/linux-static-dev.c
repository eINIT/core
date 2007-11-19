/*
 *  linux-static-dev.c
 *  einit
 *
 *  Created on 17/10/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
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

#include <einit-modules/exec.h>

#include <pthread.h>

#include <sys/mount.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <sys/socket.h>

#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_static_dev_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_static_dev_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Device Setup (Linux, static)",
 .rid       = "linux-static_dev",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_static_dev_configure
};

module_register(linux_static_dev_self);

#endif

char linux_static_dev_enabled = 0;

#define NETLINK_BUFFER 1024*1024*64

void linux_static_dev_load_kernel_extensions() {
 struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
 evstaticdestroy(eml);
}

void linux_static_dev_hotplug_handle (char **v) {
 if (v && v[0]) {
  int i = 0;
  char **args = NULL;
  struct einit_event ev = evstaticinit(einit_hotplug_generic);

  if (strstr (v[0], "add@") == v[0]) {
   ev.type = einit_hotplug_add;
  } else if (strstr (v[0], "remove@") == v[0]) {
   ev.type = einit_hotplug_remove;
  } else if (strstr (v[0], "change@") == v[0]) {
   ev.type = einit_hotplug_change;
  } else if (strstr (v[0], "online@") == v[0]) {
   ev.type = einit_hotplug_online;
  } else if (strstr (v[0], "offline@") == v[0]) {
   ev.type = einit_hotplug_offline;
  } else if (strstr (v[0], "move@") == v[0]) {
   ev.type = einit_hotplug_move;
  }

  for (i = 1; v[i]; i++) {
   char *n = strchr (v[i], '=');
   if (n) {
    *n = 0;
    n++;

    args = (char **)setadd ((void **)args, v[i], SET_TYPE_STRING);
    args = (char **)setadd ((void **)args, n, SET_TYPE_STRING);
   }
  }

  ev.stringset = args;

/* emit the event, waiting for it to be processed */
  event_emit (&ev, einit_event_flag_broadcast);
  evstaticdestroy (ev);

  if (args) free (args);
 }
}

void *linux_static_dev_hotplug(void *ignored) {
 struct sockaddr_nl nls;
 int fd, pos = 0;
 char buffer[BUFFERSIZE];

 redo:

 memset(&nls, 0, sizeof(struct sockaddr_nl));
 nls.nl_family = AF_NETLINK;
 nls.nl_pid = getpid();
 nls.nl_groups = -1;

 fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

 if (fd == -1) goto done;

 if (bind(fd, (void *)&nls, sizeof(struct sockaddr_nl))) goto done;

 errno = 0;

 char **v = NULL;

 int newlength = NETLINK_BUFFER;

 if (setsockopt (fd, SOL_SOCKET, SO_RCVBUF, &newlength, sizeof (int))) {
  perror ("setsockopt: can't increase buffer size");
 }

 if (fcntl (fd, F_SETFD, FD_CLOEXEC)) {
  perror ("can't set close-on-exec flag");
 }

 while (!errno || (errno == EAGAIN) || (errno == ESPIPE) || (errno == EINTR)) {
  int rp = read (fd, buffer + pos, BUFFERSIZE - pos);
  int i = 0;
  char last = rp < (BUFFERSIZE - pos);

  if ((rp == -1) && !(!errno || (errno == EAGAIN) || (errno == ESPIPE) || (errno == EINTR))) {
   perror ("static_dev/read");

   continue;
  }

  pos += rp;
  buffer[rp] = 0;

  for (i = 0; (i < pos); i++) {
   if (((buffer[i] == 0)) && (i > 0)) {
    char lbuffer[BUFFERSIZE];
    int offset = 0;

    for (offset = 0; (offset < i) && !buffer[offset]; offset++) {
     offset++;
    }

    memcpy (lbuffer, buffer + offset, i - offset +1);
    if ((strstr (lbuffer, "add@") == lbuffer) ||
        (strstr (lbuffer, "remove@") == lbuffer) ||
        (strstr (lbuffer, "change@") == lbuffer) ||
        (strstr (lbuffer, "online@") == lbuffer) ||
        (strstr (lbuffer, "offline@") == lbuffer) ||
        (strstr (lbuffer, "move@") == lbuffer)) {

     if (v) {
      linux_static_dev_hotplug_handle(v);

      free (v);
      v = NULL;
     }
    }

    v = (char **)setadd ((void **)v, lbuffer, SET_TYPE_STRING);

    i++;

    memmove (buffer, buffer + offset + i, pos - i);
    pos -= i;

    i = -1;
   }
  }

/* we got less than we requested, assume that was a last message */
  if (last) {
   if (v) {
    linux_static_dev_hotplug_handle(v);

    free (v);
    v = NULL;
   }
  }

  errno = 0;
 }

 if (v) {
  linux_static_dev_hotplug_handle(v);

  free (v);
  v = NULL;
 }

 close (fd);

 if (errno) {
  perror ("static_dev");
 }

 sleep (1);
 goto redo;

 done:

 notice (1, "hotplug thread exiting... respawning in 10 sec");

 sleep (10);
 return linux_static_dev_hotplug (NULL);
}

int linux_static_dev_run() {
 char *dm;

 if (!linux_static_dev_enabled && (dm = cfg_getstring("configuration-system-device-manager", NULL)) && strmatch (dm, "static")) {
  linux_static_dev_enabled = 1;

  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);

  mount ("devpts", "/dev/pts", "devpts", 0, NULL);

  mount ("shm", "/dev/shm", "tmpfs", 0, NULL);

  ethread_spawn_detached ((void *(*)(void *))linux_static_dev_hotplug, (void *)NULL);

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

  linux_static_dev_load_kernel_extensions();

  mount ("usbfs", "/proc/bus/usb", "usbfs", 0, NULL);

  return status_ok;
 } else
  return status_failed;
}

int linux_static_dev_shutdown() {
 return status_ok;
}

void linux_static_dev_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  linux_static_dev_shutdown();
 }
}

void linux_static_dev_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   if (linux_static_dev_run() == status_ok) {
    struct einit_event eml = evstaticinit(einit_boot_devices_available);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }
   break;

  default: break;
 }
}

int linux_static_dev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_static_dev_power_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_static_dev_boot_event_handler);

 return 0;
}

int linux_static_dev_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_static_dev_cleanup;

 event_listen (einit_event_subsystem_boot, linux_static_dev_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_static_dev_power_event_handler);

 return 0;
}
