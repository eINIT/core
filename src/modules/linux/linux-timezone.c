/**
 *  linux-timezone.c
 *  einit
 *
 *  Created by Ryan Hope on 02/14/2008.
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

int linux_timezone_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_timezone_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Timezone",
 .rid       = "linux-timezone",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_timezone_configure
};

module_register(linux_timezone_self);

#endif

void linux_timezone_root_ok_handler (struct einit_event *ev) {
 switch (ev->type) {
  case einit_core_configuration_update:
   /* some code */
   break;

  default:
   /* default: is necessary to make sure the compiler won't warn us of
      unhandled events. */
   break;
 }
}

void *linux_timezone_make_symlink (void) {
 char *zoneinfo = cfg_getstring ("configuration-system-timezone", NULL);
 if (*zoneinfo) {
  char tmp [BUFFERSIZE];
  esprintf (tmp, BUFFERSIZE, "/usr/share/zoneinfo/%s", zoneinfo);
  symlink (tmp, "/etc/localtime");
 }
}

int linux_timezone_cleanup (struct lmodule *pa) {

 function_unregister ("make-timezone-symlink", 1, linux_timezone_make_symlink);

 event_ignore (einit_boot_root_device_ok, linux_timezone_root_ok_handler);

 return 0;
}

int linux_timezone_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_timezone_cleanup;

 event_listen (einit_boot_root_device_ok, linux_timezone_root_ok_handler);

 function_register ("make-timezone-symlink", 1, linux_timezone_make_symlink);

 return 0;
}

