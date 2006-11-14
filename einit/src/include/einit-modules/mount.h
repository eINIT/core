/*
 *  mount.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 07/06/2006.
 *  Moved and renamed from common-mount.h on 20/10/2006.
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

/*!\file einit/common-mount.h
 * \brief Header file used by the *-mount modules.
 * \author Magnus Deininger
 *
 * This header file defines functions and data types that are used by the common-mount.c and linux-mount.c
 * modules.
*/
#ifndef _EINIT_MODULES_MOUNT_H
#define _EINIT_MODULES_MOUNT_H

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
#include <einit/dexec.h>

/*!\name Filesystem Capabilities
 * \bug not used
 \{
*/
#define FS_CAPA_RW                      0x0001     /*!< Filesystem Capability: can be mounted RW */
#define FS_CAPA_VOLATILE                0x0002     /*!< Filesystem Capability: changes are lost */
#define FS_CAPA_NETWORK                 0x0004     /*!< Filesystem Capability: network filesystem */
/*!\}*/

/*!\name Status Bitfield
 * \bug only partially used and set appropriately
 \{
*/
#define BF_STATUS_ERROR                 0xf0000000 /*!< Status Bitmask: errors occured */
#define BF_STATUS_ERROR_IO              0x10000000 /*!< Status Bit: I/O errors */
#define BF_STATUS_ERROR_NOTINIT         0x20000000 /*!< Status Bit: blockdevice not yet initialised */
#define BF_STATUS_MOUNTED               0x00000001 /*!< Status Bit: blockdevice mounted */
#define BF_STATUS_DIRTY                 0x00000002 /*!< Status Bit: blockdevice dirty (run fsck) */
#define BF_STATUS_HAS_MEDIUM            0x00000004 /*!< Status Bit: blockdevice has medium */
/*!\}*/

/*!\name Mountwrapper Task Bitfield
 * \bug only partially used and set appropriately
 \{
*/
#define MOUNT_TF_MOUNT                  0x0001     /*!< Mountwrapper Task: mount */
#define MOUNT_TF_UMOUNT                 0x0002     /*!< Mountwrapper Task: unmount */
#define MOUNT_TF_FORCE_RW               0x0010
/*!< Mountwrapper Task Option: force R/W
   \bug not implemented */
#define MOUNT_TF_FORCE_RO               0x0020
/*!< Mountwrapper Task Option: force R/O
   \bug not implemented */
/*!\}*/

/*!\name FSTab Options
 \{*/
#define MOUNT_FSTAB_NOAUTO              0x0001
#define MOUNT_FSTAB_CRITICAL            0x0002
/*!\}*/

/*!\name Mount data update requests
 \{*/
#define EVENT_UPDATE_METADATA           0x01
#define EVENT_UPDATE_BLOCK_DEVICES      0x02
#define EVENT_UPDATE_FSTAB              0x04
#define EVENT_UPDATE_MTAB               0x08

/* definitions */
enum update_task {
 UPDATE_METADATA      = 0x01,
 UPDATE_BLOCK_DEVICES = 0x02,
 UPDATE_FSTAB         = 0x03,
 UPDATE_MTAB          = 0x04
};
/*!\}*/

/*!\name Mount module parametres
 \{*/
enum mounttask {
 MOUNT_ROOT = 1,
 MOUNT_SYSTEM = 4,
 MOUNT_LOCAL = 5,
 MOUNT_REMOTE = 6,
 MOUNT_CRITICAL = 8
};
/*!\}*/

enum known_filesystems {
 FILESYSTEM_UNKNOWN = 0x0000,
 FILESYSTEM_EXT2 = 0x0001,
 FILESYSTEM_EXT3 = 0x0002,
 FILESYSTEM_REISERFS = 0x0003,
 FILESYSTEM_REISER4 = 0x0004,
 FILESYSTEM_JFS = 0x0005,
 FILESYSTEM_XFS = 0x0006,
 FILESYSTEM_UFS = 0x0007,
 FILESYSTEM_OTHER = 0xffff
};

/* struct definitions */
struct bd_info {
 char *label, *uuid, *fs;
 uint32_t major, minor;
 enum known_filesystems fs_type;
 uint32_t status;
};

struct fstab_entry {
 char *mountpoint, *device, *fs;
 char **options;
 uint32_t mountflags;
 char *before_mount;
 char *after_mount;
 char *before_umount;
 char *after_umount;
 struct dexecinfo *manager;
 char **variables;
 uint32_t status;
};

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
 struct stree *mtab;
 struct stree *filesystems;
 void (*add_block_device) (char *, uint32_t, uint32_t);
 void (*add_fstab_entry) (char *, char *, char *, char **, uint32_t, char *, char *, char *, char *, char *, uint32_t, char **);
 void (*add_mtab_entry) (char *, char *, char *, char *, uint32_t, uint32_t);
 void (*add_filesystem) (char *, char *);
 uint32_t update_options;
 char **critical;
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
void update (enum update_task);
#define update_filesystem_metadata() update (UPDATE_METADATA)
#define update_block_devices() update (UPDATE_BLOCK_DEVICES)
#define update_fstab() update (UPDATE_FSTAB)
#define update_mtab() update (UPDATE_MTAB)

void mount_ipc_handler(struct einit_event *);
void mount_update_handler(struct einit_event *);

void add_block_device (char *, uint32_t, uint32_t);
void add_fstab_entry (char *, char *, char *, char **, uint32_t, char *, char *, char *, char *, char *, uint32_t, char **);
void add_mtab_entry (char *, char *, char *, char *, uint32_t, uint32_t);
void add_filesystem (char *, char *);

struct stree *read_fsspec_file (char *);

#endif
