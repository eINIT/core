/*
 *  linux-mount.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/05/2006.
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

#include <sys/mount.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/event.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include <einit-modules/mount.h>
#include <einit-modules/exec.h>

/* filesystem header files */
/* i kept the original designators where appropriate */
/* NOTE: i seem to have trouble #include-ing the reiserfs-headers, and since i don't want to #include an
 excessive amount of headers just to read a filesystem's label, i'll be a cheapo and just create a very
 simple struct that only contains the information that i really need */

struct reiserfs_super_block {
 uint32_t s_block_count;
 char     na_1[40];
 uint16_t s_blocksize;
 char     na_2[4];
 uint16_t s_umount_state;
 char     s_magic[10];
 uint16_t s_fs_state;
 char     na_3[0x14];
 char     s_uuid[16];
 char     s_label[16];
 char     s_unused[88];
};

/* now that i think about it, i might as well do the same for ext2/3:
   (this should fix a reported bug about this module not compiling properly) */
struct ext2_super_block {
 uint32_t s_inodes_count;
 uint32_t s_blocks_count;
 uint32_t s_r_blocks_count;
 uint32_t na_1[3];
 uint32_t s_log_block_size;
 uint32_t s_log_frag_size;
 uint32_t s_blocks_per_group;
 uint32_t s_frags_per_group;
 uint32_t s_inodes_per_group;
 uint32_t s_mtime;
 uint32_t s_wtime;
 uint16_t s_mnt_count;
 uint16_t s_max_mnt_count;
 uint16_t s_magic;
 uint16_t s_state;
 uint16_t s_errors;
 uint16_t s_minor_rev_level;
 uint32_t s_lastcheck;
 uint32_t s_checkinterval;
 uint32_t s_creator_os;
 uint32_t s_rev_level;
 uint16_t na_2[12];
 uint8_t  s_uuid[16];
 char     s_volume_name[16];
 char     s_last_mounted[64];
 uint32_t s_reserved[222];
};

// from ext2 kernel header files
#define EXT2_GOOD_OLD_REV 0
#define EXT2_DYNAMIC_REV  1
#define EXT2_VALID_FS     0x0001
#define EXT2_ERROR_FS     0x0002
#define EXT2_SUPER_MAGIC  0xEF53


#if 0
#include <linux/nfs.h>
#include <linux/nfs_mount.h>
#include <sys/socket.h>

/* these should be included, but it works without 'em and compilation breaks on my system if i include them */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_mount_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
/* module definitions */
const struct smodule module_linux_mount_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "Linux-specific Filesystem-Mounter Functions",
 .rid       = "linux-mount",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_mount_configure
};

module_register(module_linux_mount_self);

#endif

/* function declarations */
unsigned char read_metadata_linux (struct mount_control_block *);
unsigned char mount_linux_real_mount (char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);
unsigned char find_block_devices_proc (struct mount_control_block *);
int linux_mount_configure (struct lmodule *);
int linux_mount_cleanup (struct lmodule *);

