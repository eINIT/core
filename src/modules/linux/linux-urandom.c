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
 return 0;
}

int linux_urandom_enable (void *param, struct einit_event *status) {
 int ret = status_failed;
 int i = 0;
 char *seedPath;
 FILE *seed;
 FILE *urandom;
 
 fprintf(stdout,"Looking for seed path!!!");
 sleep(5);
 struct cfgnode *node = cfg_getnode ("configuration-services-urandom", NULL);
 if (node && node->arbattrs) {
  char *seedPath = NULL;
  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "seed")) {
    seedPath = node->arbattrs[i+1];
    fprintf(stdout,"Found seed path!!!");
    sleep(5);
    break;
   }
  }
 }
 fprintf(stdout,"%s",seedPath);
 if (seedPath) {
  seed = fopen(seedPath, "rw");
  fprintf(stdout,"Seed read???");
  if (seed) {
   fprintf(stdout,"Seed exists!!!");
   sleep(5);
   char old[512];
   int i = 0;
   for (i=0; i<512; i++) {
	sprintf (old, "%s%c", old, fgetc(seed));
   }
   fprintf(urandom, old);
  }
  char new[512];
  int j = 0;
  for (j=0; j<512; j++) {
   sprintf (new, "%s%c", new, fgetc(urandom));
  }
  fprintf(seed, new);
  fclose(seed);
  fclose(urandom);
  ret = status_ok;
 }
 return ret;
}

int linux_urandom_disable (void *param, struct einit_event *status) {
 int ret;
 int i = 0;
 char *seedPath;
 struct cfgnode *node = cfg_getnode ("configuration-services-urandom", NULL);
 if (node && node->arbattrs) {
  char *seedPath = NULL;
  for (; node->arbattrs[i]; i+=2) {
   if (strmatch (node->arbattrs[i], "seed")) {
    seedPath = node->arbattrs[i+1];
   }
  }
 }
 FILE *urandom = fopen("/dev/urandom","rw");
 FILE *seed = fopen(seedPath, "rw");
 char new[512];
 int j = 0;
 for (j=0; j<512; j++) {
  sprintf (new, "%s%c", new, fgetc(urandom));
 }
 ret = fprintf(seed, new);
 fclose(seed);
 fclose(urandom);
 return ret;
}

int linux_urandom_configure (struct lmodule *pa) {
 module_init (pa);

 pa->enable = linux_urandom_enable;
 pa->disable = linux_urandom_disable;
 pa->cleanup = linux_urandom_cleanup;

 return 0;
}
