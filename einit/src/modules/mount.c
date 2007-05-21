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
 .rid       = "mount",
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

#if 1


char *provides_mountlocal[] = {"mount-local", NULL};
char *requires_mountlocal[] = {"mount-system", "mount-critical", NULL};
struct smodule sm_mountlocal = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "mount (local)",
 .rid		= "einit-mount-local",
 .si           = {
  .provides = provides_mountlocal,
  .requires = requires_mountlocal,
  .after    = NULL,
  .before   = NULL
 }
};

char *provides_mountremote[] = { "mount-remote", NULL};
char *requires_mountremote[] = { "mount-system", "network", NULL};
char *after_mountremote[] = { "portmap", NULL};
struct smodule sm_mountremote = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "mount (remote)",
 .rid		= "einit-mount-remote",
 .si           = {
  .provides = provides_mountremote,
  .requires = requires_mountremote,
  .after    = after_mountremote,
  .before   = NULL
 }
};

char *provides_system[] = {"mount-system", NULL};
struct smodule sm_system = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "Device Mounter ( mount-system )",
 .rid		= "einit-mount-system",
 .si           = {
  .provides = provides_system,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

char *provides_critical[] = {"mount-critical", NULL};
char *requires_critical[] = {"mount-system", NULL};
struct smodule sm_critical = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "Device Mounter ( mount-critical )",
 .rid		= "einit-mount-critical",
 .si           = {
  .provides = provides_critical,
  .requires = requires_critical,
  .after    = NULL,
  .before   = NULL
 }
};

char *provides_sysfs[] = {"mount-sysfs", NULL};
char *requires_sysfs[] = {"mount-rootfs", NULL};
struct smodule sm_sysfs = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "Device Mounter ( mount-sysfs )",
 .rid		= "mount-sysfs",
 .si           = {
  .provides = provides_sysfs,
  .requires = requires_sysfs,
  .after    = NULL,
  .before   = NULL
 }
};

char *provides_proc[] = {"mount-proc", NULL};
char *requires_proc[] = {"mount-rootfs", NULL};
struct smodule sm_proc = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "Device Mounter ( mount-proc )",
 .rid		= "mount-proc",
 .si           = {
  .provides = provides_proc,
  .requires = requires_proc,
  .after    = NULL,
  .before   = NULL
 }
};

char *provides_rootfs[] = {"mount-rootfs", NULL};
char *before_rootfs[] = {".*", NULL};
struct smodule sm_rootfs = {
 .eiversion	= EINIT_VERSION,
 .version	= 1,
 .mode		= einit_module_generic,
 .name		= "Device Mounter ( mount-rootfs )",
 .rid		= "mount-rootfs",
 .si           = {
  .provides = provides_rootfs,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

/* variable declarations */
pthread_mutex_t blockdevices_mutex;
char *defaultblockdevicesource[5];
char *defaultfstabsource[5];
char *defaultmtabsource[5];
char *defaultfilesystems[5];

/* function declarations */
unsigned char find_block_devices_recurse_path (char *);
unsigned char forge_fstab_by_label (void *);
unsigned char read_fstab_from_configuration (void *);
unsigned char read_fstab (void *);
unsigned char read_mtab (void *);
unsigned char read_filesystem_flags_from_configuration (void *);
void einit_mount_update (enum update_task);
#define update_filesystem_metadata() einit_mount_update (UPDATE_METADATA)
#define update_block_devices() einit_mount_update (UPDATE_BLOCK_DEVICES)
#define update_fstab() einit_mount_update (UPDATE_FSTAB)
#define update_mtab() einit_mount_update (UPDATE_MTAB)

void einit_mount_mount_ipc_handler(struct einit_event *);
void einit_mount_mount_update_handler(struct einit_event *);

void add_block_device (char *, uint32_t, uint32_t);
void add_fstab_entry (char *, char *, char *, char **, uint32_t, char *, char *, char *, char *, char *, uint32_t, char **);
void add_mtab_entry (char *, char *, char *, char *, uint32_t, uint32_t);
void add_filesystem (char *, char *);

struct stree *read_fsspec_file (char *);

/* variable definitions */
pthread_mutex_t blockdevices_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fstab_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fs_mutex = PTHREAD_MUTEX_INITIALIZER;
char *fslist_hr[] = {
 "unknown",
 "ext2",
 "ext3",
 "reiserfs",
 "reiser4",
 "jfs",
 "xfs",
 "ufs"
};
char *defaultblockdevicesource[] = {"dev", NULL};
char *defaultfstabsource[] = {"configuration", NULL};
char *defaultmtabsource[] = {"legacy", NULL};
char *defaultfilesystems[] = {"linux", NULL};
char *fsck_command = NULL;

struct mount_control_block mcb = {
 .blockdevices		= NULL,
 .fstab			= NULL,
 .filesystems		= NULL,
 .add_block_device	= add_block_device,
 .add_fstab_entry	= add_fstab_entry,
 .add_mtab_entry	= add_mtab_entry,
 .add_filesystem	= add_filesystem,
 .update_options	= mount_update_metadata + mount_update_block_devices + mount_update_fstab + mount_update_mtab,
 .critical		= NULL,
 .noumount		= NULL
};

/* function declarations */
int einit_mount_scanmodules (struct lmodule *);
int einit_mount_cleanup (struct lmodule *);
int einit_mount_enable (enum mounttask, struct einit_event *);
int einit_mount_disable (enum mounttask, struct einit_event *);
int emount (char *, struct einit_event *);
int eumount (char *, struct einit_event *);
char *options_string_to_mountflags (char **, unsigned long *, char *);
void einit_mount_einit_event_handler (struct einit_event *);
void einit_mount_update_configuration ();

char *generate_legacy_mtab (struct mount_control_block *);

/* macro definitions */
#define update_real_mtab() {\
 if (mcb.options & mount_maintain_mtab) {\
  char *tmpmtab = generate_legacy_mtab (&mcb);\
\
  if (tmpmtab) {\
   unlink (mcb.mtab_file);\
\
   FILE *mtabfile = efopen (mcb.mtab_file, "w");\
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


/* function definitions */

/* the actual module */

int einit_mount_cleanup (struct lmodule *this) {
 struct stree *ucur;

 streefree (mcb.blockdevices);
 ucur = mcb.fstab;
 while (ucur) {
  if (ucur->value) {
/*   if (((struct fstab_entry *)(ucur->value))->mountpoint)
   free (((struct fstab_entry *)(ucur->value))->mountpoint);
   if (((struct fstab_entry *)(ucur->value))->device)
   free (((struct fstab_entry *)(ucur->value))->device);
   if (((struct fstab_entry *)(ucur->value))->fs)
   free (((struct fstab_entry *)(ucur->value))->fs);*/
   if (((struct fstab_entry *)(ucur->value))->before_mount)
    free (((struct fstab_entry *)(ucur->value))->before_mount);
   if (((struct fstab_entry *)(ucur->value))->after_mount)
    free (((struct fstab_entry *)(ucur->value))->after_mount);
   if (((struct fstab_entry *)(ucur->value))->before_umount)
    free (((struct fstab_entry *)(ucur->value))->before_umount);
   if (((struct fstab_entry *)(ucur->value))->after_umount)
    free (((struct fstab_entry *)(ucur->value))->after_umount);
   if (((struct fstab_entry *)(ucur->value))->variables)
    free (((struct fstab_entry *)(ucur->value))->variables);
   if (((struct fstab_entry *)(ucur->value))->manager) {
    if (((struct fstab_entry *)(ucur->value))->manager->command)
     free (((struct fstab_entry *)(ucur->value))->manager->command);

    free (((struct fstab_entry *)(ucur->value))->manager);
   }
  }
  ucur = streenext (ucur);
 }
 streefree (mcb.fstab);
 streefree (mcb.filesystems);

 mcb.blockdevices = NULL;
 mcb.fstab = NULL;
 mcb.filesystems = NULL;

 function_unregister ("find-block-devices-dev", 1, (void *)find_block_devices_recurse_path);
 function_unregister ("read-fstab-label", 1, (void *)forge_fstab_by_label);
 function_unregister ("read-fstab-configuration", 1, (void *)read_fstab_from_configuration);
 function_unregister ("read-fstab-fstab", 1, (void *)read_fstab);
 function_unregister ("read-mtab-legacy", 1, (void *)read_mtab);
 function_unregister ("fs-mount", 1, (void *)emount);
 function_unregister ("fs-umount", 1, (void *)eumount);

 event_ignore (einit_event_subsystem_core, einit_mount_einit_event_handler);
 event_ignore (einit_event_subsystem_ipc, einit_mount_mount_ipc_handler);
 event_ignore (einit_event_subsystem_mount, einit_mount_mount_update_handler);

 if (fsck_command) {
  free (fsck_command);
  fsck_command = NULL;
 }

 if (mcb.critical) {
  free (mcb.critical);
  mcb.critical = NULL;
 }

 exec_cleanup(this);

 return 0;
}

unsigned char find_block_devices_recurse_path (char *path) {
 DIR *dir;
 struct dirent *entry;
 if (path == (char *)&mcb) path = "/dev/";

#ifdef POSIXREGEX
 unsigned char nfitfc = 0;
 static char *npattern = NULL;
 static regex_t devpattern;
 static unsigned char havedevpattern = 0;

 if (!npattern) {
  nfitfc = 1;
  npattern = cfg_getstring ("configuration-storage-block-devices-dev-constraints", NULL);
  if (npattern) {
   if (!(havedevpattern = !eregcomp (&devpattern, npattern))) {
    notice (2, "find_block_devices_recurse_path(): bad device constraints, bailing...");
    return 0;
   }
  } else {
   notice (2, "find_block_devices_recurse_path(): no device constraints, bailing...");

   return 0;
  }
 }
#endif

 dir = eopendir (path);
 if (dir != NULL) {
  while ((entry = ereaddir (dir))) {
   if (entry->d_name[0] == '.') continue;
   struct stat statbuf;
   char *tmp = emalloc (strlen(path) + entry->d_reclen);
   tmp[0] = 0;
   tmp = strcat (tmp, path);
   tmp = strcat (tmp, entry->d_name);
   if (lstat (tmp, &statbuf)) {
    perror ("einit-common-mount");
    free (tmp);
    continue;
   }
   if (!S_ISLNK(statbuf.st_mode)) {
#ifdef POSIXREGEX
    if (S_ISBLK (statbuf.st_mode) && (!havedevpattern || !regexec (&devpattern, tmp, 0, NULL, 0)))
#else
     if (S_ISBLK (statbuf.st_mode))
#endif
    {
     add_block_device (tmp, 0, 0);
// what was i thinking with this one?
//    } else if (S_ISSOCK (statbuf.st_mode) && S_ISDIR (statbuf.st_mode)) {
    } else if (S_ISDIR (statbuf.st_mode)) {
     tmp = strcat (tmp, "/");
     find_block_devices_recurse_path (tmp);
    }
   }

   free (tmp);
  }
  eclosedir (dir);
 } else {
  errno = 0;
  return 1;
 }


#ifdef POSIXREGEX
 if (nfitfc) {
  npattern = NULL;
  havedevpattern = 0;
  regfree (&devpattern);
 }
#endif
 return 0;
}

unsigned char forge_fstab_by_label (void *na) {
 struct stree *element = mcb.blockdevices;
 struct cfgnode *node = NULL;
 char *hostname = NULL;
 uint32_t hnl = 0;
 if ((node = cfg_findnode ("hostname", 0, node))) {
  hostname = node->svalue;
 } else if ((node = cfg_findnode ("conf_hostname", 0, node))) {
  hostname = node->svalue;
 } else {
  hostname = "einit";
 }
 hnl = strlen (hostname);

 while (element) {
  struct bd_info *bdi = element->value;
  if (bdi) {
   if ((bdi->status & device_status_has_medium) && !(bdi->status & BF_STATUS_ERROR)) {
    char *fsname = NULL;
    char *mpoint = NULL;
    if (bdi->fs_type < 0xffff)
     fsname = fslist_hr[bdi->fs_type];
    else
     fsname = "auto";

    if (bdi->label) {
     if (strchr (bdi->label, '/'))
      mpoint = bdi->label;
     else if (strchr (bdi->label, '-')) {
      char *cur = bdi->label;
      uint32_t i = 0;
      for (; *cur && hostname[i]; i++) {
       if (hostname[i] == *cur) cur++;
       else break;
      }
      if (i == hnl) {
       uint32_t sc = 0;
       if (*cur == 0) cur = "/";
       else if (strmatch (cur, "-root")) cur = "/";
//       else if (*cur == '-') cur++;
       mpoint = estrdup (cur);
       for (i = 0; mpoint[i]; i++) {
        if (mpoint[i] == '-') {
         mpoint[i] = '/';
         sc++;
         if (sc == 2) {
          mpoint[i] = 0;
          if (strcmp (mpoint, "/usr") &&
              strcmp (mpoint, "/opt") &&
              strcmp (mpoint, "/var") &&
              strcmp (mpoint, "/srv") &&
              strcmp (mpoint, "/tmp")) {
           free (mpoint);
           if (*cur == '-') cur++;
           mpoint = ecalloc (1, strlen(cur)+8);
           mpoint = strcat (mpoint, "/media/");
           mpoint = strcat (mpoint, cur);
           break;
              }
              mpoint[i] = '/';
         }
        }
       }
      }
     }
     if (!mpoint) {
      char tmp[BUFFERSIZE] = "/media/";
      strncat (tmp, bdi->label, sizeof(tmp) - strlen (tmp) + 1);
      mpoint = estrdup (tmp);
     }
    } else {
     char tmp[BUFFERSIZE] = "/media";
     strncat (tmp, element->key, sizeof(tmp) - strlen (tmp) + 1);
     mpoint = estrdup (tmp);
    }

    if (mpoint)
     mcb.add_fstab_entry (estrdup(mpoint), estrdup(element->key), estrdup(fsname), NULL, 0, NULL, NULL, NULL, NULL, NULL, 0, NULL);
   }
  }
  element = streenext (element);
 }
 return 0;
}

unsigned char read_fstab_from_configuration (void *na) {
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
   }

   add_fstab_entry (mountpoint, device, fs, options, mountflags, before_mount, after_mount, before_umount, after_umount, manager, 1, variables);
  }
 }
 return 0;
}

unsigned char read_fstab (void *na) {
 struct stree *workstree = read_fsspec_file ("/etc/fstab");
 struct stree *cur = workstree;

 while (cur) {
  struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;

  add_fstab_entry (val->fs_file ? estrdup(val->fs_file) : NULL, val->fs_spec ? estrdup(val->fs_spec) : NULL, val->fs_vfstype ? estrdup(val->fs_vfstype) : NULL, str2set (',', val->fs_mntops), 0, NULL, NULL, NULL, NULL, NULL, 0, NULL);

  cur = streenext (cur);
 }

 streefree(workstree);
 return 0;
}

unsigned char read_mtab (void *na) {
 struct stree *workstree = read_fsspec_file ("/proc/mounts");
 struct stree *cur = workstree;

 if (workstree) {
  while (cur) {
   struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;
   add_mtab_entry (val->fs_spec, val->fs_file, val->fs_vfstype, val->fs_mntops, val->fs_freq, val->fs_passno);

   cur = streenext (cur);
  }

  streefree(workstree);
 }
 return 0;
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
   add_filesystem (id, flags);
  }
 }
 return 0;
}