/* function definitions */
unsigned char read_metadata_linux (struct mount_control_block *mcb) {
 struct stree *element = mcb->blockdevices;
 struct ext2_super_block ext2_sb;
 struct reiserfs_super_block reiser_sb;
 struct bd_info *bdi;
 uint32_t cdev = 0;
 uint8_t uuid[16];
 char c_uuid[38];
 char tmp[BUFFERSIZE];

 if (coremode == einit_mode_sandbox) return 0;

 if (!element) return 1;

 while (element) {
  FILE *device = NULL;
  bdi = (struct bd_info *)element->value;
  cdev++;
  struct stat statbuf;

  if (stat(element->key, &statbuf)) { /* see if we can stat the file */
   bdi->status = BF_STATUS_ERROR_IO;
  } else {
   device = fopen (element->key, "r");
   if (device) {
    c_uuid[0] = 0;
    if (fseek (device, 1024, SEEK_SET) || (fread (&ext2_sb, sizeof(struct ext2_super_block), 1, device) < 0)) {
     bdi->status = BF_STATUS_ERROR_IO;
    } else {
     if (ext2_sb.s_magic == EXT2_SUPER_MAGIC) {
       {
        bdi->fs_type = FILESYSTEM_EXT2;
        bdi->status = BF_STATUS_HAS_MEDIUM;

/* checks to see if the filesystem is dirty */
        if (ext2_sb.s_mnt_count >= ext2_sb.s_max_mnt_count) // the "maximum mount count"-thing
         bdi->status |= BF_STATUS_DIRTY;

        if (ext2_sb.s_lastcheck + ext2_sb.s_checkinterval >= time(NULL)) // the "check-interval"-thing
         bdi->status |= BF_STATUS_DIRTY;

        if (ext2_sb.s_state != EXT2_VALID_FS) // the current filesystem state
         bdi->status |= BF_STATUS_DIRTY;

        if (ext2_sb.s_rev_level == EXT2_DYNAMIC_REV) {
         memcpy (uuid, ext2_sb.s_uuid, 16);
         if (ext2_sb.s_volume_name[0])
          bdi->label = estrdup (ext2_sb.s_volume_name);
       }
      }
     } else {
      if (fseek (device, 64*1024, SEEK_SET) || (fread (&reiser_sb, sizeof(struct reiserfs_super_block), 1, device) < 0)) {
       bdi->status = BF_STATUS_ERROR_IO;
      } else if (strmatch (reiser_sb.s_magic, "ReIsErFs") || strmatch (reiser_sb.s_magic, "ReIsEr2Fs") || strmatch (reiser_sb.s_magic, "ReIsEr3Fs")) {
       if (fseek (device, (uintmax_t)((uintmax_t)(reiser_sb.s_block_count -1) * (uintmax_t)reiser_sb.s_blocksize), SEEK_SET) || fread (&tmp, (reiser_sb.s_blocksize < 1024 ? reiser_sb.s_blocksize : 1024), 1, device) <= 0) { // verify that the device is actually large enough (raid, anyone?)
#ifdef DEBUG
        eprintf (stderr, "%s: ReiserFS superblock found, but blockdevice not large enough (%i*%i): invalid superblock or raw RAID device\n", element->key, reiser_sb.s_block_count, reiser_sb.s_blocksize);
#endif
        bdi->status = BF_STATUS_ERROR_IO;
       } else {
        bdi->fs_type = FILESYSTEM_REISERFS;
        bdi->status = BF_STATUS_HAS_MEDIUM;

        if (reiser_sb.s_umount_state == 2) // 1 means cleanly unmounted, 2 means the device wasn't unmounted
         bdi->status |= BF_STATUS_DIRTY;

        if (reiser_sb.s_label[0])
         bdi->label = estrdup (reiser_sb.s_label);

        memcpy (uuid, reiser_sb.s_uuid, 16);
       }
      }
     }
    }

    if (bdi->fs_type) {
     esprintf (c_uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", /*((char)ext2_sb.s_uuid)*/ uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
     bdi->uuid = estrdup (c_uuid);
    }
    efclose (device);
   } else {
    bdi->status = BF_STATUS_ERROR_IO;

    switch (errno) {
     case ENODEV:
     case ENXIO:
     case ENOMEDIUM:
      break;
     default:
      bitch(bitch_stdio, errno, "opening file failed.");
    }
   }
  }
  errno = 0;
  element = streenext (element);
 }

 return 0;
}

unsigned char find_block_devices_proc (struct mount_control_block *mcb) {
 FILE *f = efopen ("/proc/partitions", "r");
 char tmp[BUFFERSIZE];
 uint32_t line = 0, device_major = 0, device_minor = 0, device_size = 0, field = 0;
 char *device_name = NULL;
 if (!f) return 1;

 errno = 0;
 while (!errno) {
  if (!fgets (tmp, BUFFERSIZE, f)) {
   switch (errno) {
    case EINTR:
    case EAGAIN:
     errno = 0;
     break;
    case 0:
     efclose (f);
     return 1;
    default:
     bitch(bitch_stdio, 0, "fgets() failed.");
     efclose (f);
     return 1;
   }
  } else {
   line++;
   if (line <= 2) continue;

   if (tmp[0]) {
    char *cur = estrdup (tmp);
    char *scur = cur;
    field = 0;
    strtrim (cur);
    for (; *cur; cur++) {
     if (isspace (*cur)) {
      *cur = 0;
      field++;
      switch (field) {
       case 1: device_major = (int) strtol(scur, (char **)NULL, 10); break;
       case 2: device_minor = (int) strtol(scur, (char **)NULL, 10); break;
       case 3: device_size = (int) strtol(scur, (char **)NULL, 10); break;
       case 4: device_name = scur; break;
      }
      scur = cur+1;
      strtrim (scur);
     }
    }
    if (cur != scur) {
     field++;
     switch (field) {
      case 1: device_major = (int) strtol(scur, (char **)NULL, 10); break;
      case 2: device_minor = (int) strtol(scur, (char **)NULL, 10); break;
      case 3: device_size = (int) strtol(scur, (char **)NULL, 10); break;
      case 4: device_name = scur; break;
     }
    }
    strcpy (tmp, "/dev/");
    strncat (tmp, device_name, sizeof(tmp) - strlen (tmp) + 1);
    mcb->add_block_device (tmp, device_major, device_minor);
   }
  }
 }

 efclose (f);
 return 0;
}

unsigned char mount_linux_real_mount (char *source, char *mountpoint, char *fstype, struct bd_info *bdi, struct fstab_entry *fse, struct einit_event *status) {
 char *fsdata = NULL;
 char command[BUFFERSIZE];

 if (fse->options) {
  int fi = 0;
  for (; fse->options[fi]; fi++) {
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

 if (fsdata) {
  esprintf (command, BUFFERSIZE, "/bin/mount %s %s -t %s -o \"%s\"", source, mountpoint, fstype, fsdata);
 } else {
  esprintf (command, BUFFERSIZE, "/bin/mount %s %s -t %s", source, mountpoint, fstype);
 }

 if (coremode != einit_mode_sandbox) {
  if (pexec_v1 (command, NULL, NULL, status) == STATUS_OK)
   return 0;
  else {
   if (fse->after_umount)
    pexec_v1 (fse->after_umount, (const char **)fse->variables, NULL, status);
   return 1;
  }
 } else {
  status->string = command;
  status_update (status);
 }

 return 0;
}

int linux_mount_cleanup (struct lmodule *this) {
 function_unregister ("find-block-devices-proc", 1, (void *)find_block_devices_proc);
 function_unregister ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
 function_unregister ("fs-mount-nfs", 1, (void *)mount_linux_real_mount);

 exec_cleanup(this);

 return 0;
}

int linux_mount_configure (struct lmodule *this) {
 module_init (this);

 thismodule->cleanup = linux_mount_cleanup;

/* pexec configuration */
 exec_configure (this);

 struct einit_event *ev = evinit (einit_mount_do_update);

 function_register ("find-block-devices-proc", 1, (void *)find_block_devices_proc);
 function_register ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
// function_register ("fs-mount-nfs", 1, (void *)mount_linux_nfs);
/* nfs mounting is a real, royal PITA. we'll use the regular /bin/mount command for the time being */
 function_register ("fs-mount-nfs", 1, (void *)mount_linux_real_mount);

 event_emit (ev, einit_event_flag_broadcast);
 evdestroy (ev);

 return 0;
}
