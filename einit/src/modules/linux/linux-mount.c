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

#include <sys/mount.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/common-mount.h>
#include <einit/event.h>
#include <linux/fs.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

/* filesystem header files */
#include <linux/ext2_fs.h>
#include <linux/ext3_fs.h>

/* NOTE: i seem to have trouble #include-ing the reiserfs-headers, and since i don't want to #include an
 excessive amount of headers just to read a filesystem's label, i'll be a cheapo and just create a very
 simple struct that only contains the information that i really need */

struct reiserfs_super_block {
 uint32_t s_block_count;
 char na_1[40];
 uint16_t s_blocksize;
 char na_2[4];
 uint16_t s_umount_state;
 char s_magic[10]; /* i kept the original designators where appropriate */
 uint16_t s_fs_state;
 char na_3[0x14];
 char s_uuid[16];
 char s_label[16];
 char s_unused[88];
};

/* now that i think about it, i might as well do the same for ext2/3 */

#if 0
#include <linux/nfs.h>
#include <linux/nfs_mount.h>
#include <sys/socket.h>

/* these should be included, but it works without 'em and compilation breaks on my system if i include them */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <einit/pexec.h>
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
	.mode		= 0,
	.options	= 0,
	.name		= "Linux-specific Filesystem-Mounter Functions",
	.rid		= "linux-mount",
	.provides	= provides,
	.requires	= NULL,
	.notwith	= NULL
};

/* function declarations */
unsigned char read_metadata_linux (struct mount_control_block *);
unsigned char mount_linux_ext2 (uint32_t, char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);
//unsigned char mount_linux_nfs (uint32_t, char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);
unsigned char mount_linux_real_mount (uint32_t, char *, char *, char *, struct bd_info *, struct fstab_entry *, struct einit_event *);
unsigned char find_block_devices_proc (struct mount_control_block *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);

