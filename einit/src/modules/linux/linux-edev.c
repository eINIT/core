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
#include <sys/ioctl.h>
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
#include <linux/cdrom.h>

#include <sys/socket.h>

#include <fcntl.h>

#include <regex.h>

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

struct stree *linux_edev_gids = NULL;
struct stree *linux_edev_uids = NULL;
struct stree *linux_edev_compiled_regexes = NULL;

char ***linux_edev_device_rules = NULL;
pthread_mutex_t linux_edev_device_rules_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void linux_edev_mkdir_p (char *path) {
 if (path) {
  char **path_components = str2set ('/', path);
  int i = 0;
  char **cur = NULL;

  for (; path_components[i] && path_components[i+1]; i++) {
   char *p = NULL;

   cur = (char **)setadd ((void **)cur, path_components[i], SET_TYPE_STRING);

   if (cur) {
    p = set2str ('/', (const char **)cur);

    if (p) {
     mkdir (p, 0777);
     free (p);
     p = NULL;
    }
   }
  }

  if (cur)
   free (cur);
 }
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
   unsigned char major = 0;
   unsigned char minor = 0;
   char have_id = 0;
   char blockdevice = 0;
   char cdrom = 0;

   for (i = 0; args[i]; i+=2) {
    if (strmatch (args[i], "MAJOR")) {
     have_id = 1;
     major = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "MINOR")) {
     have_id = 1;
     minor = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "DEVPATH")) {
     device = estrdup(args[i+1]);
    }
   }

   if (have_id && device) {
    dev_t ldev = (((major) << 8) | (minor));
    char *base = strrchr (device, '/');
    if (base && (base[1] || ((base = strrchr (base, '/')) && base[1]))) {
     char *devicefile = NULL;
     char **symlinks = NULL;
     char *group = NULL;
     char *user = NULL;
     mode_t chmode = 0;

     base++;

     args = (char **)setadd ((void **)args, "DEVPATH_BASE", SET_TYPE_STRING);
     args = (char **)setadd ((void **)args, base, SET_TYPE_STRING);

     char *tmpsysdev = joinpath("/sys", device);

     if (tmpsysdev) {
      char *tn;
      char *data;

      tn = joinpath(tmpsysdev, "vendor");
      if (tn) {
       if ((data = readfile (tn))) {
        args = (char **)setadd ((void **)args, "VENDOR", SET_TYPE_STRING);
        args = (char **)setadd ((void **)args, data, SET_TYPE_STRING);

        free (data);
       }

       free (tn);
      }

      tn = joinpath(tmpsysdev, "device");
      if (tn) {
       if ((data = readfile (tn))) {
        args = (char **)setadd ((void **)args, "DEVICE", SET_TYPE_STRING);
        args = (char **)setadd ((void **)args, data, SET_TYPE_STRING);

        free (data);
       }

       free (tn);
      }

      tn = joinpath(tmpsysdev, "class");
      if (tn) {
       if ((data = readfile (tn))) {
        args = (char **)setadd ((void **)args, "CLASS", SET_TYPE_STRING);
        args = (char **)setadd ((void **)args, data, SET_TYPE_STRING);

        free (data);
       }

       free (tn);
      }

      free (tmpsysdev);
     }

     emutex_lock (&linux_edev_device_rules_mutex);
     if (linux_edev_device_rules) {
      for (i = 0; linux_edev_device_rules[i]; i++) {
       int j;
       char match = 1;

       for (j = 0; args[j]; j += 2) {
        int k;

        for (k = 0; linux_edev_device_rules[i][k]; k += 2) {
         if (strmatch (args[j], linux_edev_device_rules[i][k])) { // and we got something to match
          struct stree *s = streefind (linux_edev_compiled_regexes, linux_edev_device_rules[i][k+1], tree_find_first);
          regex_t *reg;

          if (s) {
           reg = s->value;
          } else {
           reg = emalloc (sizeof (regex_t));
           if (regcomp (reg, linux_edev_device_rules[i][k+1], REG_NOSUB | REG_EXTENDED) == 0) {
            linux_edev_compiled_regexes = streeadd (linux_edev_compiled_regexes, linux_edev_device_rules[i][k+1], reg, SET_NOALLOC, NULL);
           } else {
            free (reg);
            reg = NULL;
           }
          }

          if (reg) {
           if (regexec (reg, args[j+1], 0, NULL, 0) == REG_NOMATCH) {
            match = 0;
            break;
           }
          }
         }
        }
       }

       if (match) {
//        notice (1, "have match!");
        for (j = 0; linux_edev_device_rules[i][j] && linux_edev_device_rules[i][j+1]; j += 2) {
         if (!devicefile && strmatch (linux_edev_device_rules[i][j], "devicefile")) {
          devicefile = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
         } else if (strmatch (linux_edev_device_rules[i][j], "symlink")) {
          char *symlink = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
          if (symlink) {
           symlinks = (char **)setadd ((void **)symlinks, symlink, SET_TYPE_STRING);
           free (symlink);
          }
         } else if (!chmode && strmatch (linux_edev_device_rules[i][j], "chmod")) {
          chmode = parse_integer (linux_edev_device_rules[i][j+1]);
         } else if (!user && strmatch (linux_edev_device_rules[i][j], "user")) {
          user = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
         } else if (!group && strmatch (linux_edev_device_rules[i][j], "group")) {
          group = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
         } else if (strmatch (linux_edev_device_rules[i][j], "blockdevice")) {
          blockdevice = parse_boolean (linux_edev_device_rules[i][j+1]);
         } else if (strmatch (linux_edev_device_rules[i][j], "cdrom")) {
          cdrom = parse_boolean (linux_edev_device_rules[i][j+1]);
         }
        }
       }
      }
     }
     emutex_unlock (&linux_edev_device_rules_mutex);

     if (devicefile) {
      ev.string = devicefile;
      linux_edev_mkdir_p (devicefile);
      struct stree *ts = NULL;
      uid_t uid = 0;
      uid_t gid = 0;
      struct stat st;

      if (stat (devicefile, &st) && (mknod (devicefile, blockdevice ? S_IFBLK : S_IFCHR, ldev) != 0)) {
       linux_edev_mkdir_p (devicefile);

       if (mknod (devicefile, blockdevice ? S_IFBLK : S_IFCHR, ldev) != 0) {
        goto no_devicefile;
       }
      }

      chmod (devicefile, chmode);

      if (user) {
       if ((ts = streefind (linux_edev_uids, user, tree_find_first))) {
        uintptr_t xn = ts->value;
        uid = (uid_t)xn;
       } else {
        lookupuidgid (&uid, NULL, user, NULL);
        uintptr_t xn = uid;
        linux_edev_uids = streeadd (linux_edev_uids, user, (void *)xn, SET_NOALLOC, NULL);
       }
      }

      if (group) {
       if ((ts = streefind (linux_edev_gids, group, tree_find_first))) {
        uintptr_t xn = ts->value;
        gid = (uid_t)xn;
       } else {
        lookupuidgid (NULL, &gid, NULL, group);
        uintptr_t xn = gid;
        linux_edev_gids = streeadd (linux_edev_gids, user, (void *)xn, SET_NOALLOC, NULL);
       }
      }

      if (user || group) {
       chown (devicefile, uid, gid);
      }

	  if (cdrom) {
	   linux_edev_get_cdrom_capabilities(devicefile);
	  }
	  
      if (symlinks) {
       for (i = 0; symlinks[i]; i++) {
        if (symlink (devicefile, symlinks[i]) != 0) {
         linux_edev_mkdir_p (symlinks[i]);
         symlink (devicefile, symlinks[i]);
        }
       }
      }
     }
    }
   }

   if (device) free (device);
  }

  no_devicefile:

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

