/*
 *  einit-mount.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 07/06/2006.
 *  Renamed from common-mount.c on 11/10/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <einit/bitch.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <einit/dexec.h>
#include <einit/pexec.h>
#include <sys/mount.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
char *provides[] = {"mount", NULL};
const struct smodule self = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_LOADER,
	.options	= 0,
	.name		= "Filesystem-Mounter",
	.rid		= "einit-mount",
	.provides	= provides,
	.requires	= NULL,
	.notwith	= NULL
};

char *provides_mountlocal[] = {"mount/local", NULL};
char *requires_mountlocal[] = {"/", "mount/system", "mount/critical", NULL};
struct smodule sm_mountlocal = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (local)",
	.rid		= "einit-mount-local",
	.provides	= provides_mountlocal,
	.requires	= requires_mountlocal,
	.notwith	= NULL
};

char *provides_mountremote[] = {"mount/remote", NULL};
char *requires_mountremote[] = {"/", "mount/system", "network", "portmap", NULL};
struct smodule sm_mountremote = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (remote)",
	.rid		= "einit-mount-remote",
	.provides	= provides_mountremote,
	.requires	= requires_mountremote,
	.notwith	= NULL
};

char *provides_system[] = {"mount/system", NULL};
char *requires_system[] = {"/", NULL};
struct smodule sm_system = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (system)",
	.rid		= "einit-mount-system",
	.provides	= provides_system,
	.requires	= requires_system,
	.notwith	= NULL
};

char *provides_critical[] = {"mount/critical", NULL};
char *requires_critical[] = {"/", "mount/system", NULL};
struct smodule sm_critical = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (critical)",
	.rid		= "einit-mount-critical",
	.provides	= provides_critical,
	.requires	= requires_critical,
	.notwith	= NULL
};

char *provides_root[] = {"/", NULL};
struct smodule sm_root = {
	.eiversion	= EINIT_VERSION,
	.version	= 1,
	.mode		= EINIT_MOD_EXEC,
	.options	= 0,
	.name		= "mount (/)",
	.rid		= "einit-mount-root",
	.provides	= provides_root,
	.requires	= NULL,
	.notwith	= NULL
};

/* variable definitions */
pthread_mutex_t blockdevices_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtab_mutex = PTHREAD_MUTEX_INITIALIZER;
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
	 .mtab			= NULL,
	 .filesystems		= NULL,
	 .add_block_device	= add_block_device,
	 .add_fstab_entry	= add_fstab_entry,
	 .add_mtab_entry	= add_mtab_entry,
	 .add_filesystem	= add_filesystem,
	 .update_options	= EVENT_UPDATE_METADATA + EVENT_UPDATE_BLOCK_DEVICES + EVENT_UPDATE_FSTAB + EVENT_UPDATE_MTAB,
	 .critical		= NULL
};

/* function declarations */
int scanmodules (struct lmodule *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);
int enable (enum mounttask, struct einit_event *);
int disable (enum mounttask, struct einit_event *);
int mountwrapper (char *, struct einit_event *, uint32_t);

/* function definitions */

