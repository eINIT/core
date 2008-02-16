/*
 *  linux-edev.c
 *  einit
 *
 *  Created on 12/10/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *  Copyright 2007 Ryan Hope. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger, Ryan Hope
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>


#include <pthread.h>

#include <einit-modules/exec.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/cdrom.h>
#include <linux/hdreg.h>

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

#define NETLINK_BUFFER 1024*1024*128

struct stree *linux_edev_gids = NULL;
struct stree *linux_edev_uids = NULL;
struct stree *linux_edev_compiled_regexes = NULL;

char ***linux_edev_device_rules = NULL;
pthread_mutex_t linux_edev_device_rules_mutex = PTHREAD_MUTEX_INITIALIZER;

//int **linux_edev_socket_relay(char **args, const char *sockn, const char *dp, const char *a);
char **linux_edev_get_cdrom_capabilities (char **args, char *devicefile);
char **linux_edev_get_ata_identity (char **args, char *devicefile);
char *linux_edev_mangle_filename (char *filename, char do_free);

static void set_str(char *to, const char *from, size_t count);

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

void linux_edev_mkdir_p (char *path) {
 if (path) {
  char **path_components = str2set ('/', path);
  int i = 0;
  char **cur = NULL;

  for (; path_components[i] && path_components[i+1]; i++) {
   char *p = NULL;

   cur = set_str_add (cur, path_components[i]);

   if (cur) {
    p = set2str ('/', (const char **)cur);

    if (p) {
     mkdir (p, 0777);
     efree (p);
     p = NULL;
    }
   }
  }

  if (cur)
   efree (cur);
 }
}

size_t strlcpy(char *, const char *, size_t);
size_t strlcpy(char *dst, const char *src, size_t siz)
{
 char *d = dst;
 const char *s = src;
 size_t n = siz;
 if (n != 0 && --n != 0) {
  do {
   if ((*d++ = *s++) == 0)
    break;
  } while (--n != 0);
 }
 if (n == 0) {
  if (siz != 0)
   *d = '\0';
  while (*s++)
   ;
  }
 return(s - src - 1);
}

void linux_edev_hotplug_handle (char **v) {
 const char *a = v[0];
 if (v && a) {
  int i = 0;
  char **args = NULL;
  struct einit_event ev = evstaticinit(einit_hotplug_generic);

  if (strprefix (v[0], "add@")) {
   ev.type = einit_hotplug_add;
  } else if (strprefix (v[0], "remove@")) {
   ev.type = einit_hotplug_remove;
  } else if (strprefix (v[0], "change@")) {
   ev.type = einit_hotplug_change;
  } else if (strprefix (v[0], "online@")) {
   ev.type = einit_hotplug_online;
  } else if (strprefix (v[0], "offline@")) {
   ev.type = einit_hotplug_offline;
  } else if (strprefix (v[0], "move@")) {
   ev.type = einit_hotplug_move;
  }

  for (i = 1; v[i]; i++) {
   char *n = strchr (v[i], '=');
   if (n) {
    *n = 0;
    n++;
    args = set_str_add (args, v[i]);
    args = set_str_add (args, n);
   }
  }

  if (args && ((ev.type == einit_hotplug_add) || (ev.type == einit_hotplug_remove))) {
   char *device = NULL;
   unsigned char major = 0;
   unsigned char minor = 0;
   char have_id, blockdevice = 0;
   char *subsys = NULL;

   for (i = 0; args[i]; i+=2) {
     if (strmatch (args[i], "MAJOR")) {
     have_id = 1;
     major = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "MINOR")) {
     have_id = 1;
     minor = parse_integer (args[i+1]);
    } else if (strmatch (args[i], "DEVPATH")) {
     device = (char *)str_stabilise(args[i+1]);
    } else if (strmatch (args[i], "SUBSYSTEM")) {
     subsys = (char *)str_stabilise(args[i+1]);
    }
   }

   if (have_id && device && (!subsys || !strmatch (subsys, "module"))) {
    dev_t ldev = (((major) << 8) | (minor));
    char *base = strrchr (device, '/');
    if (base && (base[1] || ((base = strrchr (base, '/')) && base[1]))) {
     char *devicefile = NULL;
     char **symlinks = NULL;
//     char **sockets = NULL;
     char *group = NULL;
     char *user = NULL;
     mode_t chmode = 0;

     base++;

     args = set_str_add (args, "DEVPATH_BASE");
     args = set_str_add (args, base);

     char *tmpsysdev = joinpath("/sys", device);

     if (tmpsysdev) {
      char *tn;
      char *data;

      tn = joinpath(tmpsysdev, "vendor");
      if (tn) {
       if ((data = readfile (tn))) {
        args = set_str_add (args, "VENDOR");
        args = set_str_add (args, data);

        efree (data);
       }

       efree (tn);
      }

      tn = joinpath(tmpsysdev, "device");
      if (tn) {
       if ((data = readfile (tn))) {
        args = set_str_add (args, "DEVICE");
        args = set_str_add (args, data);

        efree (data);
       }

       efree (tn);
      }

      tn = joinpath(tmpsysdev, "class");
      if (tn) {
       if ((data = readfile (tn))) {
        args = set_str_add (args, "CLASS");
        args = set_str_add (args, data);

        efree (data);
       }

       efree (tn);
      }

      efree (tmpsysdev);
     }

     if(strmatch (subsys, "block")) {
      char *dummyblock = linux_edev_mangle_filename("/dev/.einit/block${NUM+}", 0);

      if (mknod (dummyblock, S_IFBLK, ldev) != 0) {
       perror ("mkdnod/dummy");
       linux_edev_mkdir_p (dummyblock);

       if (mknod (dummyblock, S_IFBLK, ldev) != 0) {
        perror ("mkdnod/dummy");
        goto noblock;
       }
      }

      args = linux_edev_get_cdrom_capabilities(args, dummyblock);
      args = linux_edev_get_ata_identity(args, dummyblock);

      efree (dummyblock);

#if 0
      char *gnb = set2str (',', (const char **)args);
      fprintf (stderr, " ** args: %s\n", gnb);
// fflush (stderr);
      efree (gnb);
#endif
     }

     noblock:

     emutex_lock (&linux_edev_device_rules_mutex);
     if (linux_edev_device_rules) {
      for (i = 0; linux_edev_device_rules[i]; i++) {
       int j, k;
       char match = 1;

/* see if all rules are set, skip the rule if not */
       for (k = 0; linux_edev_device_rules[i][k]; k += 2) {
        char could_match = 0;

        if (strmatch (linux_edev_device_rules[i][k], "symlink") ||
            strmatch (linux_edev_device_rules[i][k], "devicefile") ||
            strmatch (linux_edev_device_rules[i][k], "group") ||
            strmatch (linux_edev_device_rules[i][k], "user") ||
            strmatch (linux_edev_device_rules[i][k], "chmod") ||
            strmatch (linux_edev_device_rules[i][k], "socket") ||
            strmatch (linux_edev_device_rules[i][k], "blockdevice")) continue;
        else for (j = 0; args[j]; j += 2) {
         if (strmatch (linux_edev_device_rules[i][k], args[j]))
          could_match = 1;
        }

        if (!could_match) {
         match = 0;
         break;
        }
       }

       if (match)
        for (j = 0; args[j]; j += 2) {
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
             efree (reg);
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
          devicefile = linux_edev_mangle_filename (devicefile, 1);
         } else if (strmatch (linux_edev_device_rules[i][j], "symlink")) {
          char *symlink = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
          if (symlink) {
           symlink = linux_edev_mangle_filename (symlink, 1);
           symlinks = set_str_add (symlinks, symlink);
           efree (symlink);
          }
         } else if (!chmode && strmatch (linux_edev_device_rules[i][j], "chmod")) {
          chmode = parse_integer (linux_edev_device_rules[i][j+1]);
         } else if (!user && strmatch (linux_edev_device_rules[i][j], "user")) {
          user = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
         } else if (!group && strmatch (linux_edev_device_rules[i][j], "group")) {
          group = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
         } else if (strmatch (linux_edev_device_rules[i][j], "blockdevice")) {
          blockdevice = parse_boolean (linux_edev_device_rules[i][j+1]);
         }
