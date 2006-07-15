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

/* filesystem header files */
#include <linux/ext2_fs.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

/* module definitions */
char *provides[] = {"mount", NULL};
const struct smodule self = {
 EINIT_VERSION, 1, 0, 0, "Linux-specific Filesystem-Mounter Functions", "linux-mount", provides, NULL, NULL
};

/* function declarations */
unsigned char read_metadata_linux (struct uhash *);
unsigned char mount_linux_ext2 (uint32_t, char *, char *, char *, struct bd_info *, struct fstab_entry *, struct mfeedback *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);

/* function definitions */
unsigned char read_metadata_linux (struct uhash *blockdevices) {
 struct uhash *element = blockdevices;
 struct ext2_super_block ext2_sb;
 struct bd_info *bdi;
 uint32_t cdev = 0;

 while (element) {
  FILE *device = NULL;
  bdi = (struct bd_info *)element->value;
  cdev++;

//  printf ("\e[K\e[sscanning device #%i: %s\e[u", cdev, element->key);
  printf ("\e[255D\e[Kscanning device #%i: %s... ", cdev, element->key);
//  printf ("scanning device #%i: %s", cdev, element->key);
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

 puts ("done");
 return 0;
}

unsigned char mount_linux_ext2 (uint32_t tflags, char *source, char *mountpoint, char *fstype, struct bd_info *bdi, struct fstab_entry *fse, struct mfeedback *status) {
 void *fsdata = NULL;

#ifndef SANDBOX
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
   return 1;
  }
 }
#endif

 return 0;
}

int configure (struct lmodule *this) {
 struct einit_event *ev = ecalloc (1, sizeof(struct einit_event));

 function_register ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
 function_register ("fs-mount-ext2", 1, (void *)mount_linux_ext2);
 function_register ("fs-mount-ext3", 1, (void *)mount_linux_ext2);

 ev->type = EINIT_EVENT_TYPE_MOUNT_UPDATE;
 ev->flag = EVENT_UPDATE_METADATA;
 event_emit (ev, EINIT_EVENT_FLAG_BROADCAST);
// free (ev);
}

int cleanup (struct lmodule *this) {
 function_unregister ("fs-read-metadata-linux", 1, (void *)read_metadata_linux);
 function_unregister ("fs-mount-ext2", 1, (void *)mount_linux_ext2);
 function_unregister ("fs-mount-ext3", 1, (void *)mount_linux_ext2);
}
