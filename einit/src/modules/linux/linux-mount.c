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

#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

/* struct definitions */
struct bd_info {
 char *label, *uuid, *fs;
};

struct fstab_entry {
 char *mountpoint, *device, *fs;
 char **options;
 uint32_t mountflags;
};

/* variable definitions */
pthread_mutex_t blockdevices_mutex = PTHREAD_MUTEX_INITIALIZER;
char *defaultblockdevicesource[] = {"dev", NULL};
char *defaultfstabsource[] = {"label", "configuration", "fstab", NULL};
struct uhash *blockdevices = NULL;
struct uhash *fstab = NULL;

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
void find_block_devices_recurse_path (char *);
void update_block_devices (void);
void update_fstab (void);
int scanmodules (struct lmodule *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);
int enable (void *, struct mfeedback *);
int disable (void *, struct mfeedback *);

/* function definitions */
void find_block_devices_recurse_path (char *path) {
 DIR *dir;
 struct dirent *entry;

 dir = opendir (path);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   struct stat statbuf;
   char *tmp = ecalloc (1, strlen(path) + entry->d_reclen + 1);
   tmp = strcat (tmp, path);
   tmp = strcat (tmp, entry->d_name);
   if (lstat (tmp, &statbuf)) {
    perror ("einit-linux-mount");
    free (tmp);
    continue;
   }
   if (S_ISDIR (statbuf.st_mode)) {
    tmp = strcat (tmp, "/");
    find_block_devices_recurse_path (tmp);
   }

   if (S_ISBLK (statbuf.st_mode) && !S_ISLNK(statbuf.st_mode)) {
    pthread_mutex_lock (&blockdevices_mutex);
    blockdevices = hashadd (blockdevices, tmp, NULL);
    pthread_mutex_unlock (&blockdevices_mutex);
   } else
    free (tmp);
  }
  closedir (dir);
 } else {
  fprintf (stderr, "einit-linux-mount: could not open %s\n", path);
  errno = 0;
  return;
 }
}

void update_block_devices (void) {
 struct cfgnode *node = cfg_findnode ("mount-block-devices-source", 0, NULL);
 char **bds = defaultblockdevicesource;
 uint32_t i = 0;

 if (node && node->svalue)
  bds = str2set (':', node->svalue);

 for (i = 0; bds[i]; i++) {
  if (!strcmp(bds[i], "dev"))
   find_block_devices_recurse_path ("/dev/");
  else fprintf (stderr, "einit-linux-mount: unhandled block-device-source \"%s\"\n", bds[i]);
 }

 if (bds != defaultblockdevicesource) free (bds);
}

void update_fstab (void) {
 struct cfgnode *node = cfg_findnode ("mount-fstab-source", 0, NULL);
 char **dfst = defaultfstabsource;
 uint32_t i = 0;

 if (node && node->svalue)
  dfst = str2set (':', node->svalue);

 for (i = 0; dfst[i]; i++) {
  if (!strcmp(dfst[i], "configuration")) {
   struct cfgnode *fstabnode = NULL;
   while (fstabnode = cfg_findnode ("fstab-entry", 0, fstabnode)) {
   }
  } else fprintf (stderr, "einit-linux-mount: unhandled fstab-source \"%s\"\n", dfst[i]);
 }

 if (dfst != defaultfstabsource) free (dfst);
}

int scanmodules (struct lmodule *modchain) {
/* struct smodule *modinfo = ecalloc (1, sizeof (struct smodule));

 modinfo->mode = EINIT_MOD_EXEC;
 modinfo->rid = "linux-mount-localmount";
 modinfo->name = "linux-mount [localmount]";
 modinfo->provides = {NULL}; */

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
 update_block_devices ();
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
