/*
 *  einit-mount.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 07/06/2006.
 *  Renamed from common-mount.c on 11/10/2006.
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

#define _MODULE
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
#include <sys/mount.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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

/* module definitions */
// char *provides[] = {"mount", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_LOADER,
	.options	= 0,
	.name		= "Filesystem-Mounter",
	.rid		= "einit-mount",
    .si           = {
        .provides = NULL,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

char *provides_mountlocal[] = {"mount/local", NULL};
char *requires_mountlocal[] = {"mount/system", "mount/critical", NULL};
struct smodule sm_mountlocal = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (local)",
	.rid		= "einit-mount-local",
    .si           = {
        .provides = provides_mountlocal,
        .requires = requires_mountlocal,
        .after    = NULL,
        .before   = NULL
    }
};

char *provides_mountremote[] = { "mount/remote", NULL};
char *requires_mountremote[] = { "mount/system", "network", NULL};
char *after_mountremote[] = { "portmap", NULL};
struct smodule sm_mountremote = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (remote)",
	.rid		= "einit-mount-remote",
    .si           = {
        .provides = provides_mountremote,
        .requires = requires_mountremote,
        .after    = after_mountremote,
        .before   = NULL
    }
};

char *provides_system[] = {"mount/system", NULL};
struct smodule sm_system = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (system)",
	.rid		= "einit-mount-system",
    .si           = {
        .provides = provides_system,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

char *provides_critical[] = {"mount/critical", NULL};
char *requires_critical[] = {"mount/system", NULL};
struct smodule sm_critical = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (critical)",
	.rid		= "einit-mount-critical",
    .si           = {
        .provides = provides_critical,
        .requires = requires_critical,
        .after    = NULL,
        .before   = NULL
    }
};

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
	 .update_options	= EVENT_UPDATE_METADATA + EVENT_UPDATE_BLOCK_DEVICES + EVENT_UPDATE_FSTAB + EVENT_UPDATE_MTAB,
	 .critical		= NULL,
	 .noumount		= NULL
};

/* function declarations */
int scanmodules (struct lmodule *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);
int enable (enum mounttask, struct einit_event *);
int disable (enum mounttask, struct einit_event *);
int mountwrapper (char *, struct einit_event *, uint32_t);
char *__options_string_to_mountflags (char **, unsigned long *, char *);
void einit_event_handler (struct einit_event *);

char *generate_legacy_mtab (struct mount_control_block *);

/* function definitions */

/* the actual module */

int configure (struct lmodule *this) {
 struct cfgnode *node = NULL;

/* pexec configuration */
 exec_configure (this);

 event_listen (EVENT_SUBSYSTEM_IPC, mount_ipc_handler);
 event_listen (EVENT_SUBSYSTEM_MOUNT, mount_update_handler);
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);

 read_filesystem_flags_from_configuration (NULL);

 function_register ("find-block-devices-dev", 1, (void *)find_block_devices_recurse_path);
 function_register ("read-fstab-label", 1, (void *)forge_fstab_by_label);
 function_register ("read-fstab-configuration", 1, (void *)read_fstab_from_configuration);
 function_register ("read-fstab-legacy", 1, (void *)read_fstab);
 function_register ("read-mtab-legacy", 1, (void *)read_mtab);
 function_register ("fs-mount", 1, (void *)mountwrapper);

 if ((node = cfg_findnode ("configuration-storage-update-steps",0,NULL)) && node->svalue) {
  char **tmp = str2set(':', node->svalue);
  uint32_t c = 0;
  mcb.update_options = EVENT_UPDATE_FSTAB + EVENT_UPDATE_MTAB;
  for (; tmp[c]; c++) {
   if (!strcmp (tmp[c], "metadata")) mcb.update_options |= EVENT_UPDATE_METADATA;
   else if (!strcmp (tmp[c], "block-devices")) mcb.update_options |= EVENT_UPDATE_BLOCK_DEVICES;
  }
  free (tmp);
 }

 if ((node = cfg_findnode ("configuration-storage-mountpoints-critical",0,NULL)) && node->svalue)
  mcb.critical = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-mountpoints-no-umount",0,NULL)) && node->svalue)
  mcb.noumount = str2set(':', node->svalue);

 if ((node = cfg_findnode ("configuration-storage-fsck-command",0,NULL)) && node->svalue)
  fsck_command = estrdup(node->svalue);

 if (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES) {
  update (UPDATE_BLOCK_DEVICES);
  if (!(mcb.update_options & EVENT_UPDATE_METADATA)) {
   update_fstab();
  }
 } else update_fstab();
 update_mtab();

 return 0;
}

int cleanup (struct lmodule *this) {
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
 function_unregister ("fs-mount", 1, (void *)mountwrapper);

 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_ignore (EVENT_SUBSYSTEM_IPC, mount_ipc_handler);
 event_ignore (EVENT_SUBSYSTEM_MOUNT, mount_update_handler);

 if (fsck_command) {
  free (fsck_command);
  fsck_command = NULL;
 }

 if (mcb.critical) {
  free (mcb.critical);
  mcb.critical = NULL;
 }

 exec_cleanup(this);
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
   uint32_t err;
   if (!(err = regcomp (&devpattern, npattern, REG_EXTENDED)))
    havedevpattern = 1;
   else {
    char errorcode [1024];
    regerror (err, &devpattern, errorcode, 1024);
    fputs (errorcode, stdout);
   }
  }
 }
