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

#include <sys/stat.h>
#include <fcntl.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_urandom_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule linux_urandom_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_generic,
 .name      = "Urandom",
 .rid       = "linux-urandom",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_urandom_configure
};

module_register(linux_urandom_self);

#endif

void linux_urandom_mini_dd(const char *from, const char *to, size_t s) {
 int from_fd = open(from, O_RDONLY);
 if (from_fd) {
  int to_fd = open(to, O_WRONLY);
  if (to_fd) {
   char buffer[s];
   size_t len = read (from_fd, buffer, s);
   if (len > 0) {
    write (to_fd, buffer, len);
   }
   close (to_fd);
  }

  close (from_fd);
 }
}

int linux_urandom_save_seed(void) {
 int ret = status_failed;
 char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
 if (seedPath) {
  char *poolsize_s = readfile("/proc/sys/kernel/random/poolsize");
  int poolsize = 512;
  if (poolsize_s) {
   parse_integer (poolsize_s);
   efree (poolsize_s);
  }

  linux_urandom_mini_dd ("/dev/urandom", seedPath, poolsize);
  return status_ok;
 } else {
  notice(3,"Don't know where to save seed!");
 }
 return status_ok;
}

int linux_urandom_do_seed(void) {
 int ret = status_failed;
 char *seedPath = cfg_getstring ("configuration-services-urandom/seed", NULL);
 if (seedPath) {
  char *poolsize_s = readfile("/proc/sys/kernel/random/poolsize");
  int poolsize = 512;
  if (poolsize_s) {
   parse_integer (poolsize_s);
   efree (poolsize_s);
  }

  linux_urandom_mini_dd (seedPath, "/dev/urandom", poolsize);
  return status_ok;
 } else {
  notice(3,"Don't know where to read the seed from!");
 }
 return status_ok;
}

void linux_urandom_root_ok_handler (struct einit_event *ev) {
 if (strmatch(ev->string, "mount-critical")) {
  notice(3,"Initialising Random Number Generator.");

  linux_urandom_do_seed();
 }
}

void linux_urandom_power_down_handler (struct einit_event *ev) {
 fprintf(stdout,"Saving random seed\n");
 linux_urandom_save_seed();
}

int linux_urandom_cleanup (struct lmodule *pa) {
 event_ignore (einit_core_service_enabled, linux_urandom_root_ok_handler);
 event_ignore (einit_power_down_scheduled, linux_urandom_power_down_handler);

 return 0;
}

int linux_urandom_configure (struct lmodule *pa) {
 module_init (pa);

 pa->cleanup = linux_urandom_cleanup;

 event_listen (einit_core_service_enabled, linux_urandom_root_ok_handler);
 event_listen (einit_power_down_scheduled, linux_urandom_power_down_handler);

 return 0;
}
