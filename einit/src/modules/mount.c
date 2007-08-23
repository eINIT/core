/*
 *  mount.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 07/06/2006.
 *  Renamed from common-mount.c on 11/10/2006.
 *  Redesigned on 12/05/2007
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
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

#include <einit-modules/mount.h>

#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/bitch.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#include <sys/param.h>
#include <sys/mount.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>

#include <einit-modules/process.h>
#include <einit-modules/exec.h>

#ifdef LINUX
#include <linux/fs.h>

#ifndef MNT_DETACH
/* this somehow doesn't always get defined... *shrugs* */
#define MNT_DETACH   0x00000002
#endif

#endif

#ifdef POSIXREGEX
#include <regex.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_mount_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
/* module definitions */
const struct smodule einit_mount_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Filesystem-Mounter",
 .rid       = "einit-mount",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_mount_configure
};

module_register(einit_mount_self);

#endif

/* new mounter code */

int emount (char *, struct einit_event *);
int eumount (char *, struct einit_event *);
int mount_mount (char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_umount (char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_do_mount_generic (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_do_umount_generic (char *, char *, char, struct device_data *, struct mountpoint_data *, struct einit_event *);

int einit_mount_recover (struct lmodule *);
int einit_mount_recover_module (struct lmodule *);

void einit_mount_mount_ipc_handler(struct einit_event *);
void einit_mount_mount_handler(struct einit_event *);
void einit_mount_einit_event_handler(struct einit_event *);
int einit_mount_scanmodules (struct lmodule *);
int einit_mount_cleanup (struct lmodule *);
void einit_mount_update_configuration ();
unsigned char read_filesystem_flags_from_configuration (void *);
void mount_add_filesystem (char *, char *);
char *options_string_to_mountflags (char **, unsigned long *, char *);
enum filesystem_capability mount_get_filesystem_options (char *);
char **mount_generate_mount_function_suffixes (char *);
int mount_fsck (char *, char *, struct einit_event *);

struct device_data **mounter_device_data = NULL;
struct stree *mounter_dd_by_mountpoint = NULL;
struct stree *mounter_dd_by_devicefile = NULL;

char **mount_dont_umount = NULL;
char **mount_critical = NULL;
char **mount_system = NULL;
char *mount_fsck_template = NULL;
char *mount_mtab_file = NULL;
enum mount_options mount_options;

extern char shutting_down;

pthread_mutex_t
 mount_fs_mutex = PTHREAD_MUTEX_INITIALIZER,
 mount_device_data_mutex = PTHREAD_MUTEX_INITIALIZER,
 mounter_dd_by_devicefile_mutex = PTHREAD_MUTEX_INITIALIZER,
 mounter_dd_by_mountpoint_mutex = PTHREAD_MUTEX_INITIALIZER;

struct stree *mount_filesystems = NULL;

char *generate_legacy_mtab ();
char mount_fastboot = 0;

/* macro definitions */
#define update_real_mtab() {\
 if (mount_mtab_file) {\
  char *tmpmtab = generate_legacy_mtab ();\
\
  if (tmpmtab) {\
   unlink (mount_mtab_file);\
\
   FILE *mtabfile = efopen (mount_mtab_file, "w");\
\
   if (mtabfile) {\
    eputs (tmpmtab, mtabfile);\
    efclose (mtabfile);\
}\
\
   free (tmpmtab);\
}\
}\
}

char *generate_legacy_mtab () {
 char *ret = NULL;
 ssize_t retlen = 0;

 struct device_data *dd = NULL;
 struct stree *t;

 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 t = mounter_dd_by_mountpoint;

 while (t) {
  dd = t->value;

  if (dd) {
   struct stree *st = streefind (dd->mountpoints, t->key, tree_find_first);

   if (st) {
    struct mountpoint_data *mp = st->value;

    if (mp && (mp->status & device_status_mounted)) {
     char tmp[BUFFERSIZE];
     char *tset = set2str (',', (const char **)mp->options); 

     if (tset)
      esprintf (tmp, BUFFERSIZE, "%s %s %s %s,%s 0 0\n", dd->device, mp->mountpoint, mp->fs,
#ifdef MS_RDONLY
                mp->mountflags & MS_RDONLY
#else
                  0
#endif
                  ? "ro" : "rw", tset);
     else
      esprintf (tmp, BUFFERSIZE, "%s %s %s %s 0 0\n", dd->device, mp->mountpoint, mp->fs,
#ifdef MS_RDONLY
                mp->mountflags & MS_RDONLY
#else
                  0
#endif
                  ? "ro" : "rw");

     ssize_t nlen = strlen(tmp);

     if (retlen == 0) {
      ret = emalloc (nlen +1);
      *ret = 0;

      retlen += 1;
     } else {
      ret = erealloc (ret, retlen + nlen);
     }

     retlen += nlen;

     strcat (ret, tmp);

     if (tset) free (tset);
    }
   }
  }

  t = streenext(t);
 }

 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 return ret;
}

unsigned char read_filesystem_flags_from_configuration (void *na) {
 struct cfgnode *node = NULL;
 uint32_t i;
 char *id, *flags;
 while ((node = cfg_findnode ("information-filesystem-type", 0, node))) {
  if (node->arbattrs) {
   id = NULL;
   flags = 0;
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (strmatch (node->arbattrs[i], "id"))
     id = node->arbattrs[i+1];
    else if (strmatch (node->arbattrs[i], "flags"))
     flags = node->arbattrs[i+1];
   }
   mount_add_filesystem (id, flags);
  }
 }
 return 0;
}

void mount_add_filesystem (char *name, char *options) {
 char **t = str2set (':', options);
 uintptr_t flags = 0, i = 0;
 if (t) {
  for (; t[i]; i++) {
   if (strmatch (t[i], "rw"))
    flags |= filesystem_capability_rw;
   else if (strmatch (t[i], "volatile"))
    flags |= filesystem_capability_volatile;
   else if (strmatch (t[i], "network"))
    flags |= filesystem_capability_network;
  }
  free (t);
 }

 emutex_lock (&mount_fs_mutex);
 if (mount_filesystems && streefind (mount_filesystems, name, tree_find_first)) {
  emutex_unlock (&mount_fs_mutex);
  return;
 }

 mount_filesystems = streeadd (mount_filesystems, name, (void *)flags, -1, NULL);
 emutex_unlock (&mount_fs_mutex);
}

enum filesystem_capability mount_get_filesystem_options (char *name) {
 enum filesystem_capability ret = filesystem_capability_rw;
 struct stree *t;

 emutex_lock (&mount_fs_mutex);
 if ((t = streefind (mount_filesystems, name, tree_find_first))) {
  ret = (enum filesystem_capability)t->value;
 }
 emutex_unlock (&mount_fs_mutex);

 return ret;
}

char **mount_get_device_files () {
 struct cfgnode *node = cfg_getnode ("configuration-storage-block-devices-constraints", NULL);

 if (node) {
  char **devices = readdirfilter(node, "/dev/", NULL, NULL, 1);

  if (devices) {
   uint32_t i = 0;

   for (; devices[i]; i++) {
    struct stat sbuf;

    if (stat (devices[i], &sbuf) || !S_ISBLK (sbuf.st_mode)) {
     devices = (char **)setdel ((void **)devices, (void *)devices[i]);
    }
   }
  }

  return devices;
 }

 return NULL;
}

char **mount_get_mounted_mountpoints () {
 struct device_data *dd = NULL;
 struct stree *t;
 char **rv = NULL;

 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 t = mounter_dd_by_mountpoint;

 while (t) {
  dd = t->value;

  if (dd) {
   struct stree *st = streefind (dd->mountpoints, t->key, tree_find_first);

   if (st) {
    struct mountpoint_data *mp = st->value;

    if (mp && (mp->status & device_status_mounted)) {
     rv = (char **)setadd((void **)rv, (char *)t->key, SET_TYPE_STRING);
    }
   }
  }

  t = streenext(t);
 }

 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 return rv;
}

void mount_clear_all_mounted_flags () {
 struct device_data *dd = NULL;
 struct stree *t;

 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 t = mounter_dd_by_mountpoint;

 while (t) {
  dd = t->value;

  if (dd) {
   struct stree *st = streefind (dd->mountpoints, t->key, tree_find_first);

   if (st) {
    struct mountpoint_data *mp = st->value;

    if (mp && (mp->status & device_status_mounted)) {
     mp->status ^= device_status_mounted;
    }
   }
  }

  t = streenext(t);
 }

 emutex_unlock (&mounter_dd_by_mountpoint_mutex);
}

void mount_update_device (struct device_data *d) {
}

void mount_add_update_fstab_data (struct device_data *dd, char *mountpoint, char *fs, char **options, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, char **variables, uint32_t mountflags) {
 struct stree *st = (dd->mountpoints ? streefind (dd->mountpoints, mountpoint, tree_find_first) : NULL);
 struct mountpoint_data *mp = st ? st->value : ecalloc (1, sizeof (struct mountpoint_data));
// char *device = dd->device;

 mp->mountpoint = mountpoint;
 mp->fs = fs ? fs : estrdup("auto");
 mp->options = options;
 mp->before_mount = before_mount;
 mp->after_mount = after_mount;
 mp->before_umount = before_umount;
 mp->after_umount = after_umount;
 if (manager) {
  struct dexecinfo *dx = emalloc (sizeof (struct dexecinfo));
  memset (dx, 0, sizeof (struct dexecinfo));

  mp->manager = dx;
  dx->command = manager;
  dx->variables = variables;
  dx->restart = 1;
 }
 mp->variables = variables;
 mp->mountflags = mountflags;

 mp->flatoptions = options_string_to_mountflags (mp->options, &(mp->mountflags), mountpoint);

 struct stree *t = NULL;
 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 if (mounter_dd_by_mountpoint && (t = streefind (mounter_dd_by_mountpoint, mountpoint, tree_find_first))) {
  t->value = dd;
 } else mounter_dd_by_mountpoint = streeadd (mounter_dd_by_mountpoint, mountpoint, dd, SET_NOALLOC, NULL);
 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 if (!st) {
/*  eprintf (stderr, " >> have mountpoint_data node for %s, device %s, fs %s: updating\n", mountpoint, device, fs);
 } else {
  eprintf (stderr, " >> inserting new mountpoint_data node for %s, device %s, fs %s\n", mountpoint, device, fs);*/

  dd->mountpoints = streeadd (dd->mountpoints, mountpoint, mp, SET_NOALLOC, mp);
 }
}

void mount_add_update_fstab (char *mountpoint, char *device, char *fs, char **options, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, char **variables, uint32_t mountflags) {
 struct device_data *dd = NULL;
 struct stree *t;

 if (!fs) fs = estrdup ("auto");

/* emutex_lock (&mounter_dd_by_mountpoint_mutex);
 if (mounter_dd_by_mountpoint && (t = streefind (mounter_dd_by_mountpoint, mountpoint, tree_find_first))) {
  dd = t->value;
 }
 emutex_unlock (&mounter_dd_by_mountpoint_mutex);*/

 if (!dd && (device || (device = fs) || (device = "(none)"))) {
  emutex_lock (&mounter_dd_by_devicefile_mutex);
  if (mounter_dd_by_devicefile && (t = streefind (mounter_dd_by_devicefile, device, tree_find_first))) {
   dd = t->value;
  }
  emutex_unlock (&mounter_dd_by_devicefile_mutex);
 }

 if (dd) {
  mount_add_update_fstab_data (dd, mountpoint, fs, options, before_mount, after_mount, before_umount, after_umount, manager, variables, mountflags);
 } else {
  struct device_data *d = emalloc(sizeof(struct device_data));
  uint32_t y = 0;

  memset (d, 0, sizeof(struct device_data));

  if (device || (device = fs) || (device = "(none)")) d->device = estrdup (device);

//  eprintf (stderr, " >> inserting new device_data node for %s, device %s\n", mountpoint, device);

  d->device_status = device_status_has_medium | device_status_error_notint;

  mounter_device_data = (struct device_data **)setadd ((void **)mounter_device_data, (void *)d, SET_NOALLOC);

  for (y = 0; mounter_device_data[y]; y++);
  if (y > 0) y--;

  emutex_lock (&mounter_dd_by_devicefile_mutex);
  mounter_dd_by_devicefile =
   streeadd (mounter_dd_by_devicefile, d->device, mounter_device_data[y], SET_NOALLOC, NULL);
  emutex_unlock (&mounter_dd_by_devicefile_mutex);

  mount_add_update_fstab_data (d, mountpoint, fs, options, before_mount, after_mount, before_umount, after_umount, manager, variables, mountflags);
 }
}

struct stree *read_fsspec_file (char *file) {
 struct stree *workstree = NULL;
 FILE *fp;
 if (!file) return NULL;

 if ((fp = efopen (file, "r"))) {
  char buffer[BUFFERSIZE];
  errno = 0;
  while (!errno) {
   if (!fgets (buffer, BUFFERSIZE, fp)) {
    switch (errno) {
     case EINTR:
     case EAGAIN:
      errno = 0;
      break;
     case 0:
      goto done_parsing_file;
     default:
      bitch(bitch_stdio, 0, "fgets() failed.");
      goto done_parsing_file;
    }
   } else if (buffer[0] != '#') {
    strtrim (buffer);
    if (buffer[0]) {
     char *cur = estrdup (buffer);
     char *scur = cur;
     char *ascur = cur;
     uint32_t icur = 0;
     struct legacy_fstab_entry ne;
     memset (&ne, 0, sizeof (struct legacy_fstab_entry));

     strtrim (cur);
     for (; *cur; cur++) {
      if (isspace (*cur)) {
       *cur = 0;
       icur++;
       switch (icur) {
        case 1: ne.fs_spec = scur; break;
        case 2: ne.fs_file = scur; break;
        case 3: ne.fs_vfstype = scur; break;
        case 4: ne.fs_mntops = scur; break;
        case 5: ne.fs_freq = (int) strtol(scur, (char **)NULL, 10); break;
        case 6: ne.fs_passno = (int) strtol(scur, (char **)NULL, 10); break;
       }
       scur = cur+1;
       strtrim (scur);
      }
     }
     if (cur != scur) {
      icur++;
      switch (icur) {
       case 1: ne.fs_spec = scur; break;
       case 2: ne.fs_file = scur; break;
       case 3: ne.fs_vfstype = scur; break;
       case 4: ne.fs_mntops = scur; break;
       case 5: ne.fs_freq = (int) strtol(scur, (char **)NULL, 10); break;
       case 6: ne.fs_passno = (int) strtol(scur, (char **)NULL, 10); break;
      }
     }
     workstree = streeadd (workstree, ne.fs_file, &ne, sizeof (struct legacy_fstab_entry), ascur);
//     workstree = streeadd (workstree, ne->fs_file, ne, -1);
    }
   }
  }
  done_parsing_file:
     efclose (fp);
 }

 return workstree;
}

void mount_update_fstab_nodes () {
 struct cfgnode *node = NULL;
 uint32_t i;
 while ((node = cfg_findnode ("configuration-storage-fstab-node", 0, node))) {
  char *mountpoint = NULL,
  *device = NULL,
  *fs = NULL,
  **options = NULL,
  *before_mount = NULL,
  *after_mount = NULL,
  *before_umount = NULL,
  *after_umount = NULL,
  *manager = NULL,
  **variables = NULL;
  uint32_t mountflags = 0;

  if (node->arbattrs) {
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (strmatch(node->arbattrs[i], "mountpoint"))
     mountpoint = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "device"))
     device = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "fs"))
     fs = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "options"))
     options = str2set (':', node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "before-mount"))
     before_mount = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "after-mount"))
     after_mount = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "before-umount"))
     before_umount = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "after-umount"))
     after_umount = estrdup (node->arbattrs[i+1]);
    else if (strmatch(node->arbattrs[i], "manager")) {
     manager = estrdup (node->arbattrs[i+1]);
    } else if (strmatch(node->arbattrs[i], "variables"))
     variables = str2set (':', node->arbattrs[i+1]);

    else if (strmatch(node->arbattrs[i], "label")) {
     char tmp[BUFFERSIZE];

     esprintf (tmp, BUFFERSIZE, "/dev/disk/by-label/%s", node->arbattrs[i+1]);
     if (device) free (device);
     device = estrdup(tmp);
    } else if (strmatch(node->arbattrs[i], "uuid")) {
     char tmp[BUFFERSIZE];

     esprintf (tmp, BUFFERSIZE, "/dev/disk/by-uuid/%s", node->arbattrs[i+1]);
     if (device) free (device);
     device = estrdup(tmp);
    }
   }

   if (mountpoint) mount_add_update_fstab (mountpoint, device, fs, options, before_mount, after_mount, before_umount, after_umount, manager, variables, mountflags);