/* error checking... */
int examine_configuration (struct lmodule *irr) {
 int pr = 0;
 char **tmpset, *tmpstring;

 if (!(tmpstring = cfg_getstring("configuration-storage-fstab-source", NULL))) {
  fputs (" * configuration variable \"configuration-storage-fstab-source\" not found.\n", stderr);
  pr++;
 } else {
  tmpset = str2set(':', tmpstring);

  if (inset ((void **)tmpset, (void *)"label", SET_TYPE_STRING)) {
   if (!(mcb.update_options & EVENT_UPDATE_METADATA)) {
    fputs (" * fstab-source \"label\" to be used, but optional update-step \"metadata\" not enabled.\n", stderr);
    pr++;
   }
   if (!(mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) {
    fputs (" * fstab-source \"label\" to be used, but optional update-step \"block-devices\" not enabled.\n", stderr);
    pr++;
   }
  }

  if (!inset ((void **)tmpset, (void *)"configuration", SET_TYPE_STRING)) {
   fputs (" * fstab-source \"configuration\" disabled! In 99.999% of all cases, you don't want to do that!\n", stderr);
   pr++;
  }

  if (inset ((void **)tmpset, (void *)"legacy", SET_TYPE_STRING)) {
   fputs (" * fstab-source \"legacy\" enabled; you shouldn't rely on that.\n", stderr);
   pr++;
  }

  free (tmpset);
 }

 if (mcb.update_options & EVENT_UPDATE_METADATA) {
  if (!(mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES)) {
   fputs (" * update-step \"metadata\" cannot be performed without update-step \"block-devices\".\n", stderr);
   pr++;
  }
 } else {
  fputs (" * update-step \"metadata\" disabled; not a problem per-se, but this will prevent your filesystems from being automatically fsck()'d.\n", stderr);
 }

 if (!mcb.fstab) {
  fputs (" * your fstab is empty.\n", stderr);
  pr++;
 } else {
  struct uhash *thash, *fhash;
  if (!(thash = hashfind (mcb.fstab, "/"))) {
   fputs (" * your fstab does not contain an entry for \"/\".\n", stderr);
   pr++;
  } else if (!(((struct fstab_entry *)(thash->value))->device)) {
   fputs (" * you have apparently forgotten to specify a device for your root-filesystem.\n", stderr);
   pr++;
  } else if (!strcmp ("/dev/ROOT", (((struct fstab_entry *)(thash->value))->device))) {
   fputs (" * you didn't edit your local.xml to specify your root-filesystem.\n", stderr);
   pr++;
  }

  thash = mcb.fstab;
  while (thash) {
   struct stat stbuf;

   if (!(((struct fstab_entry *)(thash->value))->fs) || !strcmp ("auto", (((struct fstab_entry *)(thash->value))->fs))) {
    fprintf (stderr, " * no filesystem type specified for fstab-node \"%s\", or type set to auto -- eINIT cannot do that, yet, please specify the filesystem type.\n", thash->key);
    pr++;
   }

   if (!(((struct fstab_entry *)(thash->value))->device)) { 
    if ((((struct fstab_entry *)(thash->value))->fs) && (fhash = hashfind (mcb.filesystems, (((struct fstab_entry *)(thash->value))->fs))) && !((uintptr_t)fhash->value & FS_CAPA_VOLATILE)) {
     fprintf (stderr, " * no device specified for fstab-node \"%s\", and filesystem does not have the volatile-attribute.\n", thash->key);
     pr++;
    }
   } else if (stat ((((struct fstab_entry *)(thash->value))->device), &stbuf) == -1) {
    fprintf (stderr, " * cannot stat device \"%s\" from node \"%s\", the error was \"%s\".\n", (((struct fstab_entry *)(thash->value))->device), thash->key, strerror (errno));
    pr++;
   }
   thash = hashnext (thash);
  }
 }

 return pr;
}

/* the actual module */

int configure (struct lmodule *this) {
 struct cfgnode *node = NULL;

/* pexec configuration */
 pexec_configure (this);

 event_listen (EVENT_SUBSYSTEM_IPC, mount_ipc_handler);
 event_listen (EVENT_SUBSYSTEM_MOUNT, mount_update_handler);

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

 if ((node = cfg_findnode ("configuration-storage-critical-mountpoints",0,NULL)) && node->svalue)
  mcb.critical = str2set(':', node->svalue);

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
 struct uhash *ucur;

 hashfree (mcb.blockdevices);
 ucur = mcb.fstab;
 while (ucur) {
  if (ucur->value) {
   if (((struct fstab_entry *)(ucur->value))->mountpoint)
    free (((struct fstab_entry *)(ucur->value))->mountpoint);
   if (((struct fstab_entry *)(ucur->value))->device)
    free (((struct fstab_entry *)(ucur->value))->device);
   if (((struct fstab_entry *)(ucur->value))->fs)
    free (((struct fstab_entry *)(ucur->value))->fs);
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
  ucur = hashnext (ucur);
 }
 hashfree (mcb.fstab);
 hashfree (mcb.mtab);
 hashfree (mcb.filesystems);

 mcb.blockdevices = NULL;
 mcb.fstab = NULL;
 mcb.mtab = NULL;
 mcb.filesystems = NULL;

 function_unregister ("find-block-devices-dev", 1, (void *)find_block_devices_recurse_path);
 function_unregister ("read-fstab-label", 1, (void *)forge_fstab_by_label);
 function_unregister ("read-fstab-configuration", 1, (void *)read_fstab_from_configuration);
 function_unregister ("read-fstab-fstab", 1, (void *)read_fstab);
 function_unregister ("read-mtab-legacy", 1, (void *)read_mtab);
 function_unregister ("fs-mount", 1, (void *)mountwrapper);

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

 pexec_cleanup(this);
}

unsigned char find_block_devices_recurse_path (char *path) {
 DIR *dir;
 struct dirent *entry;
 if (path == (char *)&mcb) path = "/dev/";

#ifdef POSIXREGEX
 unsigned char nfitfc = 0;
 static struct cfgnode *npattern = NULL;
 static regex_t devpattern;
 static unsigned char havedevpattern = 0;

 if (!npattern) {
  nfitfc = 1;
  npattern = cfg_findnode ("configuration-storage-block-devices-dev-constraints", 0, NULL);
  if (npattern && npattern->svalue) {
   uint32_t err;
   if (!(err = regcomp (&devpattern, npattern->svalue, REG_EXTENDED)))
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
    } else if (S_ISSOCK (statbuf.st_mode) && S_ISDIR (statbuf.st_mode)) {
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
 struct uhash *element = mcb.blockdevices;
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
  element = hashnext (element);
 }
 return 0;
}

unsigned char read_fstab_from_configuration (void *na) {
 struct cfgnode *node = NULL;
 uint32_t i;
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
 struct uhash *workhash = read_fsspec_file ("/etc/fstab");
 struct uhash *cur = workhash;

 while (cur) {
  struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;

  add_fstab_entry (val->fs_file ? estrdup(val->fs_file) : NULL, val->fs_spec ? estrdup(val->fs_spec) : NULL, val->fs_vfstype ? estrdup(val->fs_vfstype) : NULL, str2set (',', val->fs_mntops), 0, NULL, NULL, NULL, NULL, NULL, 0, NULL);

  cur = hashnext (cur);
 }

 hashfree(workhash);
 return 0;
}

unsigned char read_mtab (void *na) {
 struct uhash *workhash = read_fsspec_file ("/etc/mtab");
 struct uhash *cur = workhash;

/* this will be removed later */
// hashfree_mtab(mcb.mtab);
 pthread_mutex_lock (&mtab_mutex);
 hashfree (mcb.mtab);
 mcb.mtab = NULL;
 pthread_mutex_unlock (&mtab_mutex);

 while (cur) {
//  puts (cur->key);
  struct legacy_fstab_entry * val = (struct legacy_fstab_entry *)cur->value;
  add_mtab_entry (val->fs_spec, val->fs_file, val->fs_vfstype, val->fs_mntops, val->fs_freq, val->fs_passno);

  cur = hashnext (cur);
 }

 hashfree(workhash);
 return 0;
}

struct uhash *read_fsspec_file (char *file) {
 struct uhash *workhash = NULL;
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
#ifdef DEBUG
     printf ("parsed fstab entry: fs_spec=%s, fs_file=%s, fs_vfstype=%s, fs_mntops=%s, fs_freq=%i, fs_passno=%i\n", ne.fs_spec, ne.fs_file, ne.fs_vfstype, ne.fs_mntops, ne.fs_freq, ne.fs_passno);
#endif
     workhash = hashadd (workhash, ne.fs_file, &ne, sizeof (struct legacy_fstab_entry), ascur);
//     workhash = hashadd (workhash, ne->fs_file, ne, -1);
    }
   }
  }
  done_parsing_file:
  fclose (fp);
 }

 return workhash;
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
 mod_add (NULL, (int (*)(void *, struct einit_event *))enable,
          (int (*)(void *, struct einit_event *))disable,
          NULL, NULL, NULL,
          (void *)MOUNT_ROOT, &sm_root);
 mod_add (NULL, (int (*)(void *, struct einit_event *))enable,
          (int (*)(void *, struct einit_event *))disable,
          NULL, NULL, NULL,
          (void *)MOUNT_LOCAL, &sm_mountlocal);
 mod_add (NULL, (int (*)(void *, struct einit_event *))enable,
          (int (*)(void *, struct einit_event *))disable,
          NULL, NULL, NULL,
          (void *)MOUNT_REMOTE, &sm_mountremote);
 mod_add (NULL, (int (*)(void *, struct einit_event *))enable,
          (int (*)(void *, struct einit_event *))disable,
          NULL, NULL, NULL,
          (void *)MOUNT_SYSTEM, &sm_system);
 mod_add (NULL, (int (*)(void *, struct einit_event *))enable,
          (int (*)(void *, struct einit_event *))disable,
          NULL, NULL, NULL,
          (void *)MOUNT_CRITICAL, &sm_critical);
}

