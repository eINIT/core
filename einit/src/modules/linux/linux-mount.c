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

/* filesystem header files */
#include <linux/ext2_fs.h>

#define MOUNT_SUPPORT_EXT2

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

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

char *provides_localmount[] = {"localmount", NULL};
char *requires_localmount[] = {"/", "/dev", NULL};
struct smodule sm_localmount = {
 EINIT_VERSION, 1, EINIT_MOD_EXEC, 0, "linux-mount [localmount]", "linux-mount-localmount", provides_localmount, requires_localmount, NULL
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
int enable (void *, struct mfeedback *);
int disable (void *, struct mfeedback *);

/* function definitions */
unsigned char read_label_linux (void *na) {
 struct uhash *element = blockdevices;
 struct ext2_super_block ext2_sb;
 struct bd_info *bdi;
 while (element) {
  FILE *device = NULL;
  device = fopen (element->key, "r");
  if (device) {
   if (fseek (device, 1024, SEEK_SET) || (fread (&ext2_sb, sizeof(struct ext2_super_block), 1, device) < 1)) {
//    perror (element->key);
   } else {
    if (ext2_sb.s_magic == EXT2_SUPER_MAGIC) {
     __u8 uuid[16];
     memcpy (uuid, ext2_sb.s_uuid, 16);
     char c_uuid[38];
     bdi = (struct bd_info *)element->value;
     if (ext2_sb.s_volume_name[0])
      bdi->label = estrdup (ext2_sb.s_volume_name);
     snprintf (c_uuid, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", /*((char)ext2_sb.s_uuid)*/ uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
     bdi->uuid = estrdup (c_uuid);
    }
   }
   fclose (device);
  }
//  else perror (element->key);
  errno = 0;
  element = hashnext (element);
 }
}

int scanmodules (struct lmodule *modchain) {
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        NULL, &sm_dev);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        NULL, &sm_root);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        NULL, &sm_localmount);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        NULL, &sm_sys);
 mod_add (NULL, (int (*)(void *, struct mfeedback *))enable,
	        (int (*)(void *, struct mfeedback *))disable,
	        NULL, &sm_proc);
}

int configure (struct lmodule *this) {
 blockdevicesupdatefunctions = hashadd (blockdevicesupdatefunctions, "dev", (void *)find_block_devices_recurse_path);
 fstabupdatefunctions = hashadd (fstabupdatefunctions, "label", (void *)forge_fstab_by_label);
 filesystemlabelupdaterfunctions = hashadd (filesystemlabelupdaterfunctions, "linux", (void *)read_label_linux);

 update_block_devices ();
 update_filesystem_labels ();
 update_fstab();
}

int cleanup (struct lmodule *this) {
}

int enable (void *pa, struct mfeedback *status) {
 return STATUS_FAIL;
}

int disable (void *pa, struct mfeedback *status) {
 return STATUS_FAIL;
}