//   add_fstab_entry (mountpoint, device, fs, options, mountflags, before_mount, after_mount, before_umount, after_umount, manager, 1, variables);
  }
 }
}

void mount_update_fstab_nodes_from_fstab () {
 struct cfgnode *node = cfg_getnode ("configuration-storage-fstab-use-legacy-fstab", NULL);
 if (node && node->flag) {
  struct stree *workstree = read_fsspec_file ("/etc/fstab");
  struct stree *cur = workstree;

  if (workstree) {
   mount_clear_all_mounted_flags();

   while (cur) {
    struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;

    if (val->fs_file) {
     char **options = val->fs_mntops ? str2set (',', val->fs_mntops): NULL;
     char *fs_spec = NULL;

     if (strstr (val->fs_spec, "UUID=") == val->fs_spec) {
      char tmp[BUFFERSIZE];

      esprintf (tmp, BUFFERSIZE, "/dev/disk/by-uuid/%s", val->fs_spec + 5);
      fs_spec = estrdup(tmp);
     } else if (strstr (val->fs_spec, "LABEL=") == val->fs_spec) {
      char tmp[BUFFERSIZE];

      esprintf (tmp, BUFFERSIZE, "/dev/disk/by-label/%s", val->fs_spec + 6);
      fs_spec = estrdup(tmp);
     } else {
      fs_spec = estrdup(val->fs_spec);
     }

     mount_add_update_fstab (estrdup(val->fs_file), fs_spec, estrdup(val->fs_vfstype), options, NULL, NULL, NULL, NULL, NULL, NULL, 0);
    }

    cur = streenext (cur);
   }

   streefree(workstree);
  }
  return;
 }
}

void mount_update_nodes_from_mtab () {
#ifdef LINUX
 struct stree *workstree = read_fsspec_file ("/proc/mounts");
#else
 struct stree *workstree = read_fsspec_file ("/etc/mtab");
#endif
 struct stree *cur = workstree;

 if (workstree) {
  mount_clear_all_mounted_flags();

  while (cur) {
   struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;
//   add_mtab_entry (val->fs_spec, val->fs_file, val->fs_vfstype, val->fs_mntops, val->fs_freq, val->fs_passno);

// void mount_add_update_fstab (char *mountpoint, char *device, char *fs, char **options, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, char **variables, uint32_t mountflags) {

   if (val->fs_file) {
    struct device_data *dd = NULL;
    struct stree *t;
    char **options = val->fs_mntops ? str2set (',', val->fs_mntops): NULL;

    mount_add_update_fstab (estrdup(val->fs_file), estrdup(val->fs_spec), estrdup(val->fs_vfstype), options, NULL, NULL, NULL, NULL, NULL, NULL, 0);

    emutex_lock (&mounter_dd_by_mountpoint_mutex);
    if (mounter_dd_by_mountpoint && (t = streefind (mounter_dd_by_mountpoint, val->fs_file, tree_find_first))) {
     dd = t->value;
    }
    emutex_unlock (&mounter_dd_by_mountpoint_mutex);

    if (dd) {
     struct stree *st = streefind (dd->mountpoints, val->fs_file, tree_find_first);

     if (st) {
      struct mountpoint_data *mp = st->value;

      if (mp) {
       mp->status |= device_status_mounted;
      }
     }
	}
   }

   cur = streenext (cur);
  }

  streefree(workstree);
 }
 return;
}

void mount_update_devices () {
 uint32_t i = 0;

 char **devices = mount_get_device_files();

 emutex_lock (&mount_device_data_mutex);

 if (mounter_device_data) {
  for (; mounter_device_data[i]; i++) {
   if (devices)
    devices = strsetdel (devices, mounter_device_data[i]->device);
  }
 }

 if (devices) {
  for (i = 0; devices[i]; i++) {
   struct device_data *d = emalloc(sizeof(struct device_data));
   uint32_t y = 0;

   memset (d, 0, sizeof(struct device_data));
   d->device = estrdup (devices[i]);
   d->device_status = device_status_has_medium | device_status_error_notint;

   mounter_device_data = (struct device_data **)setadd ((void **)mounter_device_data, (void *)d, SET_NOALLOC);

   for (y = 0; mounter_device_data[y]; y++);
   if (y > 0) y--;

   emutex_lock (&mounter_dd_by_devicefile_mutex);
   mounter_dd_by_devicefile =
    streeadd (mounter_dd_by_devicefile, devices[i], mounter_device_data[y], SET_NOALLOC, NULL);
   emutex_unlock (&mounter_dd_by_devicefile_mutex);
  }

  free (devices);
 }

 if (mounter_device_data) {
  for (i = 0; mounter_device_data[i]; i++) {
   mount_update_device (mounter_device_data[i]);
  }
 }

 mount_update_fstab_nodes_from_fstab ();
 mount_update_fstab_nodes ();

 mount_update_nodes_from_mtab ();

 emutex_unlock (&mount_device_data_mutex);
}