#if 0
         else if (strmatch (linux_edev_device_rules[i][j], "socket")) {
          char *socket = apply_variables (linux_edev_device_rules[i][j+1], (const char **)args);
          if (socket) {
           sockets = set_str_add (sockets, socket);
           efree (socket);
          }
         }
#endif
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

       unlink (devicefile);

       if (mknod (devicefile, blockdevice ? S_IFBLK : S_IFCHR, ldev) != 0) {
        goto no_devicefile;
       }
      }

      chmod (devicefile, chmode);

      if (user) {
       if ((ts = streefind (linux_edev_uids, user, tree_find_first))) {
        uintptr_t xn = (uintptr_t)ts->value;
        uid = (uid_t)xn;
       } else {
        lookupuidgid (&uid, NULL, user, NULL);
        uintptr_t xn = uid;
        linux_edev_uids = streeadd (linux_edev_uids, user, (void *)xn, SET_NOALLOC, NULL);
       }
      }

      if (group) {
       if ((ts = streefind (linux_edev_gids, group, tree_find_first))) {
        uintptr_t xn = (uintptr_t)ts->value;
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

      if (symlinks) {
       for (i = 0; symlinks[i]; i++) {
        if (symlink (devicefile, symlinks[i]) != 0) {
         linux_edev_mkdir_p (symlinks[i]);
         symlink (devicefile, symlinks[i]);
        }
       }
      }
#if 0
      if (sockets) {
       for (i = 0; sockets[i]; i++) {
        if (symlink (devicefile, symlinks[i]) != 0) {
         linux_edev_socket_relay(args, sockets[i], device, a);
        }
       }
      }
#endif
     }
    }
   }
  }

  no_devicefile:

  ev.stringset = args;

