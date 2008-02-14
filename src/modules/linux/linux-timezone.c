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

void linux_timezone_core_event_handler (struct einit_event *ev) {
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

void linux_timezone_ipc_event_handler (struct einit_event *ev) {
 /* mess with ev->argv here */
}

void *linux_timezone_some_function (void *arg1, int arg2, char **arg3) {
 /* do whatever your function is supposed to do */

 /* do note that it's YOUR JOB to make sure the arguments in your function
    match up with the arguments that the callees think your function will have.
    neither eINIT nor GCC will complain if there's a mismatch! */
}

int linux_timezone_cleanup (struct lmodule *pa) {
 /* cleanup code here */

 function_unregister ("my-fancy-new-function", 1, linux_timezone_some_function);

 event_ignore (einit_event_subsystem_ipc, linux_timezone_ipc_event_handler);
 event_ignore (einit_event_subsystem_core, linux_timezone_core_event_handler);

 return 0;
}

int linux_timezone_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_timezone_cleanup;

 event_listen (einit_event_subsystem_core, linux_timezone_core_event_handler);
 event_listen (einit_event_subsystem_ipc, linux_timezone_ipc_event_handler);

 function_register ("my-fancy-new-function", 1, linux_timezone_some_function);

 /* more configure code here */

 return 0;
}