/*
 *  linux-mount.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/05/2006.
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

#include <einit/module.h>
#include <einit/config.h>
#include <einit/common-mount.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <einit/pexec.h>
#include <einit/dexec.h>

/* filesystem header files */
#include <linux/ext2_fs.h>

#define MOUNT_SUPPORT_EXT2

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

/* definitions */
enum mounttask {
 MOUNT_ROOT = 1,
 MOUNT_DEV = 2,
 MOUNT_PROC = 3,
 MOUNT_SYS = 4,
 MOUNT_LOCAL = 5,
 MOUNT_REMOTE = 6,
 MOUNT_ENCRYPTED = 7
};

#define MOUNT_TF_MOUNT			0x0001
#define MOUNT_TF_UMOUNT			0x0002
#define MOUNT_TF_FORCE_RW		0x0010
#define MOUNT_TF_FORCE_RO		0x0020

/* variable definitions */
char *defaultblockdevicesource[] = {"dev", NULL};
char *defaultfstabsource[] = {"label", "configuration", "fstab", NULL};
char *defaultfilesystems[] = {"linux", NULL};
struct uhash *blockdevices = NULL;
struct uhash *fstab = NULL;
struct uhash *blockdevicesupdatefunctions = NULL;
struct uhash *fstabupdatefunctions = NULL;
struct uhash *filesystemlabelupdaterfunctions = NULL;

/* module definitions */
char *provides[] = {"mount", NULL};
const struct smodule self = {
 EINIT_VERSION, 1, EINIT_MOD_LOADER, 0, "Linux-specific Filesystem mounter", "linux-mount", provides, NULL, NULL
};

char *provides_mountlocal[] = {"mount/local", NULL};
char *requires_mountlocal[] = {"/", "/dev", "/sys", "/proc", NULL};
struct smodule sm_mountlocal = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [local]", "linux-mount-local", provides_mountlocal, requires_mountlocal, NULL
};

char *provides_mountremote[] = {"mount/remote", NULL};
char *requires_mountremote[] = {"/", "/dev", "network", NULL};
struct smodule sm_mountremote = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [remote]", "linux-mount-remote", provides_mountremote, requires_mountremote, NULL
};

char *provides_mountencrypted[] = {"mount/encrypted", NULL};
char *requires_mountencrypted[] = {"/", "/dev", NULL};
struct smodule sm_mountencrypted = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [encrypted]", "linux-mount-encrypted", provides_mountencrypted, requires_mountencrypted, NULL
};

char *provides_dev[] = {"/dev", NULL};
char *requires_dev[] = {"/", NULL};
struct smodule sm_dev = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [/dev]", "linux-mount-dev", provides_dev, requires_dev, NULL
};

char *provides_sys[] = {"/sys", NULL};
char *requires_sys[] = {"/", NULL};
struct smodule sm_sys = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [/sys]", "linux-mount-sys", provides_sys, requires_sys, NULL
};

char *provides_proc[] = {"/proc", NULL};
char *requires_proc[] = {"/", NULL};
struct smodule sm_proc = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [/proc]", "linux-mount-proc", provides_proc, requires_proc, NULL
};

char *provides_root[] = {"/", NULL};
struct smodule sm_root = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [/]", "linux-mount-root", provides_root, NULL, NULL
};

/* function declarations */
unsigned char read_label_linux (void *);
int scanmodules (struct lmodule *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);
int enable (enum mounttask, struct mfeedback *);
int disable (enum mounttask, struct mfeedback *);
int mountwrapper (char *, struct mfeedback *, uint32_t);

/* function definitions */
unsigned char read_label_linux (void *na) {
 struct uhash *element = blockdevices;
 struct ext2_super_block ext2_sb;
 struct bd_info *bdi;
 while (element) {
  bdi = (struct bd_info *)element->value;

  FILE *device = NULL;
  device = fopen (element->key, "r");
  if (device) {
   if (fseek (device, 1024, SEEK_SET) || (fread (&ext2_sb, sizeof(struct ext2_super_block), 1, device) < 1)) {
//    perror (element->key);
    bdi->fs_status = FS_STATUS_ERROR | FS_STATUS_ERROR_IO;
   } else {
    if (ext2_sb.s_magic == EXT2_SUPER_MAGIC) {
     __u8 uuid[16];
     char c_uuid[38];

     bdi->fs_type = FILESYSTEM_EXT2;
     bdi->fs_status = FS_STATUS_OK;

     memcpy (uuid, ext2_sb.s_uuid, 16);
     if (ext2_sb.s_volume_name[0])
      bdi->label = estrdup (ext2_sb.s_volume_name);
     snprintf (c_uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", /*((char)ext2_sb.s_uuid)*/ uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
     bdi->uuid = estrdup (c_uuid);
    }
   }
   fclose (device);
  } else {
   bdi->fs_status = FS_STATUS_ERROR | FS_STATUS_ERROR_IO;
//   perror (element->key);
  }
  errno = 0;
  element = hashnext (element);
 }
 return 0;
}

int scanmodules (struct lmodule *modchain) {
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_DEV, &sm_dev);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_ROOT, &sm_root);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_LOCAL, &sm_mountlocal);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_REMOTE, &sm_mountremote);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_ENCRYPTED, &sm_mountencrypted);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_SYS, &sm_sys);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        (void *)MOUNT_PROC, &sm_proc);
}