void einit_mount_mount_ipc_handler(struct einit_event *ev) {
}

void einit_mount_mount_handler(struct einit_event *ev) {
}

void einit_mount_einit_event_handler(struct einit_event *ev) {
 if ((ev->type == einit_core_configuration_update) || (ev->type == einit_core_module_list_update_complete)) {
  einit_mount_update_configuration();
 }
}

char *mount_mp_to_service_name (char *mp) {
 if (strmatch (mp, "/"))
  return estrdup ("fs-root");
 else {
  char *tmp = emalloc (4+strlen (mp));
  uint32_t i = 0, j = 3;

  tmp[0] = 'f';
  tmp[1] = 's';
  tmp[2] = '-';

  for (; mp[i]; i++) {
   if ((mp[i] == '/') && (i == 0)) continue;
   if (mp[i] == '/') {
    tmp[j] = '-';
   } else {
    tmp[j] = mp[i];
   }
   j++;
  }

  tmp[j] = 0;
  j--;
  for (; tmp[j] == '-'; j--) {
   tmp[j] = 0;
  }

  return tmp;
 }

 return NULL;
}

int einit_mountpoint_configure (struct lmodule *tm) {
 tm->enable = (int (*)(void *, struct einit_event *))emount;
 tm->disable = (int (*)(void *, struct einit_event *))eumount;

 tm->recover = einit_mount_recover_module;

 tm->param = tm->module->rid + 6;

 tm->source = estrdup(tm->module->rid);

 return 0;
}

void mount_add_update_group (char *groupname, char **elements, char *seq) {
 struct cfgnode newnode;
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "services-alias-%s", groupname);

 struct cfgnode *onode = cfg_getnode (tmp, NULL);
 if (onode) {
  char *jele = NULL;
  if (!onode->source || !strmatch (onode->source, self->rid)) {
   uint32_t i = 0;

   for (; onode->arbattrs[i]; i+=2) {
    if (strmatch (onode->arbattrs[i], "group")) {
     char **nele = str2set (':', onode->arbattrs[i+1]);

     nele = (char **)setcombine_nc ((void **)nele, (const void **)elements, SET_TYPE_STRING);

     jele = set2str (':', (const char **)nele);

     break;
    }
   }
  }

  if (!jele) {
   jele = set2str (':', (const char **)elements);
  }

  char **oarb = onode->arbattrs;
  char **narb = NULL;

  narb = (char **)setadd ((void **)narb, (void *)"group", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)jele, SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)"seq", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)seq, SET_TYPE_STRING);

  onode->arbattrs = narb;

  if (oarb) {
   free (oarb);
  }
  free (jele);

 } else {
  char *jele = set2str (':', (const char **)elements);
  memset (&newnode, 0, sizeof(struct cfgnode));

  newnode.id = estrdup (tmp);
  newnode.source = self->rid;
  newnode.type = einit_node_regular;

  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"group", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)jele, SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"seq", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)seq, SET_TYPE_STRING);

  cfg_addnode (&newnode);
  free (jele);
 }
}