#endif

 dir = opendir (path);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
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
    if (S_ISBLK (statbuf.st_mode) && (!havedevpattern || !regexec (&devpattern, tmp, 0, NULL, 0))) {
#else
    if (S_ISBLK (statbuf.st_mode)) {
#endif
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
  closedir (dir);
 } else {
  fprintf (stdout, "einit-common-mount: could not open %s\n", path);
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
 if (node = cfg_findnode ("hostname", 0, node)) {
  hostname = node->svalue;
 } else if (node = cfg_findnode ("conf_hostname", 0, node)) {
  hostname = node->svalue;
 } else {
  hostname = "einit";
 }
 hnl = strlen (hostname);

// puts (".");

 while (element) {
  struct bd_info *bdi = element->value;
  if (bdi) {
   if ((bdi->status & BF_STATUS_HAS_MEDIUM) && !(bdi->status & BF_STATUS_ERROR)) {
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
       else if (!strcmp (cur, "-root")) cur = "/";
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
      char tmp[1024] = "/media/";
      strcat (tmp, bdi->label);
      mpoint = estrdup (tmp);
     }
    } else {
     char tmp[1024] = "/media";
     strcat (tmp, element->key);
     mpoint = estrdup (tmp);
    }

//    printf ("%s: %s, %s\n", mpoint, element->key, fsname);
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
// puts ("adding fstab node");
 while (node = cfg_findnode ("configuration-storage-fstab-node", 0, node)) {
  char *mountpoint = NULL, *device = NULL, *fs = NULL, **options = NULL, *before_mount = NULL, *after_mount = NULL, *before_umount = NULL, *after_umount = NULL, *manager = NULL, **variables = NULL;
  uint32_t mountflags = 0;

  if (node->arbattrs) {
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (!strcmp(node->arbattrs[i], "mountpoint"))
     mountpoint = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "device"))
     device = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "fs"))
     fs = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "options"))
     options = str2set (':', node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "before-mount"))
     before_mount = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "after-mount"))
     after_mount = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "before-umount"))
     before_umount = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "after-umount"))
     after_umount = estrdup (node->arbattrs[i+1]);
    else if (!strcmp(node->arbattrs[i], "manager")) {
     manager = estrdup (node->arbattrs[i+1]);
    } else if (!strcmp(node->arbattrs[i], "variables"))
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

 if (fp = fopen (file, "r")) {
  char buffer[1024];
  errno = 0;
  while (!errno) {
   if (!fgets (buffer, 1024, fp)) {
    switch (errno) {
     case EINTR:
     case EAGAIN:
      errno = 0;
      break;
     case 0:
      goto done_parsing_file;
     default:
      bitch(BTCH_ERRNO);
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
  fclose (fp);
 }

 return workstree;
}

unsigned char read_filesystem_flags_from_configuration (void *na) {
 struct cfgnode *node = NULL;
 uint32_t i, j;
 char *id, *flags;
 while (node = cfg_findnode ("information-filesystem-type", 0, node)) {
  if (node->arbattrs) {
   id = NULL;
   flags = 0;
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (!strcmp (node->arbattrs[i], "id"))
     id = node->arbattrs[i+1];
    else if (!strcmp (node->arbattrs[i], "flags"))
     flags = node->arbattrs[i+1];
   }
//   printf (" %s=%i", id, flags);
   add_filesystem (id, flags);
  }
 }
 return 0;
}

void update (enum update_task task) {
 struct cfgnode *node = NULL;
 char **fl = NULL;
 void **functions = NULL;
 char *flb = NULL;
 uint32_t i = 0;
 void (*f)(struct mount_control_block *);

 switch (task) {
  case UPDATE_METADATA:
   if (!(mcb.update_options & EVENT_UPDATE_METADATA)) return;
   node = cfg_findnode ("configuration-storage-filesystem-label-readers", 0, NULL);
   fl = defaultfilesystems;
   flb = "fs-read-metadata";
   break;
  case UPDATE_BLOCK_DEVICES:
   if (!(mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) return;
   node = cfg_findnode ("configuration-storage-block-devices-source", 0, NULL);
   fl = defaultblockdevicesource;
   flb = "find-block-devices";
   break;
  case UPDATE_FSTAB:
   if (!(mcb.update_options & EVENT_UPDATE_FSTAB)) return;
   node = cfg_findnode ("configuration-storage-fstab-source", 0, NULL);
   fl = defaultfstabsource;
   flb = "read-fstab";
   break;
  case UPDATE_MTAB:
   if (!(mcb.update_options & EVENT_UPDATE_MTAB)) return;
   node = cfg_findnode ("configuration-storage-mtab-source", 0, NULL);
   fl = defaultmtabsource;
   flb = "read-mtab";
   break;
 }

 if (node && node->svalue)
  fl = str2set (':', node->svalue);

 functions = function_find (flb, 1, fl);
 if (functions && functions[0]) {
  for (; functions[i]; i++) {
   f = functions[i];
   f (&mcb);
  }
  free (functions);
 }
#ifdef BITCHY
 else {
  printf ("no functions for update(%s)", flb);
 }
#endif

 switch (task) {
  case UPDATE_METADATA:
   if (fl != defaultfilesystems) free (fl);
   if (mcb.update_options & EVENT_UPDATE_FSTAB) update (UPDATE_FSTAB);
   break;
  case UPDATE_BLOCK_DEVICES:
   if (fl != defaultblockdevicesource) free (fl);
   if (mcb.update_options & EVENT_UPDATE_METADATA) update (UPDATE_METADATA);
   break;
  case UPDATE_FSTAB:
   if (fl != defaultfstabsource) free (fl);
   break;
  case UPDATE_MTAB:
   if (fl != defaultmtabsource) free (fl);
   break;
 }
}

int scanmodules (struct lmodule *modchain) {
 struct lmodule *new,
                *lm = modchain;
 char doop = 1;

 while (lm) { if (lm->source && !strcmp(lm->source, sm_mountlocal.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_mountlocal))) {
   new->source = new->module->rid;
   new->enable = (int (*)(void *, struct einit_event *))enable;
   new->disable = (int (*)(void *, struct einit_event *))disable;
   new->param = (void *)MOUNT_LOCAL;
  }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && !strcmp(lm->source, sm_mountremote.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_mountremote))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))enable;
  new->disable = (int (*)(void *, struct einit_event *))disable;
  new->param = (void *)MOUNT_REMOTE;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && !strcmp(lm->source, sm_system.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_system))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))enable;
  new->disable = (int (*)(void *, struct einit_event *))disable;
  new->param = (void *)MOUNT_SYSTEM;
 }

 doop = 1;
 lm = modchain;
 while (lm) { if (lm->source && !strcmp(lm->source, sm_critical.rid)) { doop = 0; lm = mod_update (lm); break; } lm = lm->next; }
 if (doop && (new = mod_add (NULL, &sm_critical))) {
  new->source = new->module->rid;
  new->enable = (int (*)(void *, struct einit_event *))enable;
  new->disable = (int (*)(void *, struct einit_event *))disable;
  new->param = (void *)MOUNT_CRITICAL;
 }
}

