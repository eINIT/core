/*
 *  linux-edev.c
 *  einit
 *
 *  Created on 12/10/2007.
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
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#include <pthread.h>

#include <einit-modules/exec.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include <linux/types.h>
#include <linux/netlink.h>

#include <sys/socket.h>

#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_edev_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_edev_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Device Setup (Linux, Internal)",
 .rid       = "linux-edev",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_edev_configure
};

module_register(linux_edev_self);

#endif

char linux_edev_enabled = 0;

#define NETLINK_BUFFER 1024*1024*64

void linux_edev_load_kernel_extensions() {
 struct einit_event eml = evstaticinit(einit_boot_load_kernel_extensions);
 event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
 evstaticdestroy(eml);
}

void linux_edev_ping_for_uevents(char *dir, char depth) {
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
       linux_edev_ping_for_uevents (f, depth - 1);
      }
     }

     free (f);
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

 free (x);
}

void linux_edev_hotplug_handle (char **v) {
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

  if (args && ((ev.type == einit_hotplug_add) || (ev.type == einit_hotplug_remove))) {
   char *device = NULL;
   char *subsystem = NULL;
   unsigned char major = 0;
   unsigned char minor = 0;
   char have_id = 0;

   for (i = 0; args[i]; i+=2) {
    if (strmatch (args[i], "MAJOR")) {
     have_id = 1;
     major = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "MINOR")) {
     have_id = 1;
     minor = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "DEVPATH")) {
     device = args[i+1];
    } else if (strmatch (args[i], "SUBSYSTEM")) {
     subsystem = args[i+1];
    }
   }

   if (have_id && device) {
    dev_t ldev = (((major) << 8) | (minor));
    char *base = strrchr (device, '/');
    if (base) {
     base++;
#define DEVPREFIX "/dev"
#define DEVPREFIX_TEMPLATE DEVPREFIX "/%s"
     int len = strlen (base) + sizeof (DEVPREFIX) + 3;
     char *devpath = emalloc (len);

     esprintf (devpath, len, DEVPREFIX_TEMPLATE, base);

     if (subsystem) {
#define DEVPREFIX_SUBSYS DEVPREFIX "/%s"
#define DEVPREFIX_SUBSYS_TEMPLATE DEVPREFIX_SUBSYS "/%s"
      int len_subsys = strlen (base) + strlen (subsystem) + sizeof (DEVPREFIX_SUBSYS) +2;
      char *devpath_subsys = emalloc (len_subsys);

      if (ev.type == einit_hotplug_add) {
       char *devpath_dir = joinpath (DEVPREFIX, subsystem);

       mkdir (devpath_dir, 0660);
       free (devpath_dir);
      }

      esprintf (devpath_subsys, len_subsys, DEVPREFIX_SUBSYS_TEMPLATE, subsystem, base);

      if (ev.type == einit_hotplug_add) {
       if (strstr (device, "/block/") == device) {
        mknod (devpath_subsys, S_IFBLK, ldev);
       } else {
        mknod (devpath_subsys, S_IFCHR, ldev);
       }

       chmod (devpath_subsys, 0660);

       symlink (devpath_subsys, devpath);
      } else if (ev.type == einit_hotplug_remove) {
       unlink (devpath_subsys);
       unlink (devpath);
      }

      ev.string = devpath_subsys;

      free (devpath);
     } else {
      if (ev.type == einit_hotplug_add) {
       if (strstr (device, "/block/") == device) {
        mknod (devpath, S_IFBLK, ldev);
       } else {
        mknod (devpath, S_IFCHR, ldev);
       }

       chmod (devpath, 0660);
      } else if (ev.type == einit_hotplug_remove) {
       unlink (devpath);
      }

      ev.string = devpath;
     }
    }
   }
  }

  ev.stringset = args;

/* emit the event, waiting for it to be processed */
  event_emit (&ev, einit_event_flag_broadcast);

  if (ev.string) free (ev.string);

  evstaticdestroy (ev);

  if (args) free (args);
 }
}

void *linux_edev_hotplug(void *ignored) {
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
   perror ("edev/read");

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
      linux_edev_hotplug_handle(v);

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
//   fputs ("terminal message\n", stderr);
//   fflush (stderr);

   if (v) {
    linux_edev_hotplug_handle(v);

    free (v);
    v = NULL;
   }
  }

  errno = 0;
 }

 if (v) {
  linux_edev_hotplug_handle(v);

  free (v);
  v = NULL;
 }

 close (fd);

 if (errno) {
  perror ("edev");
 }

 sleep (1);
 goto redo;

 done:

 notice (1, "hotplug thread exiting... respawning in 10 sec");

 sleep (10);
 return linux_edev_hotplug (NULL);
}

int linux_edev_run() {
 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 if (!linux_edev_enabled && strmatch (dm, "edev")) {
  pthread_t th;
  linux_edev_enabled = 1;

  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);
  mount ("edev", "/dev", "tmpfs", 0, NULL);

  //mkdir ("/dev/pts", 0660);
  mount ("devpts", "/dev/pts", "devpts", 0, NULL);

  //mkdir ("/dev/shm", 0660);
  mount ("shm", "/dev/shm", "tmpfs", 0, NULL);

  symlink ("/proc/self/fd", "/dev/fd");
  symlink ("fd/0", "/dev/stdin");
  symlink ("fd/1", "/dev/stdout");
  symlink ("fd/2", "/dev/stderr");

  ethread_create (&th, &thread_attribute_detached, linux_edev_hotplug, NULL);

  FILE *he = fopen ("/proc/sys/kernel/hotplug", "w");
  if (he) {
   fputs ("", he);
   fclose (he);
  }

  linux_edev_ping_for_uevents("/sys", 6);

  if (coremode & einit_mode_sandbox) {
   while (1) {
    sleep (1);
   }
  }

  linux_edev_load_kernel_extensions();

  return status_ok;
 }

 return status_failed;
}

int linux_edev_shutdown() {
 if (linux_edev_enabled) {
  linux_edev_enabled = 0;

  return status_ok;
 } else
  return status_failed;
}

void linux_edev_power_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_power_down_scheduled) || (ev->type == einit_power_reset_scheduled)) {
  linux_edev_shutdown();
 }
}

void linux_edev_boot_event_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_boot_early:
   if (linux_edev_run() == status_ok) {
    struct einit_event eml = evstaticinit(einit_boot_devices_available);
    event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
    evstaticdestroy(eml);
   }

   /* some code */
   break;

  default: break;
 }
}

int linux_edev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_edev_power_event_handler);
 event_ignore (einit_event_subsystem_boot, linux_edev_boot_event_handler);

 return 0;
}

int linux_edev_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_edev_cleanup;

 event_listen (einit_event_subsystem_boot, linux_edev_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_edev_power_event_handler);

 return 0;
}