/* emit the event, waiting for it to be processed */
  event_emit (&ev, einit_event_flag_broadcast);

  if (ev.string) efree (ev.string);

  evstaticdestroy (ev);

  if (args) efree (args);
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
    if ((strprefix (lbuffer, "add@")) ||
        (strprefix (lbuffer, "remove@")) ||
        (strprefix (lbuffer, "change@")) ||
        (strprefix (lbuffer, "online@")) ||
        (strprefix (lbuffer, "offline@")) ||
        (strprefix (lbuffer, "move@"))) {

     if (v) {
      linux_edev_hotplug_handle(v);

      efree (v);
      v = NULL;
     }
    }

    v = set_str_add (v, lbuffer);

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

    efree (v);
    v = NULL;
   }
  }

  errno = 0;
 }

 if (v) {
  linux_edev_hotplug_handle(v);

  efree (v);
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
 linux_edev_retrieve_rules();

 if (!linux_edev_enabled) {
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

//  linux_edev_ping_for_uevents("/sys", 6);
  linux_edev_ping_for_uevents("/sys/class", 4);
  linux_edev_ping_for_uevents("/sys/block", 3);

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

void linux_edev_shutdown() {
 if (linux_edev_enabled) {
  linux_edev_enabled = 0;
 }

 return;
}

void linux_edev_initramfs_event_handler (struct einit_event *ev) {
 if (linux_edev_run() == status_ok) {
  struct einit_event eml = evstaticinit(einit_boot_initramfs_done);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
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
   char **e = set_str_dup_stable (node->arbattrs);

   new_rules = (char ***)set_noa_add ((void **)new_rules, (void *)e);
  }
 }

 emutex_lock (&linux_edev_device_rules_mutex);
 if (linux_edev_device_rules) {
  int i = 0;
  for (; linux_edev_device_rules[i]; i++) {
   efree (linux_edev_device_rules[i]);
  }
  efree (linux_edev_device_rules);
 }
 linux_edev_device_rules = new_rules;
 emutex_unlock (&linux_edev_device_rules_mutex);
}

char **linux_edev_get_cdrom_capabilities (char **args, char *devicefile) {
 int out, fd;
 char **cdrom_attrs = NULL, *cdattrs = NULL;
 fd = open(devicefile, O_RDONLY|O_NONBLOCK);
 if (fd < 0) {
  perror ("opening device file (get_cdrom_capabilities)");
  close(fd);
  return args;
 }

 out = ioctl(fd, CDROM_GET_CAPABILITY, NULL);

 if (out < 0) {
  perror ("ioctl/CDROM_GET_CAPABILITY");
  close(fd);
  return args;
 }

 close(fd);

 cdrom_attrs = set_str_add (cdrom_attrs, "CDROM");
 if (out & CDC_CD_R)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_R");
 if (out & CDC_CD_RW)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_RW");
 if (out & CDC_DVD)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_DVD");
 if (out & CDC_DVD_R)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_DVD_R");
 if (out & CDC_DVD_RAM)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_DVD_RAM");
 if (out & CDC_MRW)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_MRW");
 if (out & CDC_MRW_W)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_MRW_W");
 if (out & CDC_RAM)
  cdrom_attrs = set_str_add (cdrom_attrs, "CD_RAM");

 cdattrs = set2str (':', (const char **)cdrom_attrs);
 efree (cdrom_attrs);

 args = set_str_add (args, "CDROM_ATTRIBUTES");
 args = set_str_add (args, cdattrs);

// notice (5, "CDROM ATTRIBUTES: %s", cdattrs);
 efree (cdattrs);

 return args;
}

static void set_str(char *to, const char *from, size_t count)
{
 size_t i, j, len;
 len = strnlen(from, count);
 while (len && isspace(from[len-1]))
  len--;
  i = 0;
 while (isspace(from[i]) && (i < len))
  i++;
  j = 0;
 while (i < len) {
  if (isspace(from[i])) {
   while (isspace(from[i]))
	i++;
	to[j++] = '_';
   }
  if (from[i] == '/') {
   i++;
   continue;
  }
  to[j++] = from[i++];
 }
 to[j] = '\0';
}

char **linux_edev_get_ata_identity (char **args, char *devicefile) {
 int fd;
 struct hd_driveid ata_ident;
 char sn[21];
 char rev[9];
 char mod[41];
 char **ata_type = NULL, *atatype = NULL;

 fd = open(devicefile, O_RDONLY|O_NONBLOCK);
 if (fd < 0) {
  perror ("opening device file (get_ata_identity)");
  close(fd);
  return args;
 }

 if (ioctl(fd, HDIO_GET_IDENTITY, &ata_ident)) {
  perror ("ioctl/HDIO_GET_IDENTITY");
  close(fd);
  return args;
 }

 set_str(sn, (char *) ata_ident.serial_no, 20);
 set_str(rev, (char *) ata_ident.fw_rev, 8);
 set_str(mod, (char *) ata_ident.model, 40);

 if ((ata_ident.config >> 8) & 0x80) {
  switch ((ata_ident.config >> 8) & 0x1f) {
   case 0:
    ata_type = set_str_add (ata_type, "CDROM");
    break;
   case 1:
    ata_type = set_str_add (ata_type, "TAPE");
    break;
   case 5:
    ata_type = set_str_add (ata_type, "CDROM");
    break;
   case 7:
    ata_type = set_str_add (ata_type, "OPTICAL");
    break;
   default:
    ata_type = set_str_add (ata_type, "GENERIC");
    break;
  }
 } else {
  ata_type = set_str_add (ata_type, "DISK");
 }

 close(fd);

// ATA_MODEL=mod
 args = set_str_add (args, "ATA_MODEL");
 args = set_str_add (args, mod);

// ATA_SERIAL=sn
 args = set_str_add (args, "ATA_SERIAL");
 args = set_str_add (args, sn);

// ATA_REVISION=rev
 args = set_str_add (args, "ATA_REVISION");
 args = set_str_add (args, rev);

 atatype = set2str (':', (const char **)ata_type);
 efree (ata_type);

 args = set_str_add (args, "ATA_TYPE");
 args = set_str_add (args, atatype);
// notice (5, "ATA_TYPE: %s", atatype);
 efree (atatype);

#if 0
 atatype = set2str (',', (const char **)args);
 fprintf (stderr, " ** args: %s\n", atatype);
// fflush (stderr);
 efree (atatype);
#endif

 return args;
}

char *linux_edev_mangle_filename (char *filename, char do_free) {
 if (strstr (filename, "${NUM+}")) {
  char *new_filename = NULL;
  char **temp_environment = (char **)set_noa_add (NULL, "NUM+");
  char buffer[32]; // no file will get more than a 32-digit suffix num... no, really
  int num = 0;
  struct stat st;

  temp_environment = (char **)set_noa_add ((void **)temp_environment, buffer);

  do {
   esprintf (buffer, 32, "%i", num);
   num++;

   new_filename = apply_variables (filename, (const char **)temp_environment);

/* see if we can stat the file... if we can, it exists, and lstat returns 0, and we increased the num by one for the next run */
  } while (!lstat (new_filename, &st));

  if (do_free) efree (filename);
  efree (temp_environment);

  return (new_filename);
 } else
  return filename;
}

#if 0
int **linux_edev_socket_relay(char **args, const char *sockn, const char *dp, const char *a)
{
 char buffer[2048];
 struct sockaddr_un s_addy;
 socklen_t addy_len;
 size_t pbuf = 0;
 int i, s;
 ssize_t c;
 s = socket(AF_LOCAL, SOCK_DGRAM, 0);
 memset(&s_addy, 0x00, sizeof(struct sockaddr_un));
 s_addy.sun_family = AF_LOCAL;
 strcpy(&s_addy.sun_path[1], sockn);
 addy_len = offsetof(struct sockaddr_un, sun_path) + strlen(s_addy.sun_path+1) + 1;
 pbuf = snprintf(buffer, sizeof(buffer)-1, "%s@%s", a, dp);
 pbuf++;
 for (i = 0; args[i] != NULL && pbuf < (sizeof(buffer)-1); i++) {
  pbuf = strlcpy(&buffer[pbuf], args[i], sizeof(buffer) - pbuf-1);
  pbuf++;
 }
 if (pbuf > sizeof(buffer))
  pbuf = sizeof(buffer);
  c = sendto(s, &buffer, pbuf, 0, (struct sockaddr *)&s_addy, addy_len);
 if (c < 0) {
  return status_failed;
 } else {
  notice(5,"passed %zi bytes to socket '%s', ", c, sockn);
 }
 close(s);
 return status_ok; 
}
#endif

int linux_edev_cleanup (struct lmodule *pa) {
 exec_cleanup(pa);

 event_ignore (einit_boot_early, linux_edev_boot_event_handler);
 event_ignore (einit_boot_initramfs, linux_edev_initramfs_event_handler);
 event_ignore (einit_core_configuration_update, linux_edev_retrieve_rules);
 event_ignore (einit_power_down_scheduled, linux_edev_shutdown);
 event_ignore (einit_power_reset_scheduled, linux_edev_shutdown);

 return 0;
}

int linux_edev_configure (struct lmodule *pa) {
 module_init (pa);

 char *dm = cfg_getstring("configuration-system-device-manager", NULL);

 if (!dm || strcmp (dm, "edev")) {
  return status_configure_failed | status_not_in_use;
 }

 exec_configure(pa);

 pa->cleanup = linux_edev_cleanup;

 event_listen (einit_core_configuration_update, linux_edev_retrieve_rules);
 event_listen (einit_boot_early, linux_edev_boot_event_handler);
 event_listen (einit_boot_initramfs, linux_edev_initramfs_event_handler);
 event_listen (einit_power_down_scheduled, linux_edev_shutdown);
 event_listen (einit_power_reset_scheduled, linux_edev_shutdown);

 return 0;
}
