/*
 *  mount.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 07/06/2006.
 *  Moved and renamed from common-mount.h on 20/10/2006.
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

#ifdef __cplusplus
extern "C" {
#endif

/*!\file einit/common-mount.h
 * \brief Header file used by the *-mount modules.
 * \author Magnus Deininger
 *
 * This header file defines functions and data types that are used by the common-mount.c and linux-mount.c
 * modules.
*/
#ifndef EINIT_MODULES_MOUNT_H
#define EINIT_MODULES_MOUNT_H

#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit/tree.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <einit-modules/exec.h>

/*!\name Filesystem Capabilities
 * \bug not used
 \{
*/
enum filesystem_capability {
 filesystem_capability_rw       = 0x1,
/*!< Filesystem Capability: can be mounted RW */
 filesystem_capability_volatile = 0x2,
/*!< Filesystem Capability: changes are lost */
 filesystem_capability_network  = 0x4
/*!< Filesystem Capability: network filesystem */
};
/*!\}*/

/*!\name Status Bitfield
 * \bug only partially used and set appropriately
 \{
*/
#define BF_STATUS_ERROR                 0xf000 /*!< Status Bitmask: errors occured */

enum device_status {
 device_status_mounted      = 0x0001,
/*!< Status Bit: blockdevice mounted */
 device_status_dirty        = 0x0002,
/*!< Status Bit: blockdevice dirty (run fsck) */
 device_status_has_medium   = 0x0004,
/*!< Status Bit: blockdevice has medium */

 device_status_error_io     = 0x1000,
/*!< Status Bit: I/O errors */
 device_status_error_notint = 0x2000
/*!< Status Bit: blockdevice not yet initialised */
};
/*!\}*/

/*!\name FSTab Options
 \{*/
enum mount_fstab_options {
 mount_fstab_noauto   = 0x1,
 mount_fstab_critical = 0x2
};
/*!\}*/

/*!\name Mount data update requests
 \{*/
enum mount_options {
 mount_update_metadata      = 0x01,
 mount_update_block_devices = 0x02,
 mount_update_fstab         = 0x04,
 mount_update_mtab          = 0x08,
 mount_maintain_mtab        = 0x10
};

/*!\}*/

/* struct definitions */
struct legacy_fstab_entry {
 char *fs_spec;
 char *fs_file;
 char *fs_vfstype;
 char *fs_mntops;
 uint32_t fs_freq;
 uint32_t fs_passno;
};

struct mount_control_block {
 struct stree *blockdevices;
 struct stree *fstab;
 struct stree *filesystems;
 void (*add_block_device) (char *, uint32_t, uint32_t);
 void (*add_fstab_entry) (char *, char *, char *, char **, uint32_t, char *, char *, char *, char *, char *, uint32_t, char **);
 void (*add_mtab_entry) (char *, char *, char *, char *, uint32_t, uint32_t);
 void (*add_filesystem) (char *, char *);
 uint32_t update_options;

 enum mount_options options;

 char **critical;
 char **noumount;
 char *mtab_file;
};

/* new block: */

struct mountpoint_data {
 char *mountpoint;

 char *fs;

 char **options;
 char *flatoptions;
 unsigned long mountflags;

 char *before_mount;
 char *after_mount;
 char *before_umount;
 char *after_umount;
 struct dexecinfo *manager;
 char **variables;

 uint32_t status;
};

struct device_data {
 struct stree *mountpoints; // values must be of type struct mountpoint_data
 char *device;

 enum device_status device_status;

 char *fs;
 char *label;
 char *uuid;
 char *encryption;
 char *encryption_key;
};

typedef int (*einit_mount_function) (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);

typedef int (*einit_umount_function) (char *, char *, char, struct device_data *, struct mountpoint_data *, struct einit_event *);

#endif

#ifdef __cplusplus
}
#endif
