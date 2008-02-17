/*
 *  linux-urandom.c
 *  einit
 *
 *  Created on 02/17/2008.
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

int linux_urandom_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

char * linux_urandom_provides[] = {"urandom", NULL};
char * linux_urandom_requires[] = {NULL, NULL};
char * linux_urandom_after[]    = {NULL, NULL};
char * linux_urandom_before[]   = {NULL, NULL};


const struct smodule linux_urandom_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Urandom",
 .rid       = "linux-urandom",
 .si        = {
  .provides = linux_urandom_provides,
  .requires = linux_urandom_requires,
  .after    = linux_urandom_after,
  .before   = linux_urandom_before
 },
 .configure = linux_urandom_configure
};

module_register(linux_urandom_self);

#endif

int linux_urandom_enable  (void *, struct einit_event *);
int linux_urandom_disable (void *, struct einit_event *);

int linux_urandom_cleanup (struct lmodule *pa) {
/* cleanup code here */

 return 0;
}

int linux_urandom_enable (void *param, struct einit_event *status) {
 /* code here */

 /* do something that may fail */

/* if (things_worked_out) {
  return status_ok;
 } else {

  fbprintf (status, "Could not enable module: %s", strerror (errno));

  return status_failed;
 }*/

 return status_ok; /* be good, assume it worked */
}

int linux_urandom_disable (void *param, struct einit_event *status) {
 /* code here */

 /* do something that may fail */
 int things_worked_out = 1;
 if (things_worked_out) {
  return status_ok;
 } else {

  fbprintf (status, "Could not disable module: %s", strerror (errno));

  return status_failed;
 }

 return status_ok; /* be good, assume it worked */
}

int linux_urandom_configure (struct lmodule *pa) {
 module_init (pa);

 pa->enable = linux_urandom_enable;
 pa->disable = linux_urandom_disable; /* yes, ->disable is required! */
 pa->cleanup = linux_urandom_cleanup;

/* more configure code here */

 return 0;
}