int einit_mount_scanmodules (struct lmodule *ml) {
 struct stree *s = NULL;
 char **scritical = NULL, **ssystem = NULL, **slocal = NULL, **sremote = NULL;

 if (!mount_filesystems) return 0;

 ssystem = (char **)setadd ((void **)NULL, (void *)"fs-root", SET_TYPE_STRING);
 scritical = (char **)setadd ((void **)NULL, (void *)"mount-system", SET_TYPE_STRING);
 slocal = (char **)setadd ((void **)NULL, (void *)"mount-critical", SET_TYPE_STRING);
 sremote = (char **)setadd ((void **)NULL, (void *)"mount-critical", SET_TYPE_STRING);

 emutex_lock (&mounter_dd_by_mountpoint_mutex);

 s = mounter_dd_by_mountpoint;
 while (s) {
  char *servicename = mount_mp_to_service_name(s->key);
  char tmp[BUFFERSIZE];
  char **after = NULL;
  char **requires = NULL;
  struct lmodule *lm = ml;
  struct cfgnode *tcnode = cfg_findnode ("configuration-storage-fstab-node-order", 0, NULL);
  char special_order = 0;

  while (tcnode) {
   if (strmatch (s->key, tcnode->idattr)) {
    uint32_t n = 0;

    for (; tcnode->arbattrs[n]; n+=2) {
     if (strmatch (tcnode->arbattrs[n], "after") && tcnode->arbattrs[n+1][0]) {
      after = str2set (':', tcnode->arbattrs[n+1]);
     }
    }

    special_order = 1;
   }

   tcnode = cfg_findnode ("configuration-storage-fstab-node-order", 0, tcnode);
  }

  if (!special_order) {
   char *tmpx = NULL;
   uint32_t r = 0;
   char **tmp_split = s->key[0] == '/' ? str2set ('/', s->key+1) : str2set ('/', s->key), **tmpxt = NULL;
   struct device_data *tmpdd = s->value;

   for (r = 0; tmp_split[r]; r++);
   for (r--; tmp_split[r] && r > 0; r--) {
    tmp_split[r] = 0;
    char *comb = set2str ('-', (const char **)tmp_split);

    tmpxt = (char **)setadd ((void **)tmpxt, (void *)comb, SET_TYPE_STRING);
   }

/* same game, but with the device and not the mountpoint */
   struct stree *t = streefind (((struct device_data *)(s->value))->mountpoints, s->key, tree_find_first);
   if (t) {
    struct mountpoint_data *mp = t->value; 
    enum filesystem_capability capa = mount_get_filesystem_options (((struct device_data *)(s->value))->fs);
    if (!((capa & filesystem_capability_network) || inset ((const void **)mp->options, "network", SET_TYPE_STRING))) {
     if (tmpdd->device) {
      tmp_split = (tmpdd->device[0] == '/') ? str2set ('/', tmpdd->device+1) : str2set ('/', tmpdd->device);

      for (r = 0; tmp_split[r]; r++);
      for (r--; tmp_split[r] && r > 0; r--) {
       tmp_split[r] = 0;
       char *comb = set2str ('-', (const char **)tmp_split);

       if (!inset ((const void **)tmpxt, comb, SET_TYPE_STRING)) {
        tmpxt = (char **)setadd ((void **)tmpxt, (void *)comb, SET_TYPE_STRING);
       }
      }
     }
    }
   }

   tmpxt = (char **)setadd ((void **)tmpxt, (void *)"root", SET_TYPE_STRING);


   if (tmpxt) {
    tmpx = set2str ('|', (const char **)tmpxt);
   }

   free (tmp_split);
   free (tmpxt);

   if (tmpx) {
    esprintf (tmp, BUFFERSIZE, "^fs-(%s)$", tmpx);
    after = (char **)setadd ((void **)after, (void *)tmp, SET_TYPE_STRING);
   }
  }

/*  eprintf (stderr, "need to create module for mountpoint %s, aka service %s, with regex %s.\n", s->key, servicename, after ? after[0] : "(none)");*/

  struct smodule *newmodule = emalloc (sizeof (struct smodule));
  memset (newmodule, 0, sizeof (struct smodule));

  struct stree *t = streefind (((struct device_data *)(s->value))->mountpoints, s->key, tree_find_first);
  if (t) {
   struct mountpoint_data *mp = t->value; 

   enum filesystem_capability capa = mount_get_filesystem_options (((struct device_data *)(s->value))->fs);
   if ((capa & filesystem_capability_network) || inset ((const void **)mp->options, "network", SET_TYPE_STRING)) {
    requires = (char **)setadd ((void **)requires, (void *)"network", SET_TYPE_STRING);
    requires = (char **)setadd ((void **)requires, (void *)"portmap", SET_TYPE_STRING);
   }

   if (mp && !inset ((const void **)mp->options, "noauto", SET_TYPE_STRING)) {
    if (inset ((const void **)mount_system, s->key, SET_TYPE_STRING)) {
     ssystem = (char **)setadd ((void **)ssystem, (void *)servicename, SET_TYPE_STRING);
    } else if (inset ((const void **)mount_critical, s->key, SET_TYPE_STRING)) {
     scritical = (char **)setadd ((void **)scritical, (void *)servicename, SET_TYPE_STRING);

     requires = (char **)setadd ((void **)requires, "mount-system", SET_TYPE_STRING);
    } else {
     char ad = 0;

     if (inset ((const void **)mp->options, "critical", SET_TYPE_STRING)) {
      scritical = (char **)setadd ((void **)scritical, (void *)servicename, SET_TYPE_STRING);
      ad = 1;

      requires = (char **)setadd ((void **)requires, "mount-system", SET_TYPE_STRING);
     }

     if (inset ((const void **)mp->options, "system", SET_TYPE_STRING)) {
      ssystem = (char **)setadd ((void **)ssystem, (void *)servicename, SET_TYPE_STRING);
      ad = 1;
     }

     if (!ad) {
      enum filesystem_capability capa = mount_get_filesystem_options (((struct device_data *)(s->value))->fs);

      if ((capa & filesystem_capability_network) || inset ((const void **)mp->options, "network", SET_TYPE_STRING)) {
       sremote = (char **)setadd ((void **)sremote, (void *)servicename, SET_TYPE_STRING);
       ad = 1;

       requires = (char **)setadd ((void **)requires, "mount-system", SET_TYPE_STRING);
      }
     }

     if (!ad) {
      slocal = (char **)setadd ((void **)slocal, (void *)servicename, SET_TYPE_STRING);
      requires = (char **)setadd ((void **)requires, "mount-system", SET_TYPE_STRING);
     }
    }
   }
  }

  esprintf (tmp, BUFFERSIZE, "mount-%s", s->key);

  if (inset ((const void **)ssystem, servicename, SET_TYPE_STRING)) {
//   notice (1, "%s is in ssystem (%s), not making it require that", servicename, set2str (' ', ssystem));

   requires = strsetdel (requires, "mount-system");
  }

  while (lm) {
   if (lm->source && strmatch(lm->source, tmp)) {
    struct smodule *sm = (struct smodule *)lm->module;
    sm->si.after = after;
    sm->si.requires = requires;

    lm = mod_update (lm);

    goto do_next;
   }

   lm = lm->next;
  }

  newmodule->configure = einit_mountpoint_configure;
  newmodule->eiversion = EINIT_VERSION;
  newmodule->eibuild = BUILDNUMBER;
  newmodule->version = 1;
  newmodule->mode = einit_module_generic;

//  esprintf (tmp, BUFFERSIZE, "mount-%s", s->key);
  newmodule->rid = estrdup (tmp);

  newmodule->si.provides = (char **)setadd ((void **)newmodule->si.provides, (void *)servicename, SET_TYPE_STRING);

  esprintf (tmp, BUFFERSIZE, "Filesystem ( %s )", s->key);
  newmodule->name = estrdup (tmp);

  newmodule->si.after = after;

  newmodule->si.requires = requires;

  lm = mod_add (NULL, newmodule);

  do_next:

  s = s->next;
 }

 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 if (ssystem) mount_add_update_group ("mount-system", ssystem, "most");
 if (scritical) mount_add_update_group ("mount-critical", scritical, "most");
 if (slocal) mount_add_update_group ("mount-local", slocal, "most");
 if (sremote) mount_add_update_group ("mount-remote", sremote, "most");

 return 0;
}

struct device_data *mount_get_device_data (char *mountpoint, char *devicefile) {
 struct device_data *dd = NULL;

 if (mountpoint) {
  emutex_lock (&mounter_dd_by_mountpoint_mutex);
  if (mounter_dd_by_mountpoint) {
   struct stree *t;

   if ((t = streefind (mounter_dd_by_mountpoint, mountpoint, tree_find_first)))
    dd = t->value;
  }
  emutex_unlock (&mounter_dd_by_mountpoint_mutex);
 }

 if (!dd && devicefile) {
  emutex_lock (&mounter_dd_by_devicefile_mutex);
  if (mounter_dd_by_devicefile) {
   struct stree *t;

   if ((t = streefind (mounter_dd_by_devicefile, devicefile, tree_find_first)))
    dd = t->value;
  }
  emutex_unlock (&mounter_dd_by_devicefile_mutex);
 }

 return dd;
}

