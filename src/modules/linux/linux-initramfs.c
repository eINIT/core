/**
 *  linux-initramfs.c
 *  einit
 *
 *  Created by Ryan Hope on 02/15/2008.
 *  Copyright 2008 Ryan Hope. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#include <run-init.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_initramfs_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_initramfs_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Initramfs Helper",
 .rid       = "linux-initramfs",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_initramfs_configure
};

module_register(linux_initramfs_self);

#endif

void linux_kernel_modules_boot_event_handler_early (struct einit_event *ev) {
 if (strmatch(einit_argv[0], "/init")) {
  fprintf (stderr, "entering initramfs mode...\n");
  // before we switch root we need to modprobe modules and mound /dev
  struct einit_event eml = evstaticinit(einit_boot_initramfs);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
 } else {
  fprintf (stderr, "running early bootup code...\n");
  struct einit_event eml = evstaticinit(einit_boot_early);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread_multi_wait);
  evstaticdestroy(eml);
 }
}

void linux_kernel_modules_boot_event_handler_initramfs_done (struct einit_event *ev) {  	    
 // mode this to event
 char realroot [BUFFERSIZE];
 int i = 1;
 // make sure not to crash when nothing is found
 realroot[0] = 0;
 // sizeof() is useless on dynamically sized vectors
 for (; einit_argv[i]; i++) {
  if ( strmatch(strtok(einit_argv[i],"="),"root") ) {
   esprintf(realroot, BUFFERSIZE, "%s", strtok(NULL,"="));
  }
 }
 if (realroot[0]) {
  if (strmatch(run_init(realroot), "ok")) {
   exit(einit_exit_status_exit_respawn);
  } else {
   notice(1,"bitch took my fish");
  }
 }
}

int linux_initramfs_cleanup (struct lmodule *pa) {
 event_ignore (einit_boot_initramfs_check, linux_kernel_modules_boot_event_handler_early);

 return 0;
}

int linux_initramfs_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_initramfs_cleanup;

 event_listen (einit_boot_initramfs_check, linux_kernel_modules_boot_event_handler_early);

 return 0;
}