void linux_edev_retrieve_rules();

int linux_edev_run() {
 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 linux_edev_retrieve_rules();

 if (!linux_edev_enabled && strmatch (dm, "edev")) {
  linux_edev_enabled = 1;

  mount ("proc", "/proc", "proc", 0, NULL);
  mount ("sys", "/sys", "sysfs", 0, NULL);
  mount ("edev", "/dev", "tmpfs", 0, NULL);

  mkdir ("/dev/pts", 0777);
  mount ("devpts", "/dev/pts", "devpts", 0, NULL);

  mkdir ("/dev/shm", 0777);
  mount ("shm", "/dev/shm", "tmpfs", 0, NULL);

  symlink ("/proc/self/fd", "/dev/fd");
  symlink ("fd/0", "/dev/stdin");
  symlink ("fd/1", "/dev/stdout");
  symlink ("fd/2", "/dev/stderr");

  ethread_spawn_detached ((void *(*)(void *))linux_edev_hotplug, (void *)NULL);

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

  linux_edev_ping_for_uevents("/sys", 6);

  if (coremode & einit_mode_sandbox) {
   while (1) {
    sleep (1);
   }
  }

  linux_edev_load_kernel_extensions();

  mount ("usbfs", "/proc/bus/usb", "usbfs", 0, NULL);

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
 if (linux_edev_run() == status_ok) {
  struct einit_event eml = evstaticinit(einit_boot_devices_available);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
 }
}

void linux_edev_retrieve_rules () {
 char ***new_rules = NULL;
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode ("configuration-edev-devicefile-rule", 0, node))) {
  if (node->arbattrs) {
   char **e = (char **)setdup ((const void **)node->arbattrs, SET_TYPE_STRING);

   new_rules = (char ***)setadd ((void **)new_rules, (void *)e, SET_NOALLOC);
  }
 }

 emutex_lock (&linux_edev_device_rules_mutex);
 if (linux_edev_device_rules) {
  int i = 0;
  for (; linux_edev_device_rules[i]; i++) {
   free (linux_edev_device_rules[i]);
  }
  free (linux_edev_device_rules);
 }
 linux_edev_device_rules = new_rules;
 emutex_unlock (&linux_edev_device_rules_mutex);
}