char *options_string_to_mountflags (char **options, unsigned long *mntflags, char *mountpoint) {
 int fi = 0;
 char *ret = NULL;

 if (!options) return NULL;

 for (; options[fi]; fi++) {
#ifdef LINUX
  if (strmatch (options[fi], "user") || strmatch (options[fi], "users")) {
//   notice (6, "node \"%s\": mount-flag \"%s\": this has no real meaning for eINIT except for implying noexec, nosuid and nodev; you should remove it.\n", mountpoint, options[fi]);

#ifdef MS_NOEXEC
   (*mntflags) |= MS_NOEXEC;
#endif
#ifdef MS_NODEV
   (*mntflags) |= MS_NODEV;
#endif
#ifdef MS_NOSUID
   (*mntflags) |= MS_NOSUID;
#endif
  } else if (strmatch (options[fi], "owner")) {
//   notice (6, "node \"%s\": mount-flag \"%s\": this has no real meaning for eINIT except for implying nosuid and nodev; you should remove it.\n", mountpoint, options[fi]);

#ifdef MS_NODEV
   (*mntflags) |= MS_NODEV;
#endif
#ifdef MS_NOSUID
   (*mntflags) |= MS_NOSUID;
#endif
  }/* else if (strmatch (options[fi], "nouser") || strmatch (options[fi], "group") || strmatch (options[fi], "auto") || strmatch (options[fi], "defaults")) {
  notice (6, "node \"%s\": ignored unsupported/irrelevant mount-flag \"%s\": it has no meaning for eINIT, you should remove it.\n", mountpoint, options[fi]);
 } else*/ if (strmatch (options[fi], "_netdev")) {
           notice (6, "node \"%s\": ignored unsupported/irrelevant mount-flag \"_netdev\": einit uses a table with filesystem data to find out if network access is required to mount a certain node, so you should rather modify that table than specify \"_netdev\".\n", mountpoint);
 } else
#endif

#ifdef MS_NOATIME
  if (strmatch (options[fi], "noatime")) (*mntflags) |= MS_NOATIME;
  else if (strmatch (options[fi], "atime")) (*mntflags) = ((*mntflags) & MS_NOATIME) ? (*mntflags) ^ MS_NOATIME : (*mntflags);
  else
#endif

#ifdef MS_NODEV
     if (strmatch (options[fi], "nodev")) (*mntflags) |= MS_NODEV;
  else if (strmatch (options[fi], "dev")) (*mntflags) = ((*mntflags) & MS_NODEV) ? (*mntflags) ^ MS_NODEV : (*mntflags);
  else
#endif

#ifdef MS_NODIRATIME
   if (strmatch (options[fi], "nodiratime")) (*mntflags) |= MS_NODIRATIME;
  else if (strmatch (options[fi], "diratime")) (*mntflags) = ((*mntflags) & MS_NODIRATIME) ? (*mntflags) ^ MS_NODIRATIME : (*mntflags);
  else
#endif

#ifdef MS_NOEXEC
   if (strmatch (options[fi], "noexec")) (*mntflags) |= MS_NOEXEC;
  else if (strmatch (options[fi], "exec")) (*mntflags) = ((*mntflags) & MS_NOEXEC) ? (*mntflags) ^ MS_NOEXEC : (*mntflags);
  else
#endif

#ifdef MS_NOSUID
   if (strmatch (options[fi], "nosuid")) (*mntflags) |= MS_NOSUID;
  else if (strmatch (options[fi], "suid")) (*mntflags) = ((*mntflags) & MS_NOSUID) ? (*mntflags) ^ MS_NOSUID : (*mntflags);
  else
#endif

#ifdef MS_DIRSYNC
   if (strmatch (options[fi], "dirsync")) (*mntflags) |= MS_DIRSYNC;
  else if (strmatch (options[fi], "nodirsync")) (*mntflags) = ((*mntflags) & MS_DIRSYNC) ? (*mntflags) ^ MS_DIRSYNC : (*mntflags);
  else
#endif

#ifdef MS_SYNCHRONOUS
   if (strmatch (options[fi], "sync")) (*mntflags) |= MS_SYNCHRONOUS;
  else if (strmatch (options[fi], "nosync")) (*mntflags) = ((*mntflags) & MS_SYNCHRONOUS) ? (*mntflags) ^ MS_SYNCHRONOUS : (*mntflags);
  else
#endif

#ifdef MS_MANDLOCK
   if (strmatch (options[fi], "mand")) (*mntflags) |= MS_MANDLOCK;
  else if (strmatch (options[fi], "nomand")) (*mntflags) = ((*mntflags) & MS_MANDLOCK) ? (*mntflags) ^ MS_MANDLOCK : (*mntflags);
  else
#endif

#ifdef MS_RDONLY
   if (strmatch (options[fi], "ro")) (*mntflags) |= MS_RDONLY;
  else if (strmatch (options[fi], "rw")) (*mntflags) = ((*mntflags) & MS_RDONLY) ? (*mntflags) ^ MS_RDONLY : (*mntflags);
  else
#endif

#ifdef MS_BIND
   if (strmatch (options[fi], "bind")) (*mntflags) |= MS_BIND;
  else
#endif

#ifdef MS_REMOUNT
   if (strmatch (options[fi], "remount")) (*mntflags) |= MS_REMOUNT;
  else
#endif
   if (strmatch (options[fi], "system") || strmatch (options[fi], "critical") || strmatch (options[fi], "network") || strmatch (options[fi], "skip-fsck")) ; // ignore our own specifiers
  else

   if (!ret) {
   uint32_t slen = strlen (options[fi])+1;
   ret = ecalloc (1, slen);
   memcpy (ret, options[fi], slen);
   } else {
    uint32_t fsdl = strlen(ret) +1, slen = strlen (options[fi])+1;
    ret = erealloc (ret, fsdl+slen);
    *(ret + fsdl -1) = ',';
    memcpy (ret+fsdl, options[fi], slen);
   }
 }

 return ret;
}