void einit_mount_update (enum update_task task) {
 struct cfgnode *node = NULL;
 char **fl = NULL;
 void **functions = NULL;
 char *flb = NULL;
 uint32_t i = 0;
 void (*f)(struct mount_control_block *);

 switch (task) {
  case UPDATE_METADATA:
   if (!(mcb.update_options & mount_update_metadata)) return;
   node = cfg_findnode ("configuration-storage-filesystem-label-readers", 0, NULL);
   fl = defaultfilesystems;
   flb = "fs-read-metadata";
   break;
  case UPDATE_BLOCK_DEVICES:
   if (!(mcb.update_options & mount_update_block_devices)) return;
   node = cfg_findnode ("configuration-storage-block-devices-source", 0, NULL);
   fl = defaultblockdevicesource;
   flb = "find-block-devices";
   break;
  case UPDATE_FSTAB:
   if (!(mcb.update_options & mount_update_fstab)) return;
   node = cfg_findnode ("configuration-storage-fstab-source", 0, NULL);
   fl = defaultfstabsource;
   flb = "read-fstab";
   break;
  case UPDATE_MTAB:
   if (!(mcb.update_options & mount_update_mtab)) return;
   node = cfg_findnode ("configuration-storage-mtab-source", 0, NULL);
   fl = defaultmtabsource;
   flb = "read-mtab";
   break;
 }

 if (node && node->svalue)
  fl = str2set (':', node->svalue);

 functions = function_find (flb, 1, (const char **)fl);
 if (functions && functions[0]) {
  for (; functions[i]; i++) {
   f = functions[i];
   f (&mcb);
  }
  free (functions);
 }

 switch (task) {
  case UPDATE_METADATA:
   if (fl != defaultfilesystems) free (fl);
   if (mcb.update_options & mount_update_fstab) einit_mount_update (UPDATE_FSTAB);
   break;
  case UPDATE_BLOCK_DEVICES:
   if (fl != defaultblockdevicesource) free (fl);
   if (mcb.update_options & mount_update_metadata) einit_mount_update (UPDATE_METADATA);
   break;
  case UPDATE_FSTAB:
   if (fl != defaultfstabsource) free (fl);
   break;
  case UPDATE_MTAB:
   if (fl != defaultmtabsource) free (fl);
   break;
 }
}

int einit_mount_scanmodules (struct lmodule *modchain) {
 struct lmodule *new,
 *lm = modchain;
 char doop = 1;

 while (lm) { if (lm->source && strmatch(lm->source, sm_mountlocal.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_mountlocal))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))einit_mount_enable;
  new->disable = (int (*)(void *, struct einit_event *))einit_mount_disable;
  new->param = (void *)MOUNT_LOCAL;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_mountremote.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_mountremote))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))einit_mount_enable;
  new->disable = (int (*)(void *, struct einit_event *))einit_mount_disable;
  new->param = (void *)MOUNT_REMOTE;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_system.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_system))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))einit_mount_enable;
  new->disable = (int (*)(void *, struct einit_event *))einit_mount_disable;
  new->param = (void *)MOUNT_SYSTEM;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_critical.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_critical))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))einit_mount_enable;
  new->disable = (int (*)(void *, struct einit_event *))einit_mount_disable;
  new->param = (void *)MOUNT_CRITICAL;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_sysfs.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_sysfs))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))emount;
  new->disable = (int (*)(void *, struct einit_event *))eumount;
  new->param = (void *)estrdup("/sys");
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_proc.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_proc))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))emount;
  new->disable = (int (*)(void *, struct einit_event *))eumount;
  new->param = (void *)estrdup("/proc");
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && strmatch(lm->source, sm_rootfs.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_rootfs))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))emount;
  new->disable = (int (*)(void *, struct einit_event *))eumount;
  new->param = (void *)estrdup("/");
 }

 return 0;
}