int linux_edev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_event_subsystem_power, linux_edev_power_event_handler);
 event_ignore (einit_boot_early, linux_edev_boot_event_handler);
 event_ignore (einit_core_configuration_update, linux_edev_retrieve_rules);

 return 0;
}

int linux_edev_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure(pa);

 pa->cleanup = linux_edev_cleanup;

 event_listen (einit_core_configuration_update, linux_edev_retrieve_rules);
 event_listen (einit_boot_early, linux_edev_boot_event_handler);
 event_listen (einit_event_subsystem_power, linux_edev_power_event_handler);

 return 0;
}

int linux_edev_get_cdrom_capabilities (char devicefile) {
 int out,fd;
 fd = open(devicefile, O_RDONLY|O_NONBLOCK);
 if (fd < 0) {
  close(fd);
  return 1;
 }
 out = ioctl(fd, CDROM_GET_CAPABILITY, NULL);
 if (out < 0) {
  close(fd);
  return 1;
 }
 printf("ID_CDROM=1\n");
 if (out & CDC_CD_R)
  printf("ID_CDROM_CD_R=1\n");
 if (out & CDC_CD_RW)
  printf("ID_CDROM_CD_RW=1\n");
 if (out & CDC_DVD)
  printf("ID_CDROM_DVD=1\n");
 if (out & CDC_DVD_R)
  printf("ID_CDROM_DVD_R=1\n");
 if (out & CDC_DVD_RAM)
  printf("ID_CDROM_DVD_RAM=1\n");
 if (out & CDC_MRW)
  printf("ID_CDROM_MRW=1\n");
 if (out & CDC_MRW_W)
  printf("ID_CDROM_MRW_W=1\n");
 if (out & CDC_RAM)
  printf("ID_CDROM_RAM=1\n");
 return 0;
}