char *__options_string_to_mountflags (char **options, unsigned long *mntflags, char *mountpoint) {
 int fi = 0;
 char *ret = NULL;

 for (; options[fi]; fi++) {
#ifdef LINUX
  if (!strcmp (options[fi], "user") || !strcmp (options[fi], "users")) {
   fprintf (stderr, " >> node \"%s\": mount-flag \"%s\": this has no real meaning for eINIT except for implying noexec, nosuid and nodev; you should remove it.\n", mountpoint, options[fi]);

#ifdef MS_NOEXEC
   (*mntflags) |= MS_NOEXEC;
#endif
#ifdef MS_NODEV
   (*mntflags) |= MS_NODEV;
#endif
#ifdef MS_NOSUID
   (*mntflags) |= MS_NOSUID;
#endif
  } else if (!strcmp (options[fi], "owner")) {
   fprintf (stderr, " >> node \"%s\": mount-flag \"%s\": this has no real meaning for eINIT except for implying nosuid and nodev; you should remove it.\n", mountpoint, options[fi]);

#ifdef MS_NODEV
   (*mntflags) |= MS_NODEV;
#endif
#ifdef MS_NOSUID
   (*mntflags) |= MS_NOSUID;
#endif
  } else if (!strcmp (options[fi], "nouser") || !strcmp (options[fi], "group") || !strcmp (options[fi], "auto") || !strcmp (options[fi], "defaults")) {
   fprintf (stderr, " >> node \"%s\": ignored unsupported/irrelevant mount-flag \"%s\": it has no meaning for eINIT, you should remove it.\n", mountpoint, options[fi]);
  } else if (!strcmp (options[fi], "_netdev")) {
   fprintf (stderr, " >> node \"%s\": ignored unsupported/irrelevant mount-flag \"_netdev\": einit uses a table with filesystem data to find out if network access is required to mount a certain node, so you should rather modify that table than specify \"_netdev\".\n", mountpoint);
  } else
#endif

#ifdef MS_NOATIME
  if (!strcmp (options[fi], "noatime")) (*mntflags) |= MS_NOATIME;
  else if (!strcmp (options[fi], "atime")) (*mntflags) = ((*mntflags) & MS_NOATIME) ? (*mntflags) ^ MS_NOATIME : (*mntflags);
  else
#endif

#ifdef MS_NODEV
  if (!strcmp (options[fi], "nodev")) (*mntflags) |= MS_NODEV;
  else if (!strcmp (options[fi], "dev")) (*mntflags) = ((*mntflags) & MS_NODEV) ? (*mntflags) ^ MS_NODEV : (*mntflags);
  else
#endif

#ifdef MS_NODIRATIME
  if (!strcmp (options[fi], "nodiratime")) (*mntflags) |= MS_NODIRATIME;
  else if (!strcmp (options[fi], "diratime")) (*mntflags) = ((*mntflags) & MS_NODIRATIME) ? (*mntflags) ^ MS_NODIRATIME : (*mntflags);
  else
#endif

#ifdef MS_NOEXEC
  if (!strcmp (options[fi], "noexec")) (*mntflags) |= MS_NOEXEC;
  else if (!strcmp (options[fi], "exec")) (*mntflags) = ((*mntflags) & MS_NOEXEC) ? (*mntflags) ^ MS_NOEXEC : (*mntflags);
  else
#endif

#ifdef MS_NOSUID
  if (!strcmp (options[fi], "nosuid")) (*mntflags) |= MS_NOSUID;
  else if (!strcmp (options[fi], "suid")) (*mntflags) = ((*mntflags) & MS_NOSUID) ? (*mntflags) ^ MS_NOSUID : (*mntflags);
  else
#endif

#ifdef MS_DIRSYNC
  if (!strcmp (options[fi], "dirsync")) (*mntflags) |= MS_DIRSYNC;
  else if (!strcmp (options[fi], "nodirsync")) (*mntflags) = ((*mntflags) & MS_DIRSYNC) ? (*mntflags) ^ MS_DIRSYNC : (*mntflags);
  else
#endif

#ifdef MS_SYNCHRONOUS
  if (!strcmp (options[fi], "sync")) (*mntflags) |= MS_SYNCHRONOUS;
  else if (!strcmp (options[fi], "nosync")) (*mntflags) = ((*mntflags) & MS_SYNCHRONOUS) ? (*mntflags) ^ MS_SYNCHRONOUS : (*mntflags);
  else
#endif

#ifdef MS_MANDLOCK
  if (!strcmp (options[fi], "mand")) (*mntflags) |= MS_MANDLOCK;
  else if (!strcmp (options[fi], "nomand")) (*mntflags) = ((*mntflags) & MS_MANDLOCK) ? (*mntflags) ^ MS_MANDLOCK : (*mntflags);
  else
#endif

#ifdef MS_RDONLY
  if (!strcmp (options[fi], "ro")) (*mntflags) |= MS_RDONLY;
  else if (!strcmp (options[fi], "rw")) (*mntflags) = ((*mntflags) & MS_RDONLY) ? (*mntflags) ^ MS_RDONLY : (*mntflags);
  else
#endif

#ifdef MS_BIND
  if (!strcmp (options[fi], "bind")) (*mntflags) |= MS_BIND;
  else
#endif

#ifdef MS_REMOUNT
  if (!strcmp (options[fi], "remount")) (*mntflags) |= MS_REMOUNT;
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
int mountwrapper (char *mountpoint, struct einit_event *status, uint32_t tflags) {
 struct stree *he = mcb.fstab;
 struct stree *de = mcb.blockdevices;
 struct fstab_entry *fse = NULL;
 struct bd_info *bdi = NULL;
 char *source;
 char *fstype = NULL;
 char *fsdata = NULL;
 uint32_t fsntype;
 char verbosebuffer [1024];
 void **fs_mount_functions = NULL;
 char *fs_mount_function_name;
 unsigned char (*mount_function)(uint32_t, char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);

 unsigned long mntflags = 0;

 if (tflags & MOUNT_TF_MOUNT) {
  char **fstype_s = NULL;
  uint32_t fsts_i = 0;
  if ((he = streefind (he, mountpoint, TREE_FIND_FIRST)) && (fse = (struct fstab_entry *)he->value)) {
   source = fse->device;
   fsntype = 0;
   if ((de = streefind (de, source, TREE_FIND_FIRST)) && (bdi = (struct bd_info *)de->value)) {
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

   if (bdi && (bdi->status & BF_STATUS_DIRTY)) {
    if (fsck_command) {
     char tmp[1024];
     status->string = "filesystem dirty; running fsck";
     status_update (status);

     snprintf (tmp, 1024, fsck_command, fstype, de->key);
     if (gmode != EINIT_GMODE_SANDBOX) {
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

   if (!strcmp (fstype, "auto"))
    fstype = cfg_getstring ("configuration-storage-filesystem-guessing-order", NULL);

   fstype_s = str2set (':', fstype);

   mntflags = 0;
   if (fse->options)
    fsdata = __options_string_to_mountflags (fse->options, &mntflags, mountpoint);

   if (gmode != EINIT_GMODE_SANDBOX) {
    if (fse->before_mount)
     pexec_v1 (fse->before_mount, fse->variables, NULL, status);
   }

   if (fstype_s) for (; fstype_s[fsts_i]; fsts_i++) {
    fstype = fstype_s[fsts_i];

    if (bdi && bdi->label)
     snprintf (verbosebuffer, 1023, "mounting %s [%s; label=%s; fs=%s]", mountpoint, source, bdi->label, fstype);
    else
     snprintf (verbosebuffer, 1023, "mounting %s [%s; fs=%s]", mountpoint, source, fstype);
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
      if (!mount_function (tflags, source, mountpoint, fstype, bdi, fse, status)) {
       free (fs_mount_functions);
       goto mount_success;
      }
     }
     free (fs_mount_functions);
    }

    if (gmode != EINIT_GMODE_SANDBOX) {
// root node should only be remounted...
     if (!strcmp ("/", mountpoint)) goto attempt_remount;
#ifdef DARWIN
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
//       sleep(2);

       if (mount (source, mountpoint, fstype, MS_REMOUNT | mntflags, fsdata) == -1) {
        status->string = "remounting node failed...";
        status_update (status);
        goto mount_panic;
       }
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
        pexec_v1 (fse->after_umount, fse->variables, NULL, status);
//       return STATUS_FAIL;
        continue;
      }
     }
    }

    mount_success:

    fse->afs = fstype;
    fse->adevice = source;
    fse->aflags = mntflags;

    if (gmode != EINIT_GMODE_SANDBOX) {
     if (fse->after_mount)
      pexec_v1 (fse->after_mount, fse->variables, NULL, status);

     if (fse->manager)
      startdaemon (fse->manager, status);
    }

    struct einit_event eem = evstaticinit (EVENT_NODE_MOUNTED);
    eem.string = mountpoint;
    event_emit (&eem, EINIT_EVENT_FLAG_BROADCAST);
    evstaticdestroy (eem);

    fse->status |= BF_STATUS_MOUNTED;


    if (mcb.options & OPTION_MAINTAIN_MTAB) {
     char *tmpmtab = generate_legacy_mtab (&mcb);

     if (tmpmtab) {
      FILE *mtabfile = fopen (mcb.mtab_file, "w");

      if (mtabfile) {
       fputs (tmpmtab, mtabfile);
       fclose (mtabfile);
      }

      free (tmpmtab);
     }
    }

    return STATUS_OK;
   }

// we reach this if none of the attempts worked out
   if (fstype_s) free (fstype_s);
   return STATUS_FAIL;
  } else {
   status->string = "nothing known about this mountpoint; bailing out.";
   status_update (status);
   return STATUS_FAIL;
  }
 }
 if (tflags & MOUNT_TF_UMOUNT) {
  char textbuffer[1024];
  errno = 0;
  uint32_t retry = 0;

  if (inset ((void **)mcb.noumount, (void *)mountpoint, SET_TYPE_STRING)) return STATUS_OK;

  if (he = streefind (he, mountpoint, TREE_FIND_FIRST)) fse = (struct fstab_entry *)he->value;

  if (fse && !(fse->status & BF_STATUS_MOUNTED))
   snprintf (textbuffer, 1024, "unmounting %s: seems not to be mounted", mountpoint);
  else
   snprintf (textbuffer, 1024, "unmounting %s", mountpoint);

  if (fse && fse->manager)
   stopdaemon (fse->manager, status);

  status->string = textbuffer;
  status_update (status);

  while (1) {
   retry++;

//   if (gmode != EINIT_GMODE_SANDBOX) {
#ifdef DARWIN
    if (unmount (mountpoint, 0))
#else
    if (umount (mountpoint))
#endif
    {
     struct pc_conditional pcc = {.match = "cwd-below", .para = mountpoint, .match_options = PC_COLLECT_ADDITIVE},
                          *pcl[2] = { &pcc, NULL };

     snprintf (textbuffer, 1024, "%s#%i: umount() failed: %s", mountpoint, retry, strerror(errno));
     status->string = textbuffer;
     status_update (status);

     pekill (pcl);
#ifdef LINUX
     if (retry >= 2) {
      if (umount2 (mountpoint, MNT_FORCE)) {
       snprintf (textbuffer, 1024, "%s#%i: umount2() failed: %s", mountpoint, retry, strerror(errno));
       status->string = textbuffer;
       status_update (status);
      }

      if (fse) {
       if (retry >= 3) {
        if (mount (fse->adevice, mountpoint, fse->afs, MS_REMOUNT | MS_RDONLY, NULL)) {
         snprintf (textbuffer, 1024, "%s#%i: remounting r/o failed: %s", mountpoint, retry, strerror(errno));
         status->string = textbuffer;
         status_update (status);
         goto umount_fail;
        } else {
         if (umount2 (mountpoint, MNT_DETACH)) {
          snprintf (textbuffer, 1024, "%s#%i: remounted r/o but detaching failed: %s", mountpoint, retry, strerror(errno));
          status->string = textbuffer;
          status_update (status);
          goto umount_ok;
         } else {
          snprintf (textbuffer, 1024, "%s#%i: remounted r/o and detached", mountpoint, retry);
          status->string = textbuffer;
          status_update (status);
          goto umount_ok;
         }
        }
       }
      } else {
       snprintf (textbuffer, 1024, "%s#%i: device mounted but I don't know anything more; bailing out", mountpoint, retry);
       status->string = textbuffer;
       status_update (status);
       goto umount_fail;
      }
     }
#else
     goto umount_fail;
#endif
    }
/*   } else {
    goto umount_ok;
   }*/

   umount_fail:

   status->flag++;
   if (retry >= 3) {
    snprintf (textbuffer, 1024, "%s: attempt %i: failed, not retrying.", mountpoint, retry);
    status->string = textbuffer;
    status_update (status);
    return STATUS_FAIL;
   }
   sleep (1);
  }

  umount_ok:
  if (gmode != EINIT_GMODE_SANDBOX) {
   if (fse && fse->after_umount)
    pexec_v1 (fse->after_umount, fse->variables, NULL, status);
  }
  if (fse && (fse->status & BF_STATUS_MOUNTED))
   fse->status ^= BF_STATUS_MOUNTED;

  struct einit_event eem = evstaticinit (EVENT_NODE_UNMOUNTED);
  eem.string = mountpoint;
  event_emit (&eem, EINIT_EVENT_FLAG_BROADCAST);
  evstaticdestroy (eem);

  if (mcb.options & OPTION_MAINTAIN_MTAB) {
   char *tmpmtab = generate_legacy_mtab (&mcb);

   if (tmpmtab) {
    FILE *mtabfile = fopen (mcb.mtab_file, "w");

    if (mtabfile) {
     fputs (tmpmtab, mtabfile);
     fclose (mtabfile);
    }

    free (tmpmtab);
   }
  }

  return STATUS_OK;
 }
}

void add_block_device (char *devicefile, uint32_t major, uint32_t minor) {
 struct bd_info bdi;

 memset (&bdi, 0, sizeof (struct bd_info));

 bdi.major = major;
 bdi.minor = minor;
 bdi.status = BF_STATUS_HAS_MEDIUM | BF_STATUS_ERROR_NOTINIT;
 pthread_mutex_lock (&blockdevices_mutex);
 if (streefind (mcb.blockdevices, devicefile, TREE_FIND_FIRST)) {
  pthread_mutex_unlock (&blockdevices_mutex);
  return;
 }

 mcb.blockdevices = streeadd (mcb.blockdevices, devicefile, &bdi, sizeof (struct bd_info), NULL);
// mcb.blockdevices = streeadd (mcb.blockdevices, devicefile, bdi, -1);

 pthread_mutex_unlock (&blockdevices_mutex);
}

void add_fstab_entry (char *mountpoint, char *device, char *fs, char **options, uint32_t mountflags, char *before_mount, char *after_mount, char *before_umount, char *after_umount, char *manager, uint32_t manager_restart, char **variables) {
 struct fstab_entry fse;
 uint32_t i = 0;
 if (!mountpoint) return;

 memset (&fse, 0, sizeof (struct fstab_entry));

 fse.mountpoint = mountpoint;
 fse.device = device;
 fse.fs = fs;
 if (options) {
  char **noptions = NULL;
  for (i = 0; options[i]; i++) {
   if (!strcmp (options[i], "noauto")) mountflags |= MOUNT_FSTAB_NOAUTO;
   else if (!strcmp (options[i], "critical")) mountflags |= MOUNT_FSTAB_CRITICAL;
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

 pthread_mutex_lock (&fstab_mutex);
 if (streefind (mcb.fstab, mountpoint, TREE_FIND_FIRST)) {
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

  pthread_mutex_unlock (&fstab_mutex);
  return;
 }

 mcb.fstab = streeadd (mcb.fstab, mountpoint, &fse, sizeof (struct fstab_entry), fse.options);
 pthread_mutex_unlock (&fstab_mutex);
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

 pthread_mutex_lock (&fstab_mutex);

 if (cur = streefind (mcb.fstab, fs_file, TREE_FIND_FIRST)) {
  struct fstab_entry *node = cur->value;

  node->adevice = dset[1];
  node->aflags = inset ((void**)dset, (void*)"ro", SET_TYPE_STRING) ? MS_RDONLY : 0;
  node->afs = dset[2];

  node->status |= BF_STATUS_MOUNTED;
 } else {
  memset (&fse, 0, sizeof (struct fstab_entry));

  fse.status = BF_STATUS_MOUNTED;
  fse.device = dset[1];
  fse.adevice = dset[1];
  fse.mountpoint = dset[0];
  fse.aflags = inset ((void**)dset, (void*)"ro", SET_TYPE_STRING) ? MS_RDONLY : 0;
  fse.options = str2set (',', dset[3]);
  fse.afs = dset[2];
  fse.fs = dset[2];

  mcb.fstab = streeadd (mcb.fstab, fs_file, &fse, sizeof (struct fstab_entry), dset);
 }

 pthread_mutex_unlock (&fstab_mutex);
}

void add_filesystem (char *name, char *options) {
 char **t = str2set (':', options);
 uintptr_t flags = 0, i = 0;
 if (t) {
  for (; t[i]; i++) {
   if (!strcmp (t[i], "rw"))
    flags |= FS_CAPA_RW;
   else if (!strcmp (t[i], "volatile"))
    flags |= FS_CAPA_VOLATILE;
   else if (!strcmp (t[i], "network"))
    flags |= FS_CAPA_NETWORK;
   }
  free (t);
 }

 pthread_mutex_lock (&fs_mutex);
 if (streefind (mcb.filesystems, name, TREE_FIND_FIRST)) {
  pthread_mutex_unlock (&fs_mutex);
  return;
 }

 mcb.filesystems = streeadd (mcb.filesystems, name, (void *)flags, -1, NULL);
 pthread_mutex_unlock (&fs_mutex);
}

/* all the current IPC commands will be made #DEBUG-only, but we'll keep 'em for now */
/* --------- error checking and direct user interaction ------------------- */
void mount_ipc_handler(struct einit_event *ev) {
 if (!ev || !ev->set) return;
 char **argv = (char **) ev->set;
 if (argv[0] && argv[1]) {
#ifdef DEBUG
  if (!strcmp (argv[0], "list")) {
   if (!strcmp (argv[1], "fstab")) {
    char buffer[1024];
    struct stree *cur = ( mcb.fstab ? *(mcb.fstab->lbase) : NULL );
    struct fstab_entry *val = NULL;

    ev->flag = 1;

    while (cur) {
     val = (struct fstab_entry *) cur->value;
     if (val) {
      snprintf (buffer, 1023, "%s [spec=%s;vfstype=%s;flags=%i;before-mount=%s;after-mount=%s;before-umount=%s;after-umount=%s;status=%i]\n", val->mountpoint, val->device, val->fs, val->mountflags, val->before_mount, val->after_mount, val->before_umount, val->after_umount, val->status);
      fdputs (buffer, ev->integer);
     }
     cur = streenext (cur);
    }
   } else if (!strcmp (argv[1], "block-devices")) {
    char buffer[1024];
    struct stree *cur = mcb.blockdevices;
    struct bd_info *val = NULL;

    ev->flag = 1;

    while (cur) {
     val = (struct bd_info *) cur->value;
     if (val) {
      snprintf (buffer, 1023, "%s [fs=%s;type=%i;label=%s;uuid=%s;flags=%i]\n", cur->key, val->fs, val->fs_type, val->label, val->uuid, val->status);
      fdputs (buffer, ev->integer);
     }
     cur = streenext (cur);
    }
   } else if (!strcmp (argv[1], "mtab")) {
    char buffer[1024];
    struct stree *cur = mcb.mtab;
    struct legacy_fstab_entry *val = NULL;

    ev->flag = 1;

    while (cur) {
     val = (struct legacy_fstab_entry *) cur->value;
     if (val) {
      snprintf (buffer, 1023, "%s [spec=%s;vfstype=%s;mntops=%s;freq=%i;passno=%i]\n", val->fs_file, val->fs_spec, val->fs_vfstype, val->fs_mntops, val->fs_freq, val->fs_passno);
      fdputs (buffer, ev->integer);
     }
     cur = streenext (cur);
    }
   }
  } else
#endif
  if (!strcmp (argv[0], "examine") && !strcmp (argv[1], "configuration")) {
/* error checking... */
   char **tmpset, *tmpstring;

   ev->flag = 1;

   if (!(tmpstring = cfg_getstring("configuration-storage-fstab-source", NULL))) {
    fdputs (" * configuration variable \"configuration-storage-fstab-source\" not found.\n", ev->integer);
    ev->task++;
   } else {
    tmpset = str2set(':', tmpstring);

    if (inset ((void **)tmpset, (void *)"label", SET_TYPE_STRING)) {
     if (!(mcb.update_options & EVENT_UPDATE_METADATA)) {
      fdputs (" * fstab-source \"label\" to be used, but optional update-step \"metadata\" not enabled.\n", ev->integer);
      ev->task++;
     }
     if (!(mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) {
      fdputs (" * fstab-source \"label\" to be used, but optional update-step \"block-devices\" not enabled.\n", ev->integer);
      ev->task++;
     }
    }

    if (!inset ((void **)tmpset, (void *)"configuration", SET_TYPE_STRING)) {
     fdputs (" * fstab-source \"configuration\" disabled! In 99.999% of all cases, you don't want to do that!\n", ev->integer);
     ev->task++;
    }

    if (inset ((void **)tmpset, (void *)"legacy", SET_TYPE_STRING)) {
     fdputs (" * fstab-source \"legacy\" enabled; you shouldn't rely on that.\n", ev->integer);
     ev->task++;
    }

    free (tmpset);
   }

   if (mcb.update_options & EVENT_UPDATE_METADATA) {
    if (!(mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) {
     fdputs (" * update-step \"metadata\" cannot be performed without update-step \"block-devices\".\n", ev->integer);
     ev->task++;
    }
   } else {
    fdputs (" * update-step \"metadata\" disabled; not a problem per-se, but this will prevent your filesystems from being automatically fsck()'d.\n", ev->integer);
   }

   if (!mcb.fstab) {
    fdputs (" * your fstab is empty.\n", ev->integer);
    ev->task++;
   } else {
    struct stree *tstree, *fstree;
    if (!(tstree = streefind (mcb.fstab, "/", TREE_FIND_FIRST))) {
     fdputs (" * your fstab does not contain an entry for \"/\".\n", ev->integer);
     ev->task++;
    } else if (!(((struct fstab_entry *)(tstree->value))->device)) {
     fdputs (" * you have apparently forgotten to specify a device for your root-filesystem.\n", ev->integer);
     ev->task++;
    } else if (!strcmp ("/dev/ROOT", (((struct fstab_entry *)(tstree->value))->device))) {
     fdputs (" * you didn't edit your local.xml to specify your root-filesystem.\n", ev->integer);
     ev->task++;
    }

    tstree = mcb.fstab;
    while (tstree) {
     struct stat stbuf;

     if (!(((struct fstab_entry *)(tstree->value))->fs) || !strcmp ("auto", (((struct fstab_entry *)(tstree->value))->fs))) {
      char tmpstr[1024];
      if (inset ((void **)(((struct fstab_entry *)(tstree->value))->options), (void *)"bind", SET_TYPE_STRING)) {
#ifdef LINUX
       tstree = streenext (tstree);
       continue;
#else
       snprintf (tmpstr, 1024, " * supposed to bind-mount %s, but this OS seems to lack support for this.\n", tstree->key);
#endif
       fdputs (tmpstr, ev->integer);
       ev->task++;
      }
     }

     if (!(((struct fstab_entry *)(tstree->value))->device)) {
      if ((((struct fstab_entry *)(tstree->value))->fs) && (fstree = streefind (mcb.filesystems, (((struct fstab_entry *)(tstree->value))->fs), TREE_FIND_FIRST)) && !((uintptr_t)fstree->value & FS_CAPA_VOLATILE)) {
       char tmpstr[1024];
       snprintf (tmpstr, 1024, " * no device specified for fstab-node \"%s\", and filesystem does not have the volatile-attribute.\n", tstree->key);
       fdputs (tmpstr, ev->integer);
       ev->task++;
      }
     } else if ((stat ((((struct fstab_entry *)(tstree->value))->device), &stbuf) == -1) && (!(((struct fstab_entry *)(tstree->value))->fs) || ((fstree = streefind (mcb.filesystems, (((struct fstab_entry *)(tstree->value))->fs), TREE_FIND_FIRST)) && !((uintptr_t)fstree->value & FS_CAPA_VOLATILE)))) {
      char tmpstr[1024];
      snprintf (tmpstr, 1024, " * cannot stat device \"%s\" from node \"%s\", the error was \"%s\".\n", (((struct fstab_entry *)(tstree->value))->device), tstree->key, strerror (errno));
      fdputs (tmpstr, ev->integer);
      ev->task++;
     }

     if (((struct fstab_entry *)(tstree->value))->options) {
      unsigned long tmpmnt = 0;
      char *xtmp = __options_string_to_mountflags ((((struct fstab_entry *)(tstree->value))->options), &tmpmnt, (((struct fstab_entry *)(tstree->value))->mountpoint));

      if (((struct fstab_entry *)(tstree->value))->options[0] && !((struct fstab_entry *)(tstree->value))->options[1] && strchr (((struct fstab_entry *)(tstree->value))->options[0], ',') && !strcmp (((struct fstab_entry *)(tstree->value))->options[0], xtmp)) {
       char tmp[1024];
       snprintf (tmp, 1024, " * node \"%s\": these options look fishy: \"%s\"\n   remember: in the xml files, you need to specify options separated using colons (:), not commas (,)!\n", ((struct fstab_entry *)(tstree->value))->mountpoint, xtmp);
       fdputs (tmp, ev->integer);
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

void mount_update_handler(struct einit_event *event) {
 if (event) {
  if ((event->flag & EVENT_UPDATE_METADATA) && (mcb.update_options & EVENT_UPDATE_METADATA)) update_filesystem_metadata ();
  if ((event->flag & EVENT_UPDATE_BLOCK_DEVICES) && (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) update_block_devices ();
  if ((event->flag & EVENT_UPDATE_FSTAB) && (mcb.update_options & EVENT_UPDATE_FSTAB)) update_fstab ();
  if ((event->flag & EVENT_UPDATE_MTAB) && (mcb.update_options & EVENT_UPDATE_MTAB)) update_mtab ();
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
   if (fse->status & BF_STATUS_MOUNTED) {
    char tmp[1024];
    char *tset = set2str (',', fse->options); 

    if (tset)
     snprintf (tmp, 1024, "%s %s %s %s,%s 0 0\n", fse->adevice, fse->mountpoint, fse->afs,
               fse->aflags & MS_RDONLY ? "ro" : "rw", tset);
    else
     snprintf (tmp, 1024, "%s %s %s %s 0 0\n", fse->adevice, fse->mountpoint, fse->afs,
               fse->aflags & MS_RDONLY ? "ro" : "rw");

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
int enable (enum mounttask p, struct einit_event *status) {
// struct einit_event feedback = ei_module_feedback_default;
 struct stree *ha = mcb.fstab, *fsi = NULL;
 struct fstab_entry *fse;
 char **candidates = NULL;
 uint32_t i, ret, sc = 0, slc;
 pthread_t **childthreads = NULL;

 switch (p) {
  case MOUNT_LOCAL:
  case MOUNT_REMOTE:
   while (ha) {
    if (!inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING) &&
         strcmp (ha->key, "/") && strcmp (ha->key, "/dev") &&
         strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if (fse = (struct fstab_entry *)ha->value) {
      if (fse->status & BF_STATUS_MOUNTED)
       goto mount_skip;
      if (fse->mountflags & (MOUNT_FSTAB_NOAUTO | MOUNT_FSTAB_CRITICAL))
       goto mount_skip;

      if (fse->fs && (fsi = streefind (mcb.filesystems, fse->fs, TREE_FIND_FIRST))) {
       if (p == MOUNT_LOCAL) {
        if ((uintptr_t)fsi->value & FS_CAPA_NETWORK) goto mount_skip;
       } else {
         if (!((uintptr_t)fsi->value & FS_CAPA_NETWORK)) goto mount_skip;
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
   ret = mountwrapper ("/proc", status, MOUNT_TF_MOUNT);
   update (UPDATE_MTAB);
   ret = mountwrapper ("/sys", status, MOUNT_TF_MOUNT);
#endif
   ret = mountwrapper ("/dev", status, MOUNT_TF_MOUNT);
   if (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES) {
    status->string = "re-scanning block devices";
    status_update (status);
    update (UPDATE_BLOCK_DEVICES);
   }

   ret = mountwrapper ("/", status, MOUNT_TF_MOUNT | MOUNT_TF_FORCE_RW);
   if (mcb.options & OPTION_MAINTAIN_MTAB) {
    char *tmpmtab = generate_legacy_mtab (&mcb);

    if (tmpmtab) {
     unlink ("/etc/mtab");

     FILE *mtabfile = fopen (mcb.mtab_file, "w");

     if (mtabfile) {
      fputs (tmpmtab, mtabfile);
      fclose (mtabfile);
     }

     free (tmpmtab);
    }
   }

   return ret;
   break;
  case MOUNT_CRITICAL:
   while (ha) {
    if ((fse = (struct fstab_entry *)ha->value) && (fse->mountflags & MOUNT_FSTAB_CRITICAL))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
    else if (inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = streenext (ha);
   }
   break;
  default:
   status->string = "I'm clueless?";
   status_update (status);
   return STATUS_FAIL;
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
    if (mountwrapper (acand[c], status, MOUNT_TF_MOUNT) != STATUS_OK)
     status->flag++;
   }

  free (acand);
  sc++;
 }

 struct einit_event rev = evstaticinit(EVE_NEW_MOUNT_LEVEL);
 rev.integer = p;
 event_emit (&rev, EINIT_EVENT_FLAG_BROADCAST);
 evstaticdestroy (rev);

// scan for new modules after mounting all critical filesystems
// if (p == MOUNT_CRITICAL) mod_scanmodules();

 return STATUS_OK;
}

int disable (enum mounttask p, struct einit_event *status) {
// return STATUS_OK;
 struct stree *ha;
 struct stree *fsi;
 struct fstab_entry *fse = NULL;
 char **candidates = NULL;
 uint32_t i, ret, sc = 0, slc;
 pthread_t **childthreads = NULL;

 update (UPDATE_MTAB);
 ha = mcb.fstab;

 switch (p) {
  case MOUNT_REMOTE:
  case MOUNT_LOCAL:
   while (ha) {
    if (!inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING) && strcmp (ha->key, "/") && strcmp (ha->key, "/dev") && strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if (fse = (struct fstab_entry *)ha->value) {
      if (!(fse->status & BF_STATUS_MOUNTED)) goto mount_skip;

      if (p == MOUNT_LOCAL) {
       if (fse->afs) {
        if ((fsi = streefind (mcb.filesystems, fse->afs, TREE_FIND_FIRST)) && ((uintptr_t)fsi->value & FS_CAPA_NETWORK)) goto mount_skip;
       }
      } else if (p == MOUNT_REMOTE) {
       if (fse->afs && (fsi = streefind (mcb.filesystems, fse->afs, TREE_FIND_FIRST))) {
        if (!((uintptr_t)fsi->value & FS_CAPA_NETWORK)) goto mount_skip;
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
    mountwrapper ("/dev", status, MOUNT_TF_UMOUNT);
#ifdef LINUX
    mountwrapper ("/sys", status, MOUNT_TF_UMOUNT);
    mountwrapper ("/proc", status, MOUNT_TF_UMOUNT);
#endif
    if (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES) {
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

   return mountwrapper ("/", status, MOUNT_TF_UMOUNT);
//   return STATUS_OK;
  case MOUNT_CRITICAL:
   while (ha) {
    if (inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = streenext (ha);
   }
   break;
  default:
   status->string = "come again?";
   status_update (status);
   return STATUS_FAIL;
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
    if (mountwrapper (acand[c], status, MOUNT_TF_UMOUNT) != STATUS_OK)
     status->flag++;
   }

  free (acand);
  sc--;
 }

 return STATUS_OK;
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_CONFIGURATION_UPDATE) {
  struct cfgnode *node = NULL;
  if ((node = cfg_getnode ("configuration-storage-maintain-mtab",NULL)) && node->flag && node->svalue) {
   mcb.options |= OPTION_MAINTAIN_MTAB;
   mcb.mtab_file = node->svalue;
  }

  update_fstab();
 }
}