char *options_string_to_mountflags (char **options, unsigned long *mntflags, char *mountpoint) {
 int fi = 0;
 char *ret = NULL;

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

#if 0
#ifdef MS_NOTAIL
   if (strmatch (options[fi], "notail")) (*mntflags) |= MS_NOTAIL;
     else if (strmatch (options[fi], "tail")) (*mntflags) = ((*mntflags) & MS_NOTAIL) ? (*mntflags) ^ MS_NOTAIL : (*mntflags);
     else
#endif
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

/* --------- the big mount-wrapper-function ------------------------------- */
int emount (char *mountpoint, struct einit_event *status) {
 struct stree *he = mcb.fstab;
 struct stree *de = mcb.blockdevices;
 struct fstab_entry *fse = NULL;
 struct bd_info *bdi = NULL;
 char *source;
 char *fstype = NULL;
 char *fsdata = NULL;
 uint32_t fsntype;
 char verbosebuffer [BUFFERSIZE];
 void **fs_mount_functions = NULL;
 char *fs_mount_function_name;
 unsigned char (*mount_function)(char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);

 unsigned long mntflags = 0;

 if (!mountpoint) return status_failed;

 notice (4, "mounting %s", mountpoint);

 if (coremode & einit_mode_sandbox) return status_ok;

 char **fstype_s = NULL;
 uint32_t fsts_i = 0;
 if (he && (he = streefind (he, mountpoint, tree_find_first)) && (fse = (struct fstab_entry *)he->value)) {
  source = fse->device;
  fsntype = 0;
  if (de && source && (de = streefind (de, source, tree_find_first)) && (bdi = (struct bd_info *)de->value)) {
   fsntype = bdi->fs_type;
  }

  if (fse->fs) {
   fstype = fse->fs;
  } else if (fsntype) {
   if (bdi->fs_type < 0xffff)
    fstype = fslist_hr[bdi->fs_type];
   else
    fstype = NULL;
  } else
   fstype = NULL;

   if (!fstype) fstype = "auto";

   if (bdi && (bdi->status & device_status_dirty)) {
    if (fsck_command) {
     char tmp[BUFFERSIZE];
     status->string = "filesystem dirty; running fsck";
     status_update (status);

     esprintf (tmp, BUFFERSIZE, fsck_command, fstype, de->key);
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
   }

   if (!source)
    source = fstype;

   if (strmatch (fstype, "auto"))
    fstype = cfg_getstring ("configuration-storage-filesystem-guessing-order", NULL);

   fstype_s = str2set (':', fstype);

   mntflags = 0;
   if (fse->options)
    fsdata = options_string_to_mountflags (fse->options, &mntflags, mountpoint);

   if (coremode != einit_mode_sandbox) {
    if (fse->before_mount)
     pexec_v1 (fse->before_mount, (const char **)fse->variables, NULL, status);
   }

   if (fstype_s) for (; fstype_s[fsts_i]; fsts_i++) {
    fstype = fstype_s[fsts_i];

    if (bdi && bdi->label)
     esprintf (verbosebuffer, BUFFERSIZE, "mounting %s [%s; label=%s; fs=%s]", mountpoint, source, bdi->label, fstype);
    else
     esprintf (verbosebuffer, BUFFERSIZE, "mounting %s [%s; fs=%s]", mountpoint, source, fstype);
    status->string = verbosebuffer;
    status_update (status);

    fs_mount_function_name = emalloc (10+strlen (fstype));
    *fs_mount_function_name = 0;
    fs_mount_function_name = strcat (fs_mount_function_name, "fs-mount-");
    fs_mount_function_name = strcat (fs_mount_function_name, fstype);

    if (fs_mount_function_name) {
     fs_mount_functions = function_find (fs_mount_function_name, 1, NULL);
     free (fs_mount_function_name);
    }

    if (fs_mount_functions && fs_mount_functions[0]) {
     uint32_t j = 0;
     for (; fs_mount_functions[j]; j++) {
      mount_function = fs_mount_functions[j];
      if (!mount_function (source, mountpoint, fstype, bdi, fse, status)) {
       free (fs_mount_functions);
       goto mount_success;
      }
     }
     free (fs_mount_functions);
    }

    if (coremode != einit_mode_sandbox) {
// root node should only be remounted...
     if (strmatch ("/", mountpoint)) goto attempt_remount;
#if defined(DARWIN) || defined(__FreeBSD__)
     if (mount (source, mountpoint, mntflags, fsdata) == -1)
#else
      if (mount (source, mountpoint, fstype, mntflags, fsdata) == -1)
#endif
     {
#ifdef MS_REMOUNT
      if (errno == EBUSY) {
      attempt_remount:
        status->string = "attempting to remount node instead of mounting";
      status_update (status);

      if (mount (source, mountpoint, fstype, MS_REMOUNT | mntflags, fsdata) == -1) {
       status->string = "remounting node failed...";
       status_update (status);
       goto mount_panic;
      }
      else
       status->string = "remounting node...";
      status_update (status);
      } else
#else
     attempt_remount:
       status->string = "should now try to remount node, but OS does not support this";
     status_update (status);
#endif
     {
      mount_panic:

        status->string = (char *)strerror(errno);
      status_update (status);
      if (fse->after_umount)
       pexec_v1 (fse->after_umount, (const char **)fse->variables, NULL, status);
      continue;
     }
     }
    }

   mount_success:

     fse->afs = fstype;
   fse->adevice = source;
   fse->aflags = mntflags;

   if (!(coremode & einit_mode_sandbox)) {
    if (fse->after_mount)
     pexec_v1 (fse->after_mount, (const char **)fse->variables, NULL, status);

    if (fse->manager)
     startdaemon (fse->manager, status);
   }

   struct einit_event eem = evstaticinit (einit_mount_node_mounted);
   eem.string = mountpoint;
   event_emit (&eem, einit_event_flag_broadcast);
   evstaticdestroy (eem);

   fse->status |= device_status_mounted;

   return status_ok;
   }

// we reach this if none of the attempts worked out
   if (fstype_s) free (fstype_s);
   return status_failed;
 } else {
  status->string = "nothing known about this mountpoint; bailing out.";
  status_update (status);
  return status_failed;
 }

 return status_ok;
}

int eumount (char *mountpoint, struct einit_event *status) {
 struct stree *he = mcb.fstab;
 struct fstab_entry *fse = NULL;

 if (!mountpoint) return status_failed;

 notice (4, "unmounting %s", mountpoint);

 if (coremode & einit_mode_sandbox) return status_ok;

 char textbuffer[BUFFERSIZE];
 errno = 0;
 uint32_t retry = 0;

 if (inset ((const void **)mcb.noumount, (void *)mountpoint, SET_TYPE_STRING)) return status_ok;

 if (he && (he = streefind (he, mountpoint, tree_find_first))) fse = (struct fstab_entry *)he->value;

 if (fse && !(fse->status & device_status_mounted))
  esprintf (textbuffer, BUFFERSIZE, "unmounting %s: seems not to be mounted", mountpoint);
 else
  esprintf (textbuffer, BUFFERSIZE, "unmounting %s", mountpoint);

 if (fse && fse->manager)
  stopdaemon (fse->manager, status);

 status->string = textbuffer;
 status_update (status);

 while (1) {
  retry++;

#if defined(DARWIN) || defined(__FreeBSD__)
  if (unmount (mountpoint, 0) != -1)
#else
   if (umount (mountpoint) != -1)
#endif
  {
   goto umount_ok;
  } else {
   struct pc_conditional pcc = {.match = "cwd-below", .para = mountpoint, .match_options = einit_pmo_additive},
   pcf = {.match = "files-below", .para = mountpoint, .match_options = einit_pmo_additive},
   *pcl[3] = { &pcc, &pcf, NULL };

   esprintf (textbuffer, BUFFERSIZE, "%s#%i: umount() failed: %s", mountpoint, retry, strerror(errno));
   errno = 0;
   status->string = textbuffer;
   status_update (status);

   pekill (pcl);
#ifdef LINUX
   if (retry >= 2) {
    if (umount2 (mountpoint, MNT_FORCE) != -1) {
     goto umount_ok;
    } else {
     esprintf (textbuffer, BUFFERSIZE, "%s#%i: umount2() failed: %s", mountpoint, retry, strerror(errno));
     errno = 0;
     status->string = textbuffer;
     status_update (status);
    }

    if (fse) {
     if (retry >= 3) {
      if (mount (fse->adevice, mountpoint, fse->afs, MS_REMOUNT | MS_RDONLY, NULL) == -1) {
       esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounting r/o failed: %s", mountpoint, retry, strerror(errno));
       errno = 0;
       status->string = textbuffer;
       status_update (status);
       goto umount_fail;
      } else {
       if (umount2 (mountpoint, MNT_DETACH) == -1) {
        esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounted r/o but detaching failed: %s", mountpoint, retry, strerror(errno));
        errno = 0;
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       } else {
        esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounted r/o and detached", mountpoint, retry);
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       }
      }
     }
    } else {
     esprintf (textbuffer, BUFFERSIZE, "%s#%i: device mounted but I don't know anything more; bailing out", mountpoint, retry);
     status->string = textbuffer;
     status_update (status);
     goto umount_fail;
    }
   }
#else
   goto umount_fail;
#endif
  }

  umount_fail:

    status->flag++;
  if (retry > 3) {
   return status_failed;
  }
  sleep (1);
 }

 umount_ok:
   if (!(coremode & einit_mode_sandbox)) {
  if (fse && fse->after_umount)
   pexec_v1 (fse->after_umount, (const char **)fse->variables, NULL, status);
   }
   if (fse && (fse->status & device_status_mounted))
    fse->status ^= device_status_mounted;

   struct einit_event eem = evstaticinit (einit_mount_node_unmounted);
   eem.string = mountpoint;
   event_emit (&eem, einit_event_flag_broadcast);
   evstaticdestroy (eem);

   return status_ok;
}

void add_block_device (char *devicefile, uint32_t major, uint32_t minor) {
 struct bd_info bdi;

 memset (&bdi, 0, sizeof (struct bd_info));

 bdi.major = major;
 bdi.minor = minor;
 bdi.status = device_status_has_medium | device_status_error_notint;
 emutex_lock (&blockdevices_mutex);
 if (mcb.blockdevices && streefind (mcb.blockdevices, devicefile, tree_find_first)) {
  emutex_unlock (&blockdevices_mutex);
  return;
 }

 mcb.blockdevices = streeadd (mcb.blockdevices, devicefile, &bdi, sizeof (struct bd_info), NULL);
// mcb.blockdevices = streeadd (mcb.blockdevices, devicefile, bdi, -1);

 emutex_unlock (&blockdevices_mutex);
}

void add_fstab_entry (char *mountpoint, char *device, char *fs, char **options, uint32_t mountflags, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, uint32_t manager_restart, char **variables) {
 struct fstab_entry fse;
 uint32_t i = 0;
 if (!mountpoint) return;

// eprintf (stderr, "DD adding fstab entry: %s -> %s", mountpoint, device ? device : "(nodev)");

 memset (&fse, 0, sizeof (struct fstab_entry));

 fse.mountpoint = mountpoint;
 fse.device = device;
 fse.fs = fs;
 if (options) {
  char **noptions = NULL;
  for (i = 0; options[i]; i++) {
   if (strmatch (options[i], "noauto")) mountflags |= mount_fstab_noauto;
   else if (strmatch (options[i], "critical")) mountflags |= mount_fstab_critical;
   else noptions = (char **)setadd ((void **)noptions, (void *)(options[i]), SET_TYPE_STRING);
  }
  free (options);
  fse.options = noptions;
 }
 fse.mountflags = mountflags;
 fse.before_mount = before_mount;
 fse.after_mount = after_mount;
 fse.before_umount = before_umount;
 fse.after_umount = after_umount;
 if (manager) {
  fse.manager = ecalloc (1, sizeof (struct dexecinfo));
  fse.manager->command = manager;
  fse.manager->variables = variables;
  fse.manager->restart = manager_restart;
 }
 fse.variables = variables;

 emutex_lock (&fstab_mutex);
 if (mcb.fstab && streefind (mcb.fstab, mountpoint, tree_find_first)) {
  if (fse.mountpoint)
   free (fse.mountpoint);
  if (fse.device)
   free (fse.device);
  if (fse.fs)
   free (fse.fs);
  if (fse.options)
   free (fse.options);
  if (fse.before_mount)
   free (fse.before_mount);
  if (fse.after_mount)
   free (fse.after_mount);
  if (fse.before_umount)
   free (fse.before_umount);
  if (fse.after_umount)
   free (fse.after_umount);
  if (fse.variables)
   free (fse.variables);
  if (fse.manager) {
   if (fse.manager->command)
    free (fse.manager->command);

   free (fse.manager);
  }

  emutex_unlock (&fstab_mutex);
  return;
 }

 mcb.fstab = streeadd (mcb.fstab, mountpoint, &fse, sizeof (struct fstab_entry), fse.options);
 emutex_unlock (&fstab_mutex);
// mcb.fstab = streeadd (mcb.fstab, mountpoint, fse, -1);
}

void add_mtab_entry (char *fs_spec, char *fs_file, char *fs_vfstype, char *fs_mntops, uint32_t fs_freq, uint32_t fs_passno) {
 struct fstab_entry fse;
 char **dset = NULL;
 struct stree *cur;

 if (!fs_file) return;

 dset = (char **)setadd ((void **)dset, (void *)fs_file, SET_TYPE_STRING);

 if (!fs_spec) dset = (char **)setadd ((void **)dset, (void *)"nodevice", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_spec, SET_TYPE_STRING);

 if (!fs_vfstype) dset = (char **)setadd ((void **)dset, (void *)"auto", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_vfstype, SET_TYPE_STRING);

 if (!fs_mntops) dset = (char **)setadd ((void **)dset, (void *)"rw", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_mntops, SET_TYPE_STRING);

 emutex_lock (&fstab_mutex);

 if (mcb.fstab && (cur = streefind (mcb.fstab, fs_file, tree_find_first))) {
  struct fstab_entry *node = cur->value;

  node->adevice = dset[1];
#ifdef MS_RDONLY
  node->aflags = inset ((const void **)dset, (void*)"ro", SET_TYPE_STRING) ? MS_RDONLY : 0;
#endif
  node->afs = dset[2];

  node->status |= device_status_mounted;
 } else {
  memset (&fse, 0, sizeof (struct fstab_entry));

  fse.status = device_status_mounted;
  fse.device = dset[1];
  fse.adevice = dset[1];
  fse.mountpoint = dset[0];
#ifdef MS_RDONLY
  fse.aflags = inset ((const void **)dset, (void*)"ro", SET_TYPE_STRING) ? MS_RDONLY : 0;
#endif
  fse.options = str2set (',', dset[3]);
  fse.afs = dset[2];
  fse.fs = dset[2];

  mcb.fstab = streeadd (mcb.fstab, fs_file, &fse, sizeof (struct fstab_entry), dset);
 }

 emutex_unlock (&fstab_mutex);
}

void add_filesystem (char *name, char *options) {
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

 emutex_lock (&fs_mutex);
 if (mcb.filesystems && streefind (mcb.filesystems, name, tree_find_first)) {
  emutex_unlock (&fs_mutex);
  return;
 }

 mcb.filesystems = streeadd (mcb.filesystems, name, (void *)flags, -1, NULL);
 emutex_unlock (&fs_mutex);
}

/* all the current IPC commands will be made #DEBUG-only, but we'll keep 'em for now */
/* --------- error checking and direct user interaction ------------------- */
void einit_mount_mount_ipc_handler(struct einit_event *ev) {
 if (!ev || !ev->argv) return;
 if (ev->argv[0] && ev->argv[1]) {
#ifdef DEBUG
  if (strmatch (ev->argv[0], "list")) {
   if (strmatch (ev->argv[1], "fstab")) {
    struct stree *cur = ( mcb.fstab ? *(mcb.fstab->lbase) : NULL );
    struct fstab_entry *val = NULL;

    ev->implemented = 1;

    while (cur) {
     val = (struct fstab_entry *) cur->value;
     if (val) {
      eprintf (ev->output, "%s [spec=%s;vfstype=%s;flags=%i;before-mount=%s;after-mount=%s;before-umount=%s;after-umount=%s;status=%i]\n", val->mountpoint, val->device, val->fs, val->mountflags, val->before_mount, val->after_mount, val->before_umount, val->after_umount, val->status);
     }
     cur = streenext (cur);
    }
   } else if (strmatch (ev->argv[1], "block-devices")) {
    struct stree *cur = mcb.blockdevices;
    struct bd_info *val = NULL;

    ev->implemented = 1;

    while (cur) {
     val = (struct bd_info *) cur->value;
     if (val) {
      eprintf (ev->output, "%s [fs=%s;type=%i;label=%s;uuid=%s;flags=%i]\n", cur->key, val->fs, val->fs_type, val->label, val->uuid, val->status);
     }
     cur = streenext (cur);
    }
   }
  } else
#endif
   if (strmatch (ev->argv[0], "examine") && strmatch (ev->argv[1], "configuration")) {
    /* error checking... */
    char **tmpset, *tmpstring;

    ev->implemented = 1;

    if (!(tmpstring = cfg_getstring("configuration-storage-fstab-source", NULL))) {
     eputs (" * configuration variable \"configuration-storage-fstab-source\" not found.\n", ev->output);
     ev->ipc_return++;
    } else {
     tmpset = str2set(':', tmpstring);

     if (inset ((const void **)tmpset, (void *)"label", SET_TYPE_STRING)) {
      if (!(mcb.update_options & mount_update_metadata)) {
       eputs (" * fstab-source \"label\" to be used, but optional update-step \"metadata\" not enabled.\n", ev->output);
       ev->ipc_return++;
      }
      if (!(mcb.update_options & mount_update_block_devices)) {
       eputs (" * fstab-source \"label\" to be used, but optional update-step \"block-devices\" not enabled.\n", ev->output);
       ev->ipc_return++;
      }
     }

     if (!inset ((const void **)tmpset, (void *)"configuration", SET_TYPE_STRING)) {
      eputs (" * fstab-source \"configuration\" disabled! In 99.999% of all cases, you don't want to do that!\n", ev->output);
      ev->ipc_return++;
     }

     free (tmpset);
    }

    if (mcb.update_options & mount_update_metadata) {
     if (!(mcb.update_options & mount_update_block_devices)) {
      eputs (" * update-step \"metadata\" cannot be performed without update-step \"block-devices\".\n", ev->output);
      ev->ipc_return++;
     }
    } else {
     eputs (" * update-step \"metadata\" disabled; not a problem per-se, but this will prevent your filesystems from being automatically fsck()'d.\n", ev->output);
    }

    if (!mcb.fstab) {
     eputs (" * your fstab is empty.\n", ev->output);
     ev->ipc_return++;
    } else {
     struct stree *tstree, *fstree;
     if (!(tstree = streefind (mcb.fstab, "/", tree_find_first))) {
      eputs (" * your fstab does not contain an entry for \"/\".\n", ev->output);
      ev->ipc_return++;
     } else if (!(((struct fstab_entry *)(tstree->value))->device)) {
      eputs (" * you have apparently forgotten to specify a device for your root-filesystem.\n", ev->output);
      ev->ipc_return++;
     } else if (strmatch ("/dev/ROOT", (((struct fstab_entry *)(tstree->value))->device))) {
      eputs (" * you didn't edit your local.xml to specify your root-filesystem.\n", ev->output);
      ev->ipc_return++;
     }

     tstree = mcb.fstab;
     while (tstree) {
      struct stat stbuf;

      if (!(((struct fstab_entry *)(tstree->value))->fs) || strmatch ("auto", (((struct fstab_entry *)(tstree->value))->fs))) {
       char tmpstr[BUFFERSIZE];
       if (inset ((const void **)(((struct fstab_entry *)(tstree->value))->options), (void *)"bind", SET_TYPE_STRING)) {
#ifdef LINUX
        tstree = streenext (tstree);
        continue;
#else
        esprintf (tmpstr, BUFFERSIZE, " * supposed to bind-mount %s, but this OS seems to lack support for this.\n", tstree->key);
#endif
        eputs (tmpstr, ev->output);
        ev->ipc_return++;
       }
      }

      if (!(((struct fstab_entry *)(tstree->value))->device)) {
       if ((((struct fstab_entry *)(tstree->value))->fs) && (fstree = streefind (mcb.filesystems, (((struct fstab_entry *)(tstree->value))->fs), tree_find_first)) && !((uintptr_t)fstree->value & filesystem_capability_volatile)) {
        eprintf (ev->output, " * no device specified for fstab-node \"%s\", and filesystem does not have the volatile-attribute.\n", tstree->key);
        ev->ipc_return++;
       }
      } else if ((stat ((((struct fstab_entry *)(tstree->value))->device), &stbuf) == -1) && (!(((struct fstab_entry *)(tstree->value))->fs) || (mcb.filesystems && (fstree = streefind (mcb.filesystems, (((struct fstab_entry *)(tstree->value))->fs), tree_find_first)) && !((uintptr_t)fstree->value & filesystem_capability_volatile) && !((uintptr_t)fstree->value & filesystem_capability_network)))) {
       eprintf (ev->output, " * cannot stat device \"%s\" from node \"%s\", the error was \"%s\".\n", (((struct fstab_entry *)(tstree->value))->device), tstree->key, strerror (errno));
       ev->ipc_return++;
      }

      if (((struct fstab_entry *)(tstree->value))->options) {
       unsigned long tmpmnt = 0;
       char *xtmp = options_string_to_mountflags ((((struct fstab_entry *)(tstree->value))->options), &tmpmnt, (((struct fstab_entry *)(tstree->value))->mountpoint));

       if (((struct fstab_entry *)(tstree->value))->options[0] && !((struct fstab_entry *)(tstree->value))->options[1] && strchr (((struct fstab_entry *)(tstree->value))->options[0], ',') && strmatch (((struct fstab_entry *)(tstree->value))->options[0], xtmp)) {
        eprintf (ev->output, " * node \"%s\": these options look fishy: \"%s\"\n   remember: in the xml files, you need to specify options separated using colons (:), not commas (,)!\n", ((struct fstab_entry *)(tstree->value))->mountpoint, xtmp);
       }

       if (xtmp) {
        free (xtmp);
       }
      }

      tstree = streenext (tstree);
     }
    }

   }
 }
}

void einit_mount_mount_update_handler(struct einit_event *event) {
 if (event) {
  if ((event->flag & mount_update_metadata) && (mcb.update_options & mount_update_metadata)) update_filesystem_metadata ();
  if ((event->flag & mount_update_block_devices) && (mcb.update_options & mount_update_block_devices)) update_block_devices ();
  if ((event->flag & mount_update_fstab) && (mcb.update_options & mount_update_fstab)) update_fstab ();
  if ((event->flag & mount_update_mtab) && (mcb.update_options & mount_update_mtab)) update_mtab ();
 }
}

char *generate_legacy_mtab (struct mount_control_block *cb) {
 char *ret = NULL;
 ssize_t retlen = 0;

 if (!cb) return NULL;

 struct stree *cur = cb->fstab;

 while (cur) {
  struct fstab_entry *fse = cur->value;

  if (fse) {
   if (fse->status & device_status_mounted) {
    char tmp[BUFFERSIZE];
    char *tset = set2str (',', (const char **)fse->options); 

    if (tset)
     esprintf (tmp, BUFFERSIZE, "%s %s %s %s,%s 0 0\n", fse->adevice, fse->mountpoint, fse->afs,
#ifdef MS_RDONLY
               fse->aflags & MS_RDONLY
#else
                 0
#endif
                 ? "ro" : "rw", tset);
    else
     esprintf (tmp, BUFFERSIZE, "%s %s %s %s 0 0\n", fse->adevice, fse->mountpoint, fse->afs,
#ifdef MS_RDONLY
               fse->aflags & MS_RDONLY
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

  cur = streenext (cur);
 }

 return ret;
}

/* --------- enabling and disabling --------------------------------------- */
int einit_mount_enable (enum mounttask p, struct einit_event *status) {
// struct einit_event feedback = ei_module_feedback_default;
 struct stree *ha = mcb.fstab, *fsi = NULL;
 struct fstab_entry *fse;
 char **candidates = NULL;
 uint32_t ret, sc = 0, slc;

 if (coremode & einit_mode_sandbox) return status_ok;

 switch (p) {
  case MOUNT_LOCAL:
  case MOUNT_REMOTE:
   while (ha) {
    if (!inset ((const void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING) &&
         strcmp (ha->key, "/") && strcmp (ha->key, "/dev") &&
         strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if ((fse = (struct fstab_entry *)ha->value)) {
      if (fse->status & device_status_mounted)
       goto mount_skip;
      if (fse->mountflags & (mount_fstab_noauto | mount_fstab_critical))
       goto mount_skip;

      if (fse->fs && mcb.filesystems && (fsi = streefind (mcb.filesystems, fse->fs, tree_find_first))) {
       if (p == MOUNT_LOCAL) {
        if ((uintptr_t)fsi->value & filesystem_capability_network) goto mount_skip;
       } else {
        if (!((uintptr_t)fsi->value & filesystem_capability_network)) goto mount_skip;
       }
      } else if (p == MOUNT_REMOTE) {
       goto mount_skip;
      }
     }
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
         }
    mount_skip:
      ha = streenext (ha);
   }
   break;
  case MOUNT_SYSTEM:
#ifdef LINUX
   ret = emount ("/proc", status);
   einit_mount_update (UPDATE_MTAB);
   ret = emount ("/sys", status);
#endif
   ret = emount ("/dev", status);
   if (mcb.update_options & mount_update_block_devices) {
    status->string = "re-scanning block devices";
    status_update (status);
    einit_mount_update (UPDATE_BLOCK_DEVICES);
   }

   return ret;
   break;
  case MOUNT_CRITICAL:
   /*   ret = emount ("/", status);*/
   unlink ("/etc/mtab");
   update_real_mtab();

   while (ha) {
    if ((fse = (struct fstab_entry *)ha->value) && (fse->mountflags & mount_fstab_critical))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
    else if (inset ((const void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = streenext (ha);
   }
   break;
  default:
   status->string = "I'm clueless?";
   status_update (status);
   return status_failed;
   break;
 }

// setsort ((void **)candidates, SORT_SET_STRING_LEXICAL, NULL);

 if (!candidates) {
  status->string = "nothing to be done";
  status_update (status);
 }


/* this next loop will shift stuff around so that every time the process gets to the if (acand) step, acand will
 contain all the mountpoints with the next-higher number of /-s inside. this is to make sure that deeper paths
 will be mounted after lower paths, no matter how they were sorted in the fstab-stree */
 while (candidates) {
  uint32_t c = 0, sin = 0;
  char **acand = NULL;

  for (c = 0; candidates[c]; c++) {
   slc = 0;
   for (sin = 0; candidates[c][sin]; sin++)
    if (candidates[c][sin] == '/')
     slc++;
   if (slc == sc)
    acand = (char **)setadd ((void **)acand, (void *)candidates[c], SET_NOALLOC);
  }

  if (acand)
   for (c = 0; acand[c]; c++) {
   candidates = (char **)setdel ((void **)candidates, (void *)acand[c]);
   if (emount (acand[c], status) != status_ok)
    status->flag++;
   }

   free (acand);
   sc++;
 }

 struct einit_event rev = evstaticinit(einit_mount_new_mount_level);
 rev.integer = p;
 event_emit (&rev, einit_event_flag_broadcast);
 evstaticdestroy (rev);

// scan for new modules after mounting all critical filesystems
// if (p == MOUNT_CRITICAL) mod_scanmodules();

 update_real_mtab();

 return status_ok;
}

int einit_mount_disable (enum mounttask p, struct einit_event *status) {
// return status_ok;
 struct stree *ha;
 struct stree *fsi;
 struct fstab_entry *fse = NULL;
 char **candidates = NULL;
 uint32_t sc = 0, slc;

 if (coremode & einit_mode_sandbox) return status_ok;

 einit_mount_update (UPDATE_MTAB);
 ha = mcb.fstab;

 switch (p) {
  case MOUNT_REMOTE:
  case MOUNT_LOCAL:
   while (ha) {
    if (!inset ((const void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING) && strcmp (ha->key, "/") && strcmp (ha->key, "/dev") && strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if ((fse = (struct fstab_entry *)ha->value)) {
      if (!(fse->status & device_status_mounted)) goto mount_skip;

      if (p == MOUNT_LOCAL) {
       if (fse->afs) {
        if (mcb.filesystems && (fsi = streefind (mcb.filesystems, fse->afs, tree_find_first)) && ((uintptr_t)fsi->value & filesystem_capability_network)) goto mount_skip;
       }
      } else if (p == MOUNT_REMOTE) {
       if (fse->afs && mcb.filesystems && (fsi = streefind (mcb.filesystems, fse->afs, tree_find_first))) {
        if (!((uintptr_t)fsi->value & filesystem_capability_network)) goto mount_skip;
       } else goto mount_skip;
      }
     }
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
    }
    mount_skip:
      ha = streenext (ha);
   }
   break;
  case MOUNT_SYSTEM:
  {
   eumount ("/dev", status);
#ifdef LINUX
   eumount ("/sys", status);
   eumount ("/proc", status);
#endif
   if (mcb.update_options & mount_update_block_devices) {
//     status->string = "re-scanning block devices";
//     status_update (status);
// things go weird if this if enabled:
//     update (UPDATE_BLOCK_DEVICES);
   }
  }

#ifdef LINUX
  /* link /etc/mtab to /proc/mounts so that after / is r/o things are still up to date*/
  unlink ("/etc/mtab");
  symlink ("/proc/mounts", "/etc/mtab");
  /* discard errors, they are irrelevant. */
  errno = 0;
#endif

  return status_ok;
//   return status_ok;
  case MOUNT_CRITICAL:
   while (ha) {
    if (inset ((const void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = streenext (ha);
   }

   break;
  default:
   status->string = "come again?";
   status_update (status);
   return status_failed;
   break;
 }

 while (candidates) {
  uint32_t c, sin;
  char **acand;

  retry:
    c = 0;
  sin = 0;
  acand = NULL;

  for (c = 0; candidates[c]; c++) {
   slc = 0;
   for (sin = 0; candidates[c][sin]; sin++)
    if (candidates[c][sin] == '/')
     slc++;
   if (slc > sc) {
    sc = slc;
    goto retry;
   }
   else if (slc == sc)
    acand = (char **)setadd ((void **)acand, (void *)candidates[c], SET_NOALLOC);
  }

  if (acand)
   for (c = 0; acand[c]; c++) {
   candidates = (char **)setdel ((void **)candidates, (void *)acand[c]);
   if (eumount (acand[c], status) != status_ok)
    status->flag++;
   }

   free (acand);
   sc--;
 }

 update_real_mtab();

/* if (p == MOUNT_CRITICAL) {
 eumount ("/", status);
}*/

 return status_ok;
}

void einit_mount_update_configuration () {
 struct cfgnode *node = NULL;

 read_filesystem_flags_from_configuration (NULL);

 if ((node = cfg_findnode ("configuration-storage-update-steps",0,NULL)) && node->svalue) {
  char **tmp = str2set(':', node->svalue);
  uint32_t c = 0;
  mcb.update_options = mount_update_fstab + mount_update_mtab;
  for (; tmp[c]; c++) {
   if (strmatch (tmp[c], "metadata")) mcb.update_options |= mount_update_metadata;
   else if (strmatch (tmp[c], "block-devices")) mcb.update_options |= mount_update_block_devices;
  }
  free (tmp);
 }

 if ((node = cfg_findnode ("configuration-storage-mountpoints-critical",0,NULL)) && node->svalue)
  mcb.critical = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-mountpoints-no-umount",0,NULL)) && node->svalue)
  mcb.noumount = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-fsck-command",0,NULL)) && node->svalue)
  fsck_command = estrdup(node->svalue);

 if ((node = cfg_getnode ("configuration-storage-maintain-mtab",NULL)) && node->flag && node->svalue) {
  mcb.options |= mount_maintain_mtab;
  mcb.mtab_file = node->svalue;
 }

 if (mcb.update_options & mount_update_block_devices) {
  einit_mount_update (UPDATE_BLOCK_DEVICES);
  if (!(mcb.update_options & mount_update_metadata)) {
   update_fstab();
  }
 } else update_fstab();

 update_mtab();
}

void einit_mount_einit_event_handler (struct einit_event *ev) {
 if ((ev->type == einit_core_configuration_update) || (ev->type == einit_core_module_list_update_complete)) {
  einit_mount_update_configuration();
 }
}

int einit_mount_configure (struct lmodule *r) {
 module_init (r);

 thismodule->scanmodules = einit_mount_scanmodules;
 thismodule->cleanup = einit_mount_cleanup;
 thismodule->enable = (int (*)(void *, struct einit_event *))einit_mount_enable;
 thismodule->disable = (int (*)(void *, struct einit_event *))einit_mount_disable;

 /* pexec configuration */
 exec_configure (this);

 event_listen (einit_event_subsystem_ipc, einit_mount_mount_ipc_handler);
 event_listen (einit_event_subsystem_mount, einit_mount_mount_update_handler);
 event_listen (einit_event_subsystem_core, einit_mount_einit_event_handler);

 function_register ("find-block-devices-dev", 1, (void *)find_block_devices_recurse_path);
 function_register ("read-fstab-label", 1, (void *)forge_fstab_by_label);
 function_register ("read-fstab-configuration", 1, (void *)read_fstab_from_configuration);
 function_register ("read-fstab-legacy", 1, (void *)read_fstab);
 function_register ("read-mtab-legacy", 1, (void *)read_mtab);
 function_register ("fs-mount", 1, (void *)emount);
 function_register ("fs-umount", 1, (void *)eumount);

 einit_mount_update_configuration();

 return 0;
}

#else

int emount (char *, struct einit_event *);
int eumount (char *, struct einit_event *);
int mount_mount (char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_umount (char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_do_mount_generic (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int mount_do_umount_generic (char *, char *, char, struct device_data *, struct mountpoint_data *, struct einit_event *);
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

pthread_mutex_t
 mount_fs_mutex = PTHREAD_MUTEX_INITIALIZER,
 mount_device_data_mutex = PTHREAD_MUTEX_INITIALIZER,
 mounter_dd_by_devicefile_mutex = PTHREAD_MUTEX_INITIALIZER,
 mounter_dd_by_mountpoint_mutex = PTHREAD_MUTEX_INITIALIZER;

struct stree *mount_filesystems = NULL;

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

void mount_update_device (struct device_data *d) {
}

void mount_add_update_fstab_data (struct device_data *dd, char *mountpoint, char *fs, char **options, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, char **variables, uint32_t mountflags) {
 struct stree *st = (dd->mountpoints ? streefind (dd->mountpoints, mountpoint, tree_find_first) : NULL);
 struct mountpoint_data *mp = st ? st->value : ecalloc (1, sizeof (struct mountpoint_data));
 char *device = dd->device;

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

 if (st) {
  eprintf (stderr, " >> have mountpoint_data node for %s, device %s: updating\n", mountpoint, device);
 } else {
  eprintf (stderr, " >> inserting new mountpoint_data node for %s, device %s\n", mountpoint, device);

  dd->mountpoints = streeadd (dd->mountpoints, mountpoint, mp, SET_NOALLOC, mp);

  emutex_lock (&mounter_dd_by_mountpoint_mutex);
  mounter_dd_by_mountpoint =
    streeadd (mounter_dd_by_mountpoint, mountpoint, dd, SET_NOALLOC, NULL);
  emutex_unlock (&mounter_dd_by_mountpoint_mutex);
 }
}

void mount_add_update_fstab (char *mountpoint, char *device, char *fs, char **options, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, char **variables, uint32_t mountflags) {
 struct device_data *dd = NULL;
 struct stree *t;

 if (!fs) fs = estrdup ("auto");

 emutex_lock (&mounter_dd_by_mountpoint_mutex);
 if (mounter_dd_by_mountpoint && (t = streefind (mounter_dd_by_mountpoint, mountpoint, tree_find_first))) {
  dd = t->value;
 }
 emutex_unlock (&mounter_dd_by_mountpoint_mutex);

 if (!dd && (device || (device = fs) || (device = "(none)"))) {
  emutex_lock (&mounter_dd_by_devicefile_mutex);
  if (mounter_dd_by_devicefile && (t = streefind (mounter_dd_by_devicefile, device, tree_find_first))) {
   dd = t->value;
  }
 }
 emutex_unlock (&mounter_dd_by_devicefile_mutex);

 if (dd) {
  mount_add_update_fstab_data (dd, mountpoint, fs, options, before_mount, after_mount, before_umount, after_umount, manager, variables, mountflags);
 } else {
  struct device_data *d = emalloc(sizeof(struct device_data));
  uint32_t y = 0;

  memset (d, 0, sizeof(struct device_data));

  if (device || (device = fs) || (device = "(none)")) d->device = estrdup (device);

  eprintf (stderr, " >> inserting new device_data node for %s, device %s\n", mountpoint, device);

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
   }

   if (mountpoint) mount_add_update_fstab (mountpoint, device, fs, options, before_mount, after_mount, before_umount, after_umount, manager, variables, mountflags);

//   add_fstab_entry (mountpoint, device, fs, options, mountflags, before_mount, after_mount, before_umount, after_umount, manager, 1, variables);
  }
 }
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

 mount_update_fstab_nodes ();

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

 ssystem = (char **)setadd ((void **)ssystem, (void *)"fs-root", SET_TYPE_STRING);
 scritical = (char **)setadd ((void **)scritical, (void *)"mount-system", SET_TYPE_STRING);
 slocal = (char **)setadd ((void **)slocal, (void *)"mount-critical", SET_TYPE_STRING);
 sremote = (char **)setadd ((void **)sremote, (void *)"mount-critical", SET_TYPE_STRING);

 emutex_lock (&mounter_dd_by_mountpoint_mutex);

 s = mounter_dd_by_mountpoint;
 while (s) {
  char *servicename = mount_mp_to_service_name(s->key);
  char tmp[BUFFERSIZE];
  char **after = NULL;
  struct lmodule *lm = ml;

  if (strcmp (s->key, "/")) {
   char *tmpx = NULL;
   char **tmp_split = s->key[0] == '/' ? str2set ('/', s->key+1) : str2set ('/', s->key), **tmpxt = NULL;
   uint32_t r = 0;

   for (; tmp_split[r]; r++);

   for (r--; tmp_split[r] && r > 0; r--) {
    tmp_split[r] = 0;
    char *comb = set2str ('-', (const char **)tmp_split);

    tmpxt = (char **)setadd ((void **)tmpxt, (void *)comb, SET_TYPE_STRING);
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

   if (mp && !inset ((const void **)mp->options, "noauto", SET_TYPE_STRING)) {
    if (inset ((const void **)mount_system, s->key, SET_TYPE_STRING)) {
     ssystem = (char **)setadd ((void **)ssystem, (void *)servicename, SET_TYPE_STRING);
    } else if (inset ((const void **)mount_critical, s->key, SET_TYPE_STRING)) {
     scritical = (char **)setadd ((void **)scritical, (void *)servicename, SET_TYPE_STRING);
    } else {
     char ad = 0;

     if (inset ((const void **)mp->options, "critical", SET_TYPE_STRING)) {
      scritical = (char **)setadd ((void **)scritical, (void *)servicename, SET_TYPE_STRING);
      ad = 1;
     }

     if (inset ((const void **)mp->options, "system", SET_TYPE_STRING)) {
      ssystem = (char **)setadd ((void **)ssystem, (void *)servicename, SET_TYPE_STRING);
      ad = 1;
     }

     if (!ad) {
      enum filesystem_capability capa = mount_get_filesystem_options (((struct device_data *)(s->value))->fs);

      if (capa & filesystem_capability_network) {
       sremote = (char **)setadd ((void **)sremote, (void *)servicename, SET_TYPE_STRING);
       ad = 1;
      }
     }

     if (!ad) {
      slocal = (char **)setadd ((void **)slocal, (void *)servicename, SET_TYPE_STRING);
     }
    }
   }
  }

  esprintf (tmp, BUFFERSIZE, "mount-%s", s->key);

  while (lm) {
   if (lm->source && strmatch(lm->source, tmp)) {
    struct smodule *sm = (struct smodule *)lm->module;
    sm->si.after = after;

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

    return status_ok;
   }
  }
  free (functions);
 }

 free (fnames);

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
    return status_ok;
   }
  }
  free (functions);
 }

 free (fnames);

 return status_failed;

#if 0
 struct stree *he = mcb.fstab;
 struct fstab_entry *fse = NULL;

 if (!mountpoint) return status_failed;

 notice (4, "unmounting %s", mountpoint);

 if (coremode & einit_mode_sandbox) return status_ok;

 char textbuffer[BUFFERSIZE];
 errno = 0;
 uint32_t retry = 0;

 if (inset ((const void **)mcb.noumount, (void *)mountpoint, SET_TYPE_STRING)) return status_ok;

 if (he && (he = streefind (he, mountpoint, tree_find_first))) fse = (struct fstab_entry *)he->value;

 if (fse && !(fse->status & device_status_mounted))
  esprintf (textbuffer, BUFFERSIZE, "unmounting %s: seems not to be mounted", mountpoint);
 else
  esprintf (textbuffer, BUFFERSIZE, "unmounting %s", mountpoint);

 if (fse && fse->manager)
  stopdaemon (fse->manager, status);

 status->string = textbuffer;
 status_update (status);

 while (1) {
  retry++;

#if defined(DARWIN) || defined(__FreeBSD__)
  if (unmount (mountpoint, 0) != -1)
#else
   if (umount (mountpoint) != -1)
#endif
  {
   goto umount_ok;
  } else {
   struct pc_conditional pcc = {.match = "cwd-below", .para = mountpoint, .match_options = einit_pmo_additive},
   pcf = {.match = "files-below", .para = mountpoint, .match_options = einit_pmo_additive},
   *pcl[3] = { &pcc, &pcf, NULL };

   esprintf (textbuffer, BUFFERSIZE, "%s#%i: umount() failed: %s", mountpoint, retry, strerror(errno));
   errno = 0;
   status->string = textbuffer;
   status_update (status);

   pekill (pcl);
#ifdef LINUX
   if (retry >= 2) {
    if (umount2 (mountpoint, MNT_FORCE) != -1) {
     goto umount_ok;
    } else {
     esprintf (textbuffer, BUFFERSIZE, "%s#%i: umount2() failed: %s", mountpoint, retry, strerror(errno));
     errno = 0;
     status->string = textbuffer;
     status_update (status);
    }

    if (fse) {
     if (retry >= 3) {
      if (mount (fse->adevice, mountpoint, fse->afs, MS_REMOUNT | MS_RDONLY, NULL) == -1) {
       esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounting r/o failed: %s", mountpoint, retry, strerror(errno));
       errno = 0;
       status->string = textbuffer;
       status_update (status);
       goto umount_fail;
      } else {
       if (umount2 (mountpoint, MNT_DETACH) == -1) {
        esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounted r/o but detaching failed: %s", mountpoint, retry, strerror(errno));
        errno = 0;
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       } else {
        esprintf (textbuffer, BUFFERSIZE, "%s#%i: remounted r/o and detached", mountpoint, retry);
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       }
      }
     }
    } else {
     esprintf (textbuffer, BUFFERSIZE, "%s#%i: device mounted but I don't know anything more; bailing out", mountpoint, retry);
     status->string = textbuffer;
     status_update (status);
     goto umount_fail;
    }
   }
#else
   goto umount_fail;
#endif
  }

  umount_fail:

    status->flag++;
  if (retry > 3) {
   return status_failed;
  }
  sleep (1);
 }

 umount_ok:
   if (!(coremode & einit_mode_sandbox)) {
  if (fse && fse->after_umount)
   pexec_v1 (fse->after_umount, (const char **)fse->variables, NULL, status);
   }
   if (fse && (fse->status & device_status_mounted))
    fse->status ^= device_status_mounted;

   struct einit_event eem = evstaticinit (einit_mount_node_unmounted);
   eem.string = mountpoint;
   event_emit (&eem, einit_event_flag_broadcast);
   evstaticdestroy (eem);

   return status_ok;
#endif
 return status_ok;
}

int mount_mount (char *mountpoint, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 if (!(coremode & einit_mode_sandbox)) {
  if (dd->device_status & (device_status_dirty | device_status_error_notint))
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

 while ((step < 3) && (retval == status_failed)) {
  retval = mount_try_umount (mountpoint, mp->fs, step, dd, mp, status);
 }

 return status_failed;
}

int mount_fsck (char *fs, char *device, struct einit_event *status) {
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

 return status_ok;
}

int mount_do_umount_generic (char *mountpoint, char *fs, char step, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {

 fbprintf (status, "unmounting %s from %s (fs=%s, attempt #%i)", dd->device, mountpoint, fs, step);

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

 status->flag++;
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

 return status_ok;
}

int emount (char *mountpoint, struct einit_event *status) {
 return status_ok;

 struct device_data *dd = mount_get_device_data (mountpoint, NULL);
 if (dd && dd->mountpoints) {
  struct stree *t = streefind (dd->mountpoints, mountpoint, tree_find_first);

  if (t) {
   struct mountpoint_data *mp = t->value;

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
 return status_ok;

 struct device_data *dd = mount_get_device_data (mountpoint, NULL);
 if (dd && dd->mountpoints) {
  struct stree *t = streefind (dd->mountpoints, mountpoint, tree_find_first);

  if (t) {
   struct mountpoint_data *mp = t->value;

   return mount_umount (mountpoint, dd, mp, status);
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

int einit_mount_configure (struct lmodule *r) {
 module_init (r);

 thismodule->scanmodules = einit_mount_scanmodules;
 thismodule->cleanup = einit_mount_cleanup;

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

 return 0;
}
#endif
