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

int linux_initramfs_is_initramfs (void) {
 int ret;
 printf("%s/n",einit_argv[0]);
 if (strmatch(einit_argv[0], "linuxrc") || strmatch(einit_argv[0], "/linuxrc")) {
  ret = 0; 
 } else {
  ret = -1;
 }
 return ret;
}

void linux_initramfs_kernel_extensions_handler (struct einit_event *ev) {
 if (linux_initramfs_is_initramfs()==0) {
  notice(1, "INITRAMFS=True/n");
 } else {
  notice(1, "INITRAMFS=False/n");
 }
 sleep(5);
}

int linux_initramfs_cleanup (struct lmodule *pa) {
 event_ignore (einit_boot_load_kernel_extensions, linux_initramfs_kernel_extensions_handler);

 return 0;
}

int linux_initramfs_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_initramfs_cleanup;

 event_listen (einit_boot_load_kernel_extensions, linux_initramfs_kernel_extensions_handler);

 return 0;
}