/* function definitions */
unsigned char read_metadata_linux (struct mount_control_block *mcb) {
 struct uhash *element = mcb->blockdevices;
 struct ext2_super_block ext2_sb;
 struct reiserfs_super_block reiser_sb;
 struct bd_info *bdi;
 uint32_t cdev = 0;
 __u8 uuid[16];
 char c_uuid[38];
 char tmp[1024];

 if (!element) return 1;

 while (element) {
  FILE *device = NULL;
  bdi = (struct bd_info *)element->value;
  cdev++;

//  printf ("\e[K\e[sscanning device #%i: %s\e[u", cdev, element->key);
//  printf ("\e[255D\e[Kscanning device #%i: %s... ", cdev, element->key);
//  printf ("scanning device #%i: %s", cdev, element->key);
  device = fopen (element->key, "r");
  if (device) {
   c_uuid[0] = 0;
   if (fseek (device, 1024, SEEK_SET) || (fread (&ext2_sb, sizeof(struct ext2_super_block), 1, device) < 0)) {
//    perror (element->key);
    bdi->status = BF_STATUS_ERROR_IO;
   } else {
    if (ext2_sb.s_magic == EXT2_SUPER_MAGIC) {
#if 0
/* this doesn't work properly with actual ext2 devices (as opposed to ext3 ones) */

      if (fseek (device, (uintmax_t)((uintmax_t)(ext2_sb.s_blocks_count -1) * (uintmax_t)ext2_sb.s_log_block_size), SEEK_SET) || fread (&tmp, (ext2_sb.s_log_block_size < 1024 ? ext2_sb.s_log_block_size : 1024), 1, device) <= 0) { // verify that the device is actually large enough (raid, anyone?)
       fprintf (stderr, "%s: EXT2/3 superblock found, but blockdevice not large enough (%i*%i): invalid superblock or raw RAID device\n", element->key, ext2_sb.s_blocks_count, ext2_sb.s_log_block_size);
       bdi->status = BF_STATUS_ERROR_IO;
      } else
#endif
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
//    perror (element->key);
      bdi->status = BF_STATUS_ERROR_IO;
     } else if (!strcmp (reiser_sb.s_magic, "ReIsErFs") || !strcmp (reiser_sb.s_magic, "ReIsEr2Fs") || !strcmp (reiser_sb.s_magic, "ReIsEr3Fs")) {
      if (fseek (device, (uintmax_t)((uintmax_t)(reiser_sb.s_block_count -1) * (uintmax_t)reiser_sb.s_blocksize), SEEK_SET) || fread (&tmp, (reiser_sb.s_blocksize < 1024 ? reiser_sb.s_blocksize : 1024), 1, device) <= 0) { // verify that the device is actually large enough (raid, anyone?)
       fprintf (stderr, "%s: ReiserFS superblock found, but blockdevice not large enough (%i*%i): invalid superblock or raw RAID device\n", element->key, reiser_sb.s_block_count, reiser_sb.s_blocksize);
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
//    if (bdi->label) fputs (bdi->label, stdout);
   }

   if (bdi->fs_type) {
    snprintf (c_uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", /*((char)ext2_sb.s_uuid)*/ uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    bdi->uuid = estrdup (c_uuid);
   }
   fclose (device);
  } else {
   bdi->status = BF_STATUS_ERROR_IO;
//   perror (element->key);
  }
  errno = 0;
  element = hashnext (element);
 }

// puts ("done");
 return 0;
}

unsigned char find_block_devices_proc (struct mount_control_block *mcb) {
 FILE *f = fopen ("/proc/partitions", "r");
 char tmp[1024];
 uint32_t line = 0, device_major = 0, device_minor = 0, device_size = 0, field = 0;
 char *device_name = NULL;
 if (!f) return 1;

 errno = 0;
 while (!errno) {
  if (!fgets (tmp, 1024, f)) {
   switch (errno) {
    case EINTR:
    case EAGAIN:
     errno = 0;
     break;
    case 0:
     return 1;
    default:
     bitch(BTCH_ERRNO);
     return 1;
   }
  } else {
   line++;
   if (line <= 2) continue;

   if (tmp[0]) {
    char *cur = estrdup (tmp);
    char *scur = cur;
    uint32_t icur = 0;
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
//    puts (tmp);
    strcpy (tmp, "/dev/");
    strcat (tmp, device_name);
//    printf ("parsed: device=%s (%i/%i) [%i blocks]\n", tmp, device_major, device_minor, device_size);
    mcb->add_block_device (tmp, device_major, device_minor);
   }
  }
 }

 return 0;
}

// this function is currently completely useless
/*
unsigned char mount_linux_ext2 (uint32_t tflags, char *source, char *mountpoint, char *fstype, struct bd_info *bdi, struct fstab_entry *fse, struct einit_event *status) {
 char *fsdata = NULL;

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

#ifndef SANDBOX
 if (mount (source, mountpoint, fstype, 0, fsdata) == -1) {
  if (errno == EBUSY) {
   if (mount (source, mountpoint, fstype, MS_REMOUNT, fsdata) == -1) goto mount_panic;
  } else {
   mount_panic:
   if (errno < sys_nerr)
    status->string = (char *)sys_errlist[errno];
   else
    status->string = "an unknown error occured while trying to mount the filesystem";
   status_update (status);
   if (fse->after_umount)
    pexec (fse->after_umount, fse->variables, 0, 0, NULL, status);
   return 1;
  }
 }
#endif

 return 0;
}
*/

#if 0
unsigned char mount_linux_nfs (uint32_t tflags, char *source, char *mountpoint, char *fstype, struct bd_info *bdi, struct fstab_entry *fse, struct einit_event *status) {
/* this doesn't really work just now, so we'll delegate it to a real mount command for the time being */
 char **hnrd;
 struct nfs_mount_data *data = ecalloc (1, sizeof(struct nfs_mount_data));
 struct sockaddr_in server;
 struct addrinfo *lres = NULL;
/* struct addrinfo reshints = {
  reshints.ai_family = AF_INET
 };*/

 hnrd = str2set (':', source);
 if (!hnrd || !hnrd[0] || !hnrd[1] || hnrd[2]) {
  status->string = "mount_linux_nfs: there's something wrong with your source specification";
  status_update (status);
  return 1;
 }
 status->string = hnrd[0];
 status_update (status);

 if (getaddrinfo(hnrd[0], NULL, NULL, &lres)) {
  status->string = "mount_linux_nfs: can't resolve host";
  status_update (status);
  return 1;
 }

// puts (lres->ai_family);

/* if (fse->options) {
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
 }*/

 freeaddrinfo (lres);

/* vers=3,rsize=32768,wsize=32768,hard,proto=udp,timeo=7,retrans=3,sec=sys,addr=chronos */
#ifndef SANDBOX
 if (mount (source, mountpoint, fstype, 0, data) == -1) {
  if (errno == EBUSY) {
   if (mount (source, mountpoint, fstype, MS_REMOUNT, data) == -1) goto mount_panic;
  } else {
   mount_panic:
   if (errno < sys_nerr)
    status->string = (char *)sys_errlist[errno];
   else
    status->string = "an unknown error occured while trying to mount the filesystem";
   status_update (status);
//   if (fse->after_umount)
//    pexec (fse->after_umount, fse->variables, 0, 0, NULL, status);
   return 1;
  }
 }
#endif

 free (hnrd);

 return 0;
}
#endif

unsigned char mount_linux_real_mount (uint32_t tflags, char *source, char *mountpoint, char *fstype, struct bd_info *bdi, struct fstab_entry *fse, struct einit_event *status) {
 char *fsdata = NULL;
 char command[4096];

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
  if (tflags & MOUNT_TF_FORCE_RW)
   snprintf (command, 4096, "/bin/mount %s %s -t %s -o \"rw,%s\"", source, mountpoint, fstype, fsdata);
  else if (tflags & MOUNT_TF_FORCE_RO)
   snprintf (command, 4096, "/bin/mount %s %s -t %s -o \"ro,%s\"", source, mountpoint, fstype, fsdata);
  else
   snprintf (command, 4096, "/bin/mount %s %s -t %s -o \"%s\"", source, mountpoint, fstype, fsdata);
 } else {
  if (tflags & MOUNT_TF_FORCE_RW)
   snprintf (command, 4096, "/bin/mount %s %s -t %s -o rw", source, mountpoint, fstype);
  else if (tflags & MOUNT_TF_FORCE_RO)
   snprintf (command, 4096, "/bin/mount %s %s -t %s -o ro", source, mountpoint, fstype);
  else
   snprintf (command, 4096, "/bin/mount %s %s -t %s", source, mountpoint, fstype);
 }

#ifndef SANDBOX
 if (pexec (command, NULL, 0, 0, NULL, status) == STATUS_OK)
  return 0;
 else {
  if (fse->after_umount)
   pexec (fse->after_umount, fse->variables, 0, 0, NULL, status);
  return 1;
 }
#else
 status->string = command;
 status_update (status);
#endif

 return 0;
}

int configure (struct lmodule *this) {
/* pexec configuration */
 pexec_configure (this);

 struct einit_event *ev = evinit (EINIT_EVENT_TYPE_MOUNT_UPDATE);

 function_register ("find-block-devices-proc", 1, (void *)find_block_devices_proc);
 function_register ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
// function_register ("fs-mount-ext2", 1, (void *)mount_linux_ext2);
// function_register ("fs-mount-ext3", 1, (void *)mount_linux_ext2);
// function_register ("fs-mount-nfs", 1, (void *)mount_linux_nfs);
/* nfs mounting is a real, royal PITA. we'll use the regular /bin/mount command for the time being */
 function_register ("fs-mount-nfs", 1, (void *)mount_linux_real_mount);

 event_emit (ev, EINIT_EVENT_FLAG_BROADCAST);
 evdestroy (ev);
}

int cleanup (struct lmodule *this) {
 function_unregister ("find-block-devices-proc", 1, (void *)find_block_devices_proc);
 function_unregister ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
 function_unregister ("fs-mount-nfs", 1, (void *)mount_linux_real_mount);
// function_unregister ("fs-mount-ext2", 1, (void *)mount_linux_ext2);
// function_unregister ("fs-mount-ext3", 1, (void *)mount_linux_ext2);
}