int configure (struct lmodule *this) {
/* pexec configuration */
 struct cfgnode *node;
 node = cfg_findnode ("shell", 0, NULL);
 if (node && node->svalue)
  shell = (char **)str2set (' ', estrdup(node->svalue));
 else
  shell = dshell;

/* own configuration */
 read_filesystem_flags_from_configuration (NULL);

 blockdevicesupdatefunctions = hashadd (blockdevicesupdatefunctions, "dev", (void *)find_block_devices_recurse_path);
 fstabupdatefunctions = hashadd (fstabupdatefunctions, "label", (void *)forge_fstab_by_label);
 fstabupdatefunctions = hashadd (fstabupdatefunctions, "configuration", (void *)read_fstab_from_configuration);
 fstabupdatefunctions = hashadd (fstabupdatefunctions, "fstab", (void *)read_fstab);
 filesystemlabelupdaterfunctions = hashadd (filesystemlabelupdaterfunctions, "linux", (void *)read_label_linux);

 update_block_devices ();
 update_filesystem_labels ();
 update_fstab();
}

int cleanup (struct lmodule *this) {
}

int mountwrapper (char *mountpoint, struct mfeedback *status, uint32_t tflags) {
 struct uhash *he = fstab;
 struct uhash *de = blockdevices;
 struct fstab_entry *fse = NULL;
 struct bd_info *bdi = NULL;
 char *source;
 char *fstype;
 void *fsdata = NULL;
 uint32_t fsntype;
 char verbosebuffer [1024];

 if (tflags & MOUNT_TF_MOUNT) {
  if ((he = hashfind (he, mountpoint)) && (fse = (struct fstab_entry *)he->value)) {
   source = fse->device;
   fsntype = 0;
   if ((de = hashfind (de, source)) && (bdi = (struct bd_info *)de->value)) {
    fsntype = bdi->fs_type;
   }

   if (fse->fs) {
    fstype = fse->fs;
   } else {
    if (fsntype) switch (fsntype) {
     case FILESYSTEM_EXT2:
      fstype = "ext2";
      break;
     default:
      status->verbose = "filesystem type not known";
      status_update (status);
      return STATUS_FAIL;
    }
   }

   if (!source)
    source = fstype;

   if (bdi && bdi->label)
    snprintf (verbosebuffer, 1023, "mounting %s [%s; label=%s; fs=%s]", mountpoint, source, bdi->label, fstype);
   else
    snprintf (verbosebuffer, 1023, "mounting %s [%s; fs=%s]", mountpoint, source, fstype);
   status->verbose = verbosebuffer;
   status_update (status);

#ifndef SANDBOX
   if (fse->before_mount)
    pexec (fse->before_mount, fse->variables, 0, 0, status);

   if (mount (source, mountpoint, fstype, 0, fsdata) == -1) {
    if (errno == EBUSY) {
     if (mount (source, mountpoint, fstype, MS_REMOUNT, fsdata) == -1) goto mount_panic;
    } else {
     mount_panic:
     if (errno < sys_nerr)
      status->verbose = (char *)sys_errlist[errno];
     else
      status->verbose = "an unknown error occured while trying to mount the filesystem";
     status_update (status);
     if (fse->after_umount)
      pexec (fse->after_umount, fse->variables, 0, 0, status);
     return STATUS_FAIL;
    }
   }

   if (fse->after_mount)
    pexec (fse->after_mount, fse->variables, 0, 0, status);

   if (fse->manager)
    startdaemon (fse->manager, status);
#endif

   return STATUS_OK;
  } else {
   status->verbose = "nothing known about this mountpoint; bailing out.";
   status_update (status);
   return STATUS_FAIL;
  }
 }
 if (tflags & MOUNT_TF_MOUNT) {
 }
}

int enable (enum mounttask p, struct mfeedback *status) {
 struct uhash *ha = fstab;
 struct fstab_entry *fse;
 uint32_t i;
 switch (p) {
  case MOUNT_LOCAL:
   while (ha) {
    if (strcmp (ha->key, "/") && strcmp (ha->key, "/dev") && strcmp (ha->key, "/proc") && strcmp (ha->key, "/sys")) {
     if ((fse = (struct fstab_entry *)ha->value) && fse->options) {
      for (i = 0; fse->options[i]; i++) {
       if (!strcmp (fse->options[i], "noauto")) goto mount_local_skip;
      }
     }
     mountwrapper (ha->key, status, MOUNT_TF_MOUNT);
    }
    mount_local_skip:
    ha = hashnext (ha);
   }
   return STATUS_OK;
   break;
  case MOUNT_ROOT:
   return mountwrapper ("/", status, MOUNT_TF_MOUNT | MOUNT_TF_FORCE_RW);
   break;
  case MOUNT_DEV:
   return mountwrapper ("/dev", status, MOUNT_TF_MOUNT);
   break;
  case MOUNT_PROC:
   return mountwrapper ("/proc", status, MOUNT_TF_MOUNT);
   break;
  case MOUNT_SYS:
   return mountwrapper ("/sys", status, MOUNT_TF_MOUNT);
   break;
  default:
   status->verbose = "I'm clueless?";
   status_update (status);
   return STATUS_FAIL;
   break;
 }
}

int disable (enum mounttask p, struct mfeedback *status) {
 return STATUS_OK;
 switch (p) {
  case MOUNT_ROOT:
   status->verbose = "remounting root filesystem r/o";
   status_update (status);
   return STATUS_OK;
   break;
  default:
   status->verbose = "I'm clueless?";
   status_update (status);
   return STATUS_FAIL;
   break;
 }
}