char **mount_generate_mount_function_suffixes (char *fs) {
 char **ret = NULL;
 char tmp[BUFFERSIZE];

#ifdef LINUX
 esprintf (tmp, BUFFERSIZE, "linux-%s", fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
#endif
 esprintf (tmp, BUFFERSIZE, "%s-%s", osinfo.sysname, fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
 esprintf (tmp, BUFFERSIZE, "generic-%s", fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);

#ifdef LINUX
 ret = (char **)setadd ((void **)ret, "linux-any", SET_TYPE_STRING);
#endif
 esprintf (tmp, BUFFERSIZE, "%s-any", osinfo.sysname);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
 ret = (char **)setadd ((void **)ret, "generic-any", SET_TYPE_STRING);

/* and we also need backups, of course */
/* NOTE: the *-backup functions are used when shit goes weird. right now that
         'give the system's native mount-command a chance to mess with this */
#ifdef LINUX
 esprintf (tmp, BUFFERSIZE, "linux-%s-backup", fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
#endif
 esprintf (tmp, BUFFERSIZE, "%s-%s-backup", osinfo.sysname, fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
 esprintf (tmp, BUFFERSIZE, "generic-%s-backup", fs);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);

#ifdef LINUX
 ret = (char **)setadd ((void **)ret, "linux-any-backup", SET_TYPE_STRING);
#endif
 esprintf (tmp, BUFFERSIZE, "%s-any-backup", osinfo.sysname);
 ret = (char **)setadd ((void **)ret, tmp, SET_TYPE_STRING);
 ret = (char **)setadd ((void **)ret, "generic-any-backup", SET_TYPE_STRING);

 return ret;
}

int mount_try_mount (char *mountpoint, char *fs, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 void **functions;
 char **fnames = mount_generate_mount_function_suffixes(fs);

 functions = function_find ("fs-mount", 1, (const char **)fnames);
 if (functions) {
  uint32_t r = 0;
  for (; functions[r]; r++) {
   einit_mount_function f = functions[r];
   if (f (mountpoint, fs, dd, mp, status) == status_ok) {
    free (functions);
    free (fnames);

    if (!(coremode & einit_mode_sandbox)) {
     if (mp->after_mount)
      pexec_v1 (mp->after_mount, (const char **)mp->variables, NULL, status);

     if (mp->manager)
      startdaemon (mp->manager, status);
    }

    struct einit_event eem = evstaticinit (einit_mount_node_mounted);
    eem.string = mountpoint;
    event_emit (&eem, einit_event_flag_broadcast);
    evstaticdestroy (eem);

    mp->status |= device_status_mounted;

    update_real_mtab();

    if (mount_critical && inset ((const void **)mount_critical, (const void *)mountpoint, SET_TYPE_STRING)) {
     struct einit_event ev = evstaticinit(einit_core_update_modules);

     fbprintf (status, "updating list of modules");

     event_emit (&ev, einit_event_flag_broadcast);

     evstaticdestroy(ev);
    }

    return status_ok;
   }
  }
  free (functions);
 }

 free (fnames);

 fbprintf (status, "none of the functions worked, giving up.");

 return status_failed;
}

int mount_try_umount (char *mountpoint, char *fs, char step, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 void **functions;
 char **fnames = mount_generate_mount_function_suffixes(mp->fs);

 functions = function_find ("fs-umount", 1, (const char **)fnames);
 if (functions) {
  uint32_t r = 0;
  for (; functions[r]; r++) {
   einit_umount_function f = functions[r];

   if (f (mountpoint, mp->fs, step, dd, mp, status) == status_ok) {
    free (functions);
    free (fnames);

    update_real_mtab();
    return status_ok;
   }
  }
  free (functions);
 }

 free (fnames);

 return status_failed;
}

int mount_mount (char *mountpoint, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 if (!(coremode & einit_mode_sandbox)) {
  if ((dd->device_status & (device_status_dirty | device_status_error_notint)) && (!inset ((const void **)mp->options, "skip-fsck", SET_TYPE_STRING)))
   mount_fsck (mp->fs, dd->device, status);

  if (mp->before_mount)
   pexec_v1 (mp->before_mount, (const char **)mp->variables, NULL, status);
 }

 if (strmatch (mp->fs, "auto")) {
  char *t = cfg_getstring ("configuration-storage-filesystem-guessing-order", NULL);

  if (t) {
   char **guesses = str2set(':', t); 

   if (guesses) {
    uint32_t i = 0;

    for (; guesses[i]; i++) {
     if (mount_try_mount(mountpoint, guesses[i], dd, mp, status) == status_ok) {
      free (guesses);

      return status_ok;
     }
    }

    free (guesses);
   }
  }
 } else {
  return mount_try_mount(mountpoint, mp->fs, dd, mp, status);
 }

 return status_failed;
}

int mount_umount (char *mountpoint, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {

 int retval = status_failed;
 char step = 0;

 while ((step <= 4) && !(retval & status_ok)) {
  retval = mount_try_umount (mountpoint, mp->fs, step, dd, mp, status);
  step++;

  if (!(retval & status_ok)) {
   struct pc_conditional
    pcc = {.match = "cwd-below", .para = mountpoint, .match_options = einit_pmo_additive},
    pcf = {.match = "files-below", .para = mountpoint, .match_options = einit_pmo_additive},
    *pcl[3] = { &pcc, &pcf, NULL };

   if (step <= 3) {
    fbprintf (status, "umount() failed, killing some proceses and waiting for three seconds");

    pekill (pcl);

    {
     int n = 3;
     while ((n = sleep (n)));
    }
   }
  } else {
   return status_ok;
  }
 }

 fbprintf (status, "none of the functions worked, giving up.");

 return status_failed;
}

int mount_fsck (char *fs, char *device, struct einit_event *status) {
 if (mount_fastboot) {
  return status_ok;
 }

 if (mount_fsck_template) {
  char tmp[BUFFERSIZE];
  status->string = "filesystem might be dirty; running fsck";
  status_update (status);

  esprintf (tmp, BUFFERSIZE, mount_fsck_template, fs, device);
  if (coremode != einit_mode_sandbox) {
   pexec_v1 (tmp, NULL, NULL, status);
  } else {
   status->string = tmp;
   status_update (status);
  }
 } else {
  status->string = "WARNING: filesystem dirty, but no fsck command known";
  status_update (status);
 }

 return status_ok;
}

int mount_do_mount_generic (char *mountpoint, char *fs, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {

 fbprintf (status, "mounting %s on %s (fs=%s)", dd->device, mountpoint, fs);
// notice (1, "mounting %s on %s (fs=%s)", dd->device, mountpoint, fs);

 if (!(coremode & einit_mode_sandbox)) {
  if (strmatch ("/", mountpoint)) goto attempt_remount;
#if defined(DARWIN) || defined(__FreeBSD__)
  if (mount (dd->device, mountpoint, mp->mountflags, mp->flatoptions) == -1)
#else
  if (mount (dd->device, mountpoint, fs, mp->mountflags, mp->flatoptions) == -1)
#endif
  {
   status->flag++;
   fbprintf (status, "mounting node %s failed (error=%s)", mountpoint, strerror (errno));
#ifdef MS_REMOUNT
   if (errno == EBUSY) {
    attempt_remount:
     fbprintf (status, "attempting to remount node %s instead of mounting", mountpoint);

     if (mount (dd->device, mountpoint, fs, MS_REMOUNT | mp->mountflags, mp->flatoptions) == -1) {
      fbprintf (status, "remounting node %s failed (error=%s)", mountpoint, strerror (errno));
      goto mount_panic;
     } else
      fbprintf (status, "remounted node %s", mountpoint);
   } else
#else
   attempt_remount:
#endif
   {
    mount_panic:
     status->flag++;
     status_update (status);
     if (mp->after_umount)
      pexec_v1 (mp->after_umount, (const char **)mp->variables, NULL, status);
     return status_failed;
   }
  }

 }

// mount_success:

 if (strmatch (mp->fs, "auto")) {
  free (mp->fs);
  mp->fs = estrdup (fs);
 }

 if (strmatch (mountpoint, "/")) {
  unlink ("/fastboot"); /* make sure to remove the fastboot-file if we successfully mount / */
 }

 return status_ok;
}

int mount_do_umount_generic (char *mountpoint, char *fs, char step, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {

 fbprintf (status, "unmounting %s from %s (fs=%s, attempt #%i)", dd->device, mountpoint, fs, step);
// notice (1, "unmounting %s from %s (fs=%s, attempt #%i)", dd->device, mountpoint, fs, step);

#if defined(DARWIN) || defined(__FreeBSD__)
 if (unmount (mountpoint, 0) != -1)
#else
  if (umount (mountpoint) != -1)
#endif
 {
  goto umount_ok;
 } else {
  fbprintf (status, "%s#%i: umount() failed: %s", mountpoint, step, strerror(errno));
#ifdef LINUX
  if (step >= 2) {
   if (umount2 (mountpoint, MNT_FORCE) != -1) {
    goto umount_ok;
   } else {
    fbprintf (status, "%s#%i: umount() failed: %s", mountpoint, step, strerror(errno));
    errno = 0;
   }

   if (step >= 3) {
    if (mount (dd->device, mountpoint, mp->fs, MS_REMOUNT | MS_RDONLY, NULL) == -1) {
     fbprintf (status, "%s#%i: remounting r/o failed: %s", mountpoint, step, strerror(errno));
     errno = 0;
     goto umount_fail;
    } else {
     if (umount2 (mountpoint, MNT_DETACH) == -1) {
      fbprintf (status, "%s#%i: remounted r/o but detaching failed: %s", mountpoint, step, strerror(errno));
      errno = 0;
      goto umount_ok;
     } else {
      fbprintf (status, "%s#%i: remounted r/o and detached", mountpoint, step);
      goto umount_ok;
     }
    }
   }
  }
#else
  goto umount_fail;
#endif
 }

 umount_fail:

 if (!shutting_down) status->flag++;
 return status_failed;

 umount_ok:

 if (!(coremode & einit_mode_sandbox)) {
  if (mp && mp->after_umount)
   pexec_v1 (mp->after_umount, (const char **)mp->variables, NULL, status);
 }
 if (mp && (mp->status & device_status_mounted))
  mp->status ^= device_status_mounted;

 struct einit_event eem = evstaticinit (einit_mount_node_unmounted);
 eem.string = mountpoint;
 event_emit (&eem, einit_event_flag_broadcast);
 evstaticdestroy (eem);

 update_real_mtab();

 return status_ok;
}

int emount (char *mountpoint, struct einit_event *status) {
 if (coremode & einit_mode_sandbox) return status_ok;

 struct device_data *dd = mount_get_device_data (mountpoint, NULL);
 if (dd && dd->mountpoints) {
  struct stree *t = streefind (dd->mountpoints, mountpoint, tree_find_first);

  if (t) {
   struct mountpoint_data *mp = t->value;

   if (mp->status & device_status_mounted) {
    update_real_mtab();
    return status_ok;
   }

   return mount_mount (mountpoint, dd, mp, status);
  } else {
   fbprintf (status, "can't find details for mountpoint \"%s\".", mountpoint);

   return status_failed;
  }
 }

 fbprintf (status, "can't find data for mountpoint \"%s\".", mountpoint);

 return status_failed;
}

int eumount (char *mountpoint, struct einit_event *status) {
 if (coremode & einit_mode_sandbox) return status_ok;

 emutex_lock (&mount_device_data_mutex);
 mount_update_nodes_from_mtab();
 emutex_unlock (&mount_device_data_mutex);

 char **cm = mount_get_mounted_mountpoints();

 if (mount_dont_umount && inset ((const void **)mount_dont_umount, (const void *)mountpoint, SET_TYPE_STRING)) return status_ok;

 if (cm) {
  uint32_t i = 0;

  for (; cm[i]; i++) {
   if (strstr (cm[i], mountpoint) == cm[i]) { // find mountpoints below this one that are still mounted
    uint32_t n = strlen (mountpoint);

    if (cm[i][n] == '/') {
     notice (8, "unmounting %s: have to umount(%s) first.", mountpoint, cm[i]);

     eumount (cm[i], status);
    }
   }
  }

  free (cm);
 }

 struct device_data *dd = mount_get_device_data (mountpoint, NULL);
 if (dd && dd->mountpoints) {
  struct stree *t = streefind (dd->mountpoints, mountpoint, tree_find_first);

  if (t) {
   struct mountpoint_data *mp = t->value;

   if (!(mp->status & device_status_mounted)) {
    update_real_mtab();
    return status_ok;
   } else {
    int r = mount_umount (mountpoint, dd, mp, status);

    if (shutting_down) {
	 if (r == status_failed) {
      fbprintf (status, "we're shutting down, so there's not much to worry about if umounting failed: last-rites will fix it later");
	  return status_ok;
	 }
	}

    return r;
   }
  } else {
   fbprintf (status, "can't find details for mountpoint \"%s\".", mountpoint);

   return status_failed;
  }
 }

 fbprintf (status, "can't find data for mountpoint \"%s\".", mountpoint);

 return status_failed;
}

void einit_mount_update_configuration () {
 struct cfgnode *node = NULL;

 read_filesystem_flags_from_configuration (NULL);

 if ((node = cfg_findnode ("configuration-storage-update-steps",0,NULL)) && node->svalue) {
  char **tmp = str2set(':', node->svalue);
  uint32_t c = 0;
  mount_options = mount_update_fstab + mount_update_mtab;
  for (; tmp[c]; c++) {
   if (strmatch (tmp[c], "metadata")) mount_options |= mount_update_metadata;
   else if (strmatch (tmp[c], "block-devices")) mount_options |= mount_update_block_devices;
  }
  free (tmp);
 }

 if ((node = cfg_findnode ("configuration-storage-mountpoints-system",0,NULL)) && node->svalue)
  mount_system = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-mountpoints-critical",0,NULL)) && node->svalue)
  mount_critical = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-mountpoints-no-umount",0,NULL)) && node->svalue)
  mount_dont_umount = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-fsck-command",0,NULL)) && node->svalue)
  mount_fsck_template = estrdup(node->svalue);

 if ((node = cfg_getnode ("configuration-storage-maintain-mtab",NULL)) && node->flag && node->svalue) {
  mount_options |= mount_maintain_mtab;
  mount_mtab_file = node->svalue;
 }

 mount_update_devices();
}

int einit_mount_cleanup (struct lmodule *tm) {
 event_ignore (einit_event_subsystem_ipc, einit_mount_mount_ipc_handler);
 event_ignore (einit_event_subsystem_mount, einit_mount_mount_handler);
 event_ignore (einit_event_subsystem_core, einit_mount_einit_event_handler);

 function_unregister ("fs-mount", 1, (void *)emount);
 function_unregister ("fs-umount", 1, (void *)eumount);

 return 0;
}

int einit_mount_recover (struct lmodule *lm) {

 return status_ok;
}

int einit_mount_recover_module (struct lmodule *module) {
 struct device_data *dd = NULL;
 struct stree *t;

 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 if (mounter_dd_by_mountpoint && (t = streefind (mounter_dd_by_mountpoint, module->param, tree_find_first))) {
  dd = t->value;
 }
 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 if (dd) {
  struct stree *st = streefind (dd->mountpoints, module->param, tree_find_first);

  if (st) {
   struct mountpoint_data *mp = st->value;

   if (mp && (mp->status & device_status_mounted)) {
    notice (3, "recovering %s", module->module->rid);
    mod (einit_module_enable | einit_module_ignore_dependencies, module, NULL);
   }
  }
 }

 return status_ok;
}

int einit_mount_configure (struct lmodule *r) {
 struct stat st;
 module_init (r);

 thismodule->scanmodules = einit_mount_scanmodules;
 thismodule->cleanup = einit_mount_cleanup;
 thismodule->recover = einit_mount_recover;

 /* pexec configuration */
 exec_configure (this);

 event_listen (einit_event_subsystem_ipc, einit_mount_mount_ipc_handler);
 event_listen (einit_event_subsystem_mount, einit_mount_mount_handler);
 event_listen (einit_event_subsystem_core, einit_mount_einit_event_handler);

 function_register ("fs-mount", 1, (void *)emount);
 function_register ("fs-umount", 1, (void *)eumount);

 function_register ("fs-mount-generic-any", 1, (void *)mount_do_mount_generic);
 function_register ("fs-umount-generic-any", 1, (void *)mount_do_umount_generic);

 einit_mount_update_configuration();

 if (!stat ("/fastboot", &st)) {
  mount_fastboot = 1;
 }

 return 0;
}