int mountwrapper (char *mountpoint, struct einit_event *status, uint32_t tflags) {
 struct uhash *he = mcb.fstab;
 struct uhash *de = mcb.blockdevices;
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
  if ((he = hashfind (he, mountpoint)) && (fse = (struct fstab_entry *)he->value)) {
   source = fse->device;
   fsntype = 0;
   if ((de = hashfind (de, source)) && (bdi = (struct bd_info *)de->value)) {
    fsntype = bdi->fs_type;
   }

   if (fse->fs) {
    fstype = fse->fs;
   } else if (fsntype) {
    if (bdi->fs_type < 0xffff)
     fstype = fslist_hr[bdi->fs_type];
    else
     fstype = "auto";
   }

   if (bdi && (bdi->status & BF_STATUS_DIRTY)) {
    if (fsck_command) {
     char tmp[1024];
     status->string = "filesystem dirty; running fsck";
     status_update (status);

     snprintf (tmp, 1024, fsck_command, fstype, de->key);
#ifndef SANDBOX
//     pexec (tmp, NULL, 0, 0, NULL, status);
     pexec_simple (tmp, NULL, NULL, status);
#else
     status->string = tmp;
     status_update (status);
#endif
    } else {
     status->string = "WARNING: filesystem dirty, but no fsck command known";
     status_update (status);
    }
   }

   fs_mount_function_name = emalloc (10+strlen (fstype));
   *fs_mount_function_name = 0;
   fs_mount_function_name = strcat (fs_mount_function_name, "fs-mount-");
   fs_mount_function_name = strcat (fs_mount_function_name, fstype);

   fs_mount_functions = function_find (fs_mount_function_name, 1, NULL);

   if (!source)
    source = fstype;

   if (bdi && bdi->label)
    snprintf (verbosebuffer, 1023, "mounting %s [%s; label=%s; fs=%s]", mountpoint, source, bdi->label, fstype);
   else
    snprintf (verbosebuffer, 1023, "mounting %s [%s; fs=%s]", mountpoint, source, fstype);
   status->string = verbosebuffer;
   status_update (status);

#ifndef SANDBOX
   if (fse->before_mount)
    pexec_simple (fse->before_mount, fse->variables, NULL, status);
//    pexec (fse->before_mount, fse->variables, 0, 0, NULL, status);
#endif

   if (fs_mount_functions && fs_mount_functions[0]) {
    uint32_t j = 0;
    for (; fs_mount_functions[j]; j++) {
     mount_function = fs_mount_functions[j];
     if (!mount_function (tflags, source, mountpoint, fstype, bdi, fse, status))
      goto mount_success;
    }
   }

#ifdef LINUX
//   mntflags = MS_MANDLOCK;
   mntflags = 0;
#else
   mntflags = 0;
#endif
   if (fse->options) {
    int fi = 0;
    for (; fse->options[fi]; fi++) {
#ifdef LINUX
     if (!strcmp (fse->options[fi], "noatime")) mntflags |= MS_NOATIME;
     else if (!strcmp (fse->options[fi], "nodev")) mntflags |= MS_NODEV;
     else if (!strcmp (fse->options[fi], "nodiratime")) mntflags |= MS_NODIRATIME;
     else if (!strcmp (fse->options[fi], "ro")) mntflags |= MS_RDONLY;
     else if (!strcmp (fse->options[fi], "nomand")) mntflags ^= MS_MANDLOCK;
     else if (!strcmp (fse->options[fi], "nosuid")) mntflags |= MS_NOSUID;
     else if (!strcmp (fse->options[fi], "remount")) mntflags |= MS_REMOUNT;
     else
#endif
     if (!fsdata) {
      uint32_t slen = strlen (fse->options[fi])+1;
      fsdata = ecalloc (1, slen);
      memcpy (fsdata, fse->options[fi], slen);
     } else {
      uint32_t fsdl = strlen(fsdata) +1, slen = strlen (fse->options[fi])+1;
      fsdata = erealloc (fsdata, fsdl+slen);
      *(fsdata + fsdl -1) = ',';
      memcpy (fsdata+fsdl, fse->options[fi], slen);
     }
    }
   }

//   fprintf (stderr, "mntflags=%i\n", mntflags);

//   fsdata = NULL;
#ifndef SANDBOX
   if (mount (source, mountpoint, fstype, mntflags, fsdata) == -1) {
    if (errno == EBUSY) {
     if (mount (source, mountpoint, fstype, MS_REMOUNT | mntflags, fsdata) == -1) goto mount_panic;
    } else {
     mount_panic:
     if (errno < sys_nerr)
      status->string = (char *)sys_errlist[errno];
     else
      status->string = "an unknown error occured while trying to mount the filesystem";
     status_update (status);
     if (fse->after_umount)
      pexec_simple (fse->after_umount, fse->variables, NULL, status);
     return STATUS_FAIL;
    }
   }

   mount_success:

   if (fse->after_mount)
    pexec_simple (fse->after_mount, fse->variables, NULL, status);

   if (fse->manager)
    startdaemon (fse->manager, status);
#else
   mount_success:
#endif
   fse->status |= BF_STATUS_MOUNTED;

   if (fs_mount_functions) free (fs_mount_functions);

   return STATUS_OK;
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
  
  while (1) {
   retry++;
   if (he = hashfind (he, mountpoint)) fse = (struct fstab_entry *)he->value;

   if (fse && !(fse->status & BF_STATUS_MOUNTED))
    snprintf (textbuffer, 1024, "unmounting %s: seems not to be mounted", mountpoint);
   else
    snprintf (textbuffer, 1024, "unmounting %s", mountpoint);

   status->string = textbuffer;
   status_update (status);
#ifndef SANDBOX
   if (umount (mountpoint)) {
    snprintf (textbuffer, 1024, "%s: umount() failed: %s", mountpoint, strerror(errno));
    status->string = textbuffer;
    status_update (status);
#ifdef LINUX
    if (umount2 (mountpoint, MNT_FORCE)) {
     snprintf (textbuffer, 1024, "%s: umount2() failed: %s", mountpoint, strerror(errno));
     status->string = textbuffer;
     status_update (status);

     struct uhash *hav = hashfind (mcb.mtab, mountpoint);
     if (!hav) {
      status->string = "wtf? this ain't mounted, bitch!";
      status_update (status);
      goto umount_fail;
     }
     struct legacy_fstab_entry *lfse = (struct legacy_fstab_entry *)hav->value;
     if (lfse) {
      if (mount (lfse->fs_spec, lfse->fs_file, NULL, MS_REMOUNT | MS_RDONLY, NULL)) {
       snprintf (textbuffer, 1024, "%s: remounting r/o failed: %s", mountpoint, strerror(errno));
       status->string = textbuffer;
       status_update (status);
       goto umount_fail;
      } else {
       if (umount2 (mountpoint, MNT_DETACH)) {
        snprintf (textbuffer, 1024, "%s: remounted r/o but detaching failed: %s", mountpoint, strerror(errno));
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       } else {
        snprintf (textbuffer, 1024, "%s: remounted r/o and detached", mountpoint);
        status->string = textbuffer;
        status_update (status);
        goto umount_ok;
       }
      }
     } else {
      snprintf (textbuffer, 1024, "%s: device mounted but I don't know anything more; bailing out", mountpoint);
      status->string = textbuffer;
      status_update (status);
      goto umount_fail;
     }
    }
#else
    goto umount_fail;
#endif
   }
#endif

   umount_fail:

   if (retry < 3) {
    snprintf (textbuffer, 1024, "%s: attempt %i: device still mounted...", mountpoint, retry);
    status->string = textbuffer;
    status_update (status);
   } else {
    snprintf (textbuffer, 1024, "%s: attempt %i: failed, not retrying.", mountpoint, retry);
    status->string = textbuffer;
    status_update (status);
	return STATUS_FAIL;
   }
   sleep (1);
  }

  umount_ok:
#ifndef SANDBOX
  if (fse && fse->after_umount)
   pexec_simple (fse->after_umount, fse->variables, NULL, status);
#endif
  if (fse && (fse->status & BF_STATUS_MOUNTED))
   fse->status ^= BF_STATUS_MOUNTED;

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
 if (hashfind (mcb.blockdevices, devicefile)) {
  pthread_mutex_unlock (&blockdevices_mutex);
  return;
 }

 mcb.blockdevices = hashadd (mcb.blockdevices, devicefile, &bdi, sizeof (struct bd_info), NULL);
// mcb.blockdevices = hashadd (mcb.blockdevices, devicefile, bdi, -1);

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
 if (hashfind (mcb.fstab, mountpoint)) {
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

 mcb.fstab = hashadd (mcb.fstab, mountpoint, &fse, sizeof (struct fstab_entry), fse.options);
 pthread_mutex_unlock (&fstab_mutex);
// mcb.fstab = hashadd (mcb.fstab, mountpoint, fse, -1);
}

void add_mtab_entry (char *fs_spec, char *fs_file, char *fs_vfstype, char *fs_mntops, uint32_t fs_freq, uint32_t fs_passno) {
 struct legacy_fstab_entry lfse;
 char **dset = NULL;

 if (!fs_file) return;
 dset = (char **)setadd ((void **)dset, (void *)fs_file, SET_TYPE_STRING);

 if (!fs_spec) dset = (char **)setadd ((void **)dset, (void *)"nodevice", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_spec, SET_TYPE_STRING);

 if (!fs_vfstype) dset = (char **)setadd ((void **)dset, (void *)"auto", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_vfstype, SET_TYPE_STRING);

 if (!fs_mntops) dset = (char **)setadd ((void **)dset, (void *)"rw", SET_TYPE_STRING);
 else dset = (char **)setadd ((void **)dset, (void *)fs_mntops, SET_TYPE_STRING);

 memset (&lfse, 0, sizeof (struct legacy_fstab_entry));

 lfse.fs_spec = dset[1];
 lfse.fs_file = dset[0];
 lfse.fs_vfstype = dset[2];
 lfse.fs_mntops = dset[3];
 lfse.fs_freq = fs_freq;
 lfse.fs_passno = fs_passno;

 pthread_mutex_lock (&mtab_mutex);
 if (hashfind (mcb.mtab, fs_file)) {
  free (dset);
  pthread_mutex_unlock (&mtab_mutex);
  return;
 }

 mcb.mtab = hashadd (mcb.mtab, fs_file, &lfse, sizeof (struct legacy_fstab_entry), dset);
 pthread_mutex_unlock (&mtab_mutex);
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
 if (hashfind (mcb.filesystems, name)) {
  pthread_mutex_unlock (&fs_mutex);
  return;
 }

 mcb.filesystems = hashadd (mcb.filesystems, name, (void *)flags, -1, NULL);
 pthread_mutex_unlock (&fs_mutex);
}

/* all the current IPC commands will be made #DEBUG-only, but we'll keep 'em for now */
void mount_ipc_handler(struct einit_event *event) {
 if (!event || !event->set) return;
 char **argv = (char **) event->set;
 if (argv[0] && argv[1] && !strcmp (argv[0], "mount")) {
  if (!strcmp (argv[1], "ls_fstab")) {
   char buffer[1024];
   struct uhash *cur = mcb.fstab;
   struct fstab_entry *val = NULL;

   if (!event->flag) event->flag = 1;

   while (cur) {
    val = (struct fstab_entry *) cur->value;
    if (val) {
     snprintf (buffer, 1023, "%s [spec=%s;vfstype=%s;flags=%i;before-mount=%s;after-mount=%s;before-umount=%s;after-umount=%s;status=%i]\n", val->mountpoint, val->device, val->fs, val->mountflags, val->before_mount, val->after_mount, val->before_umount, val->after_umount, val->status);
     write (event->integer, buffer, strlen (buffer));
    }
    cur = hashnext (cur);
   }
  } else if (!strcmp (argv[1], "ls_blockdev")) {
   char buffer[1024];
   struct uhash *cur = mcb.blockdevices;
   struct bd_info *val = NULL;

   if (!event->flag) event->flag = 1;

   while (cur) {
    val = (struct bd_info *) cur->value;
    if (val) {
     snprintf (buffer, 1023, "%s [fs=%s;type=%i;label=%s;uuid=%s;flags=%i]\n", cur->key, val->fs, val->fs_type, val->label, val->uuid, val->status);
     write (event->integer, buffer, strlen (buffer));
    }
    cur = hashnext (cur);
   }
  } else if (!strcmp (argv[1], "ls_mtab")) {
   char buffer[1024];
   struct uhash *cur = mcb.mtab;
   struct legacy_fstab_entry *val = NULL;

   if (!event->flag) event->flag = 1;

   while (cur) {
    val = (struct legacy_fstab_entry *) cur->value;
    if (val) {
     snprintf (buffer, 1023, "%s [spec=%s;vfstype=%s;mntops=%s;freq=%i;passno=%i]\n", val->fs_file, val->fs_spec, val->fs_vfstype, val->fs_mntops, val->fs_freq, val->fs_passno);
     write (event->integer, buffer, strlen (buffer));
    }
    cur = hashnext (cur);
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

int enable (enum mounttask p, struct einit_event *status) {
// struct einit_event feedback = ei_module_feedback_default;
 struct uhash *ha = mcb.fstab, *fsi = NULL;
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
      if (fse->mountflags & (MOUNT_FSTAB_NOAUTO | MOUNT_FSTAB_CRITICAL))
       goto mount_skip;

      if (fse->fs && (fsi = hashfind (mcb.filesystems, fse->fs))) {
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
    ha = hashnext (ha);
   }
   break;
  case MOUNT_ROOT:
   return mountwrapper ("/", status, MOUNT_TF_MOUNT | MOUNT_TF_FORCE_RW);
   break;
  case MOUNT_SYSTEM:
#ifdef LINUX
   ret = mountwrapper ("/proc", status, MOUNT_TF_MOUNT);
/* link /etc/mtab to /proc/mounts */
   unlink ("/etc/mtab");
   symlink ("/proc/mounts", "/etc/mtab");
/* discard errors, they are irrelevant. */
   errno = 0;

   ret = mountwrapper ("/sys", status, MOUNT_TF_MOUNT);
#endif
   ret = mountwrapper ("/dev", status, MOUNT_TF_MOUNT);
   if (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES) {
    status->string = "re-scanning block devices";
    status_update (status);
    update (UPDATE_BLOCK_DEVICES);
   }
   return ret;
   break;
  case MOUNT_CRITICAL:
   while (ha) {
    if ((fse = (struct fstab_entry *)ha->value) && (fse->mountflags & MOUNT_FSTAB_CRITICAL))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
    else if (inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = hashnext (ha);
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
 will be mounted after lower paths, no matter how they were sorted in the fstab-hash */
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

 return STATUS_OK;
}

int disable (enum mounttask p, struct einit_event *status) {
// return STATUS_OK;
 struct uhash *ha = mcb.mtab;
 struct uhash *fsi;
 struct legacy_fstab_entry *lfse = NULL;
 char **candidates = NULL;
 uint32_t i, ret, sc = 0, slc;
 pthread_t **childthreads = NULL;

 switch (p) {
  case MOUNT_LOCAL:
  case MOUNT_REMOTE:
   while (ha) {
    if (!inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING) && strcmp (ha->key, "/") && strcmp (ha->key, "/dev") && strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if (lfse = (struct legacy_fstab_entry *)ha->value) {
      if (p == MOUNT_LOCAL) {
       if (lfse->fs_vfstype) {
        if ((fsi = hashfind (mcb.filesystems, lfse->fs_vfstype)) && ((uintptr_t)fsi->value & FS_CAPA_NETWORK)) goto mount_skip;
       }
      } else if (p == MOUNT_REMOTE) {
       if (lfse->fs_vfstype && (fsi = hashfind (mcb.filesystems, lfse->fs_vfstype))) {
        if (!((uintptr_t)fsi->value & FS_CAPA_NETWORK)) goto mount_skip;
       } else goto mount_skip;
      }
     }
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);
    }
    mount_skip:
    ha = hashnext (ha);
   }
   break;
  case MOUNT_ROOT:
   return mountwrapper ("/", status, MOUNT_TF_UMOUNT);
//   return STATUS_OK;
   break;
  case MOUNT_SYSTEM:
   {
    mountwrapper ("/dev", status, MOUNT_TF_UMOUNT);
#ifdef LINUX
    mountwrapper ("/sys", status, MOUNT_TF_UMOUNT);
    unlink ("/etc/mtab");
    mountwrapper ("/proc", status, MOUNT_TF_UMOUNT);
#endif
    if (mcb.update_options & EVENT_UPDATE_BLOCK_DEVICES) {
     status->string = "re-scanning block devices";
     status_update (status);
// things go weird if this if enabled:
//     update (UPDATE_BLOCK_DEVICES);
    }
   }
   return STATUS_OK;
  case MOUNT_CRITICAL:
   while (ha) {
    if (inset ((void **)mcb.critical, (void *)ha->key, SET_TYPE_STRING))
     candidates = (char **)setadd ((void **)candidates, (void *)ha->key, SET_NOALLOC);

    ha = hashnext (ha);
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
