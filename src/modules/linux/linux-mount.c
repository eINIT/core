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

#include <sys/swap.h>

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
unsigned char find_block_devices_proc (struct mount_control_block *);
int linux_mount_configure (struct lmodule *);
int linux_mount_cleanup (struct lmodule *);

int linux_mount_do_mount_real (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int linux_mount_do_mount_ntfs_3g (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int linux_mount_do_mount_swap (char *, char *, struct device_data *, struct mountpoint_data *, struct einit_event *);
int linux_mount_do_umount_swap (char *, char *, char, struct device_data *, struct mountpoint_data *, struct einit_event *);

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

int linux_mount_do_mount_real (char *mountpoint, char *fs, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 fbprintf (status, "mounting %s on %s (fs=%s; using native mount command)", dd->device, mountpoint, fs);

 char *fsdata = NULL;
 char command[BUFFERSIZE];

 if (mp->options) {
  int fi = 0;
  for (; mp->options[fi]; fi++) {
   if (strmatch (mp->options[fi], "auto") || strmatch (mp->options[fi], "noauto") || strmatch (mp->options[fi], "system") || strmatch (mp->options[fi], "critical") || strmatch (mp->options[fi], "network") || strmatch (mp->options[fi], "skip-fsck")) ; // ignore our own specifiers, as well as auto/noauto
   else if (!fsdata) {
    uint32_t slen = strlen (mp->options[fi])+1;
    fsdata = ecalloc (1, slen);
    memcpy (fsdata, mp->options[fi], slen);
   } else {
    uint32_t fsdl = strlen(fsdata) +1, slen = strlen (mp->options[fi])+1;
    fsdata = erealloc (fsdata, fsdl+slen);
    *(fsdata + fsdl -1) = ',';
    memcpy (fsdata+fsdl, mp->options[fi], slen);
   }
  }
 }

 if (fsdata) {
  esprintf (command, BUFFERSIZE, "/bin/mount %s %s -t %s -o %s", dd->device, mountpoint, fs, fsdata);
 } else {
  esprintf (command, BUFFERSIZE, "/bin/mount %s %s -t %s", dd->device, mountpoint, fs);
 }

 return pexec_simple (command, status);
}

int linux_mount_do_mount_ntfs_3g (char *mountpoint, char *fs, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 fbprintf (status, "mounting %s on %s (fs=%s; using specialised ntfs-3g command)", dd->device, mountpoint, fs);

 char *fsdata = NULL;
 char command[BUFFERSIZE];

 if (mp->options) {
  int fi = 0;
  for (; mp->options[fi]; fi++) {
   if (strmatch (mp->options[fi], "auto") || strmatch (mp->options[fi], "noauto") || strmatch (mp->options[fi], "system") || strmatch (mp->options[fi], "critical") || strmatch (mp->options[fi], "network") || strmatch (mp->options[fi], "skip-fsck")) ; // ignore our own specifiers, as well as auto/noauto
   else if (!fsdata) {
    uint32_t slen = strlen (mp->options[fi])+1;
    fsdata = ecalloc (1, slen);
    memcpy (fsdata, mp->options[fi], slen);
   } else {
    uint32_t fsdl = strlen(fsdata) +1, slen = strlen (mp->options[fi])+1;
    fsdata = erealloc (fsdata, fsdl+slen);
    *(fsdata + fsdl -1) = ',';
    memcpy (fsdata+fsdl, mp->options[fi], slen);
   }
  }
 }

 if (fsdata) {
  esprintf (command, BUFFERSIZE, "/bin/ntfs-3g %s %s -t %s -o %s", dd->device, mountpoint, fs, fsdata);
 } else {
  esprintf (command, BUFFERSIZE, "/bin/ntfs-3g %s %s -t %s", dd->device, mountpoint, fs);
 }

 return pexec_simple (command, status);
}

int linux_mount_do_mount_swap (char *mountpoint, char *fs, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 fbprintf (status, "using %s as swap (using swapon()-syscall)", dd->device);

 if (swapon (dd->device, 0) == -1) {
  fbprintf (status, "error while calling swapon(): %s", strerror (errno));

  return status_failed;
 }

 return status_ok;
}

int linux_mount_do_umount_swap (char *mountpoint, char *fs, char step, struct device_data *dd, struct mountpoint_data *mp, struct einit_event *status) {
 fbprintf (status, "disabling swapspace at %s (using swapoff()-syscall)", dd->device);

 if (swapoff (dd->device) == -1) {
  fbprintf (status, "error while calling swapoff(): %s", strerror (errno));

  return status_failed;
 }

 return status_ok;
}

int linux_mount_update_nfs (struct lmodule *lm, struct smodule *sm, struct device_data *dd, struct mountpoint_data *mp) {
 if (!inset ((const void **)sm->si.requires, "network", SET_TYPE_STRING)) {
  sm->si.requires = (char **)setadd ((void **)sm->si.requires, "network", SET_TYPE_STRING);
 }

 if (mp->options) {
  if (!inset ((const void **)mp->options, "nolock", SET_TYPE_STRING)) {
//   if (!inset ((const void **)sm->si.requires, "sm-notify", SET_TYPE_STRING)) {
//    sm->si.requires = (char **)setadd ((void **)sm->si.requires, "sm-notify", SET_TYPE_STRING);
//   }
   if (!inset ((const void **)sm->si.requires, "rpc.statd", SET_TYPE_STRING)) {
    sm->si.requires = (char **)setadd ((void **)sm->si.requires, "rpc.statd", SET_TYPE_STRING);
   }
  } else {
   if (!inset ((const void **)sm->si.requires, "portmap", SET_TYPE_STRING)) {
    sm->si.requires = (char **)setadd ((void **)sm->si.requires, "portmap", SET_TYPE_STRING);
   }
  }
 } else {
//  if (!inset ((const void **)sm->si.requires, "sm-notify", SET_TYPE_STRING)) {
//   sm->si.requires = (char **)setadd ((void **)sm->si.requires, "sm-notify", SET_TYPE_STRING);
//  }
  if (!inset ((const void **)sm->si.requires, "rpc.statd", SET_TYPE_STRING)) {
   sm->si.requires = (char **)setadd ((void **)sm->si.requires, "rpc.statd", SET_TYPE_STRING);
  }
 }

 return 0;
}

int linux_mount_update_nfs4 (struct lmodule *lm, struct smodule *sm, struct device_data *dd, struct mountpoint_data *mp) {
 if (!inset ((const void **)sm->si.requires, "network", SET_TYPE_STRING)) {
  sm->si.requires = (char **)setadd ((void **)sm->si.requires, "network", SET_TYPE_STRING);
 }

 if (mp->options) {
  if (inset ((const void **)mp->options, "sec=krb", SET_TYPE_STRING)) {
   if (!inset ((const void **)sm->si.requires, "rpc.svcgssd", SET_TYPE_STRING)) {
    sm->si.requires = (char **)setadd ((void **)sm->si.requires, "rpc.svcgssd", SET_TYPE_STRING);
   }
  }
 }
 if (!inset ((const void **)sm->si.requires, "rpc.idmapd", SET_TYPE_STRING)) {
  sm->si.requires = (char **)setadd ((void **)sm->si.requires, "rpc.idmapd", SET_TYPE_STRING);
 }

 return 0;
}

int linux_mount_cleanup (struct lmodule *this) {
 function_unregister ("find-block-devices-proc", 1, (void *)find_block_devices_proc);

 function_unregister ("fs-mount-linux-nfs", 1, (void *)linux_mount_do_mount_real);
 function_unregister ("fs-mount-Linux-nfs", 1, (void *)linux_mount_do_mount_real);
 function_unregister ("fs-mount-generic-nfs", 1, (void *)linux_mount_do_mount_real);

 function_unregister ("fs-mount-linux-any-backup", 1, (void *)linux_mount_do_mount_real);
 function_unregister ("fs-mount-Linux-any-backup", 1, (void *)linux_mount_do_mount_real);
 function_unregister ("fs-mount-generic-any-backup", 1, (void *)linux_mount_do_mount_real);

 function_unregister ("fs-mount-linux-swap", 1, (void *)linux_mount_do_mount_swap);
 function_unregister ("fs-mount-Linux-swap", 1, (void *)linux_mount_do_mount_swap);
 function_unregister ("fs-mount-generic-swap", 1, (void *)linux_mount_do_mount_swap);

 function_unregister ("fs-umount-linux-swap", 1, (void *)linux_mount_do_umount_swap);
 function_unregister ("fs-umount-Linux-swap", 1, (void *)linux_mount_do_umount_swap);
 function_unregister ("fs-umount-generic-swap", 1, (void *)linux_mount_do_umount_swap);

 function_unregister ("fs-mount-linux-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);
 function_unregister ("fs-mount-Linux-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);
 function_unregister ("fs-mount-generic-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);

 function_unregister ("fs-update-generic-nfs", 1, (void *)linux_mount_update_nfs);
 function_unregister ("fs-update-generic-nfs4", 1, (void *)linux_mount_update_nfs4);

 exec_cleanup(this);

 return 0;
}

int linux_mount_configure (struct lmodule *this) {
 module_init (this);

 thismodule->cleanup = linux_mount_cleanup;

/* pexec configuration */
 exec_configure (this);

 function_register ("find-block-devices-proc", 1, (void *)find_block_devices_proc);
/* register some functions for swap */
 function_register ("fs-mount-linux-swap", 1, (void *)linux_mount_do_mount_swap);
 function_register ("fs-mount-Linux-swap", 1, (void *)linux_mount_do_mount_swap);
 function_register ("fs-mount-generic-swap", 1, (void *)linux_mount_do_mount_swap);

/* this one also needs a special 'umount' call */
 function_register ("fs-umount-linux-swap", 1, (void *)linux_mount_do_umount_swap);
 function_register ("fs-umount-Linux-swap", 1, (void *)linux_mount_do_umount_swap);
 function_register ("fs-umount-generic-swap", 1, (void *)linux_mount_do_umount_swap);

/* nfs mounting is a real, royal PITA. we'll use the regular /bin/mount command for the time being */
 function_register ("fs-mount-linux-nfs", 1, (void *)linux_mount_do_mount_real);
 function_register ("fs-mount-Linux-nfs", 1, (void *)linux_mount_do_mount_real);
 function_register ("fs-mount-generic-nfs", 1, (void *)linux_mount_do_mount_real);

/* ntfs-3g has its own handler for things */
 function_register ("fs-mount-linux-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);
 function_register ("fs-mount-Linux-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);
 function_register ("fs-mount-generic-ntfs-3g", 1, (void *)linux_mount_do_mount_ntfs_3g);

/* register our handler as a generic backup-mounter */
/* this oughta come in handy */
 function_register ("fs-mount-linux-any-backup", 1, (void *)linux_mount_do_mount_real);
 function_register ("fs-mount-Linux-any-backup", 1, (void *)linux_mount_do_mount_real);
 function_register ("fs-mount-generic-any-backup", 1, (void *)linux_mount_do_mount_real);

 function_register ("fs-update-generic-nfs", 1, (void *)linux_mount_update_nfs);
 function_register ("fs-update-generic-nfs4", 1, (void *)linux_mount_update_nfs4);

 return 0;
}
