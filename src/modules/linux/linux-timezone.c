/*
 *  linux-timezone.c
 *  einit
 *
 *  Created by Ryan Hope on 2/13/2008.
 *  Copyright 2008 Magnus Deininger, Ryan Hope. All rights reserved.
 *
 */

/*
Copyright (c) 2007-2008, Magnus Deininger
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <einit/bitch.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/set.h>
#include <einit/tree.h>

int timezone_cleanup (struct lmodule *);
int timezone_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule linux_timezone_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Set Timezone",
 .rid       = "linux-timezone",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = timezone_configure
};

module_register(linux_timezone_self);

#endif

#define timezone_MODULES 70
#define timezone_SEED 200
#define timezone_DEPENDENCIES 3
#define timezone_SLEEP_MAX 3
#define timezone_GROUPS 10
#define timezone_GROUPELEMENTS 20

#define timezone_GROUP_TYPE "all"

char timezone_haverun = 0;

void timezone_add_update_group (char *groupname, char **elements, char *seq) {
 struct cfgnode newnode;
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "services-alias-%s", groupname);

 struct cfgnode *onode = cfg_getnode (tmp, NULL);
 if (onode) {
  char *jele = NULL;

  uint32_t i = 0;

  for (; onode->arbattrs[i]; i+=2) {
   if (strmatch (onode->arbattrs[i], "group")) {
    char **nele = str2set (':', onode->arbattrs[i+1]);

    nele = (char **)setcombine_nc ((void **)nele, (const void **)elements, SET_TYPE_STRING);

    jele = set2str (':', (const char **)nele);

    break;
   }
  }

  if (!jele) {
   jele = set2str (':', (const char **)elements);
  }

  char **oarb = onode->arbattrs;
  char **narb = NULL;

  narb = set_str_add (narb, (void *)"group");
  narb = set_str_add (narb, (void *)jele);
  narb = set_str_add (narb, (void *)"seq");
  narb = set_str_add (narb, (void *)seq);

  onode->arbattrs = narb;

  if (oarb) {
   efree (oarb);
  }
  efree (jele);

 } else {
  char *jele = set2str (':', (const char **)elements);
  memset (&newnode, 0, sizeof(struct cfgnode));

  newnode.id = str_stabilise (tmp);
  newnode.type = einit_node_regular;

  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"group");
  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)jele);
  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"seq");
  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)seq);

  cfg_addnode (&newnode);
  efree (jele);
 }
}

int timezone_int (int max) {
 char *zoneinfo = cfg_getstring ("configuration-system-timezone", NULL);
 char tmp [BUFFERSIZE];
 esprintf (tmp, BUFFERSIZE, "/usr/share/zoneinfo/%s", zoneinfo);
 return symlink (tmp, "/etc/localtime");
}

int timezone_module_enable (void *ignored, struct einit_event *status) {
 char sleeptime = timezone_int (10);

 while ((sleeptime = sleep (sleeptime)));

 return (timezone_int (10) <= 6) ? status_ok : status_failed;
}

int timezone_module_disable (void *ignored, struct einit_event *status) {
 return status_ok;
}

int timezone_module_cleanup (struct lmodule *me) {
 return status_ok;
}

int timezone_module_configure (struct lmodule *me) {
 me->enable = timezone_module_enable;
 me->disable = timezone_module_disable;
 me->cleanup = timezone_module_cleanup;

 return status_ok;
}

int timezone_scanmodules (struct lmodule *list) {
 if (timezone_haverun || !cfg_getnode("configuration", NULL)) return status_ok; /* make sure we only run ONCE */

 timezone_haverun = 1;

 char **timezonemodules = NULL;
 uint32_t r = 0;

 for (; r < timezone_MODULES; r++) if (r < (timezone_MODULES - timezone_GROUPS)) {
  char module_name[BUFFERSIZE];
  char tmp[BUFFERSIZE];
  char module_rid[BUFFERSIZE];

  esprintf (module_name, BUFFERSIZE, "timezone Module (%i)", r);
  esprintf (module_rid, BUFFERSIZE, "timezone-%i", r);

  struct smodule *nsm = emalloc (sizeof (struct smodule));

  memset (nsm, 0, sizeof (struct smodule));

  nsm->name = str_stabilise(module_name);
  nsm->rid = str_stabilise(module_rid);

  esprintf (tmp, BUFFERSIZE, "timezone%i", r);
  nsm->si.provides = set_str_add (nsm->si.provides, tmp);
  timezonemodules = set_str_add (timezonemodules, tmp);

  nsm->configure = timezone_module_configure;

/* cross-deps: */
  if (r > 0) { /* need at least a single entry point */
   uint32_t n = 0;
   uint32_t n_limit = timezone_int (timezone_DEPENDENCIES);

   for (; n < n_limit; n++) {
    esprintf (tmp, BUFFERSIZE, "timezone%i", timezone_int (timezone_MODULES -1));
    nsm->si.requires = set_str_add (nsm->si.requires, tmp);
   }
  }

  mod_add (NULL, nsm);
 } else {
  char groupname[BUFFERSIZE];
  char **elements = NULL;

  esprintf (groupname, BUFFERSIZE, "timezone%i", r);

  uint32_t n = 0;
  uint32_t n_limit = timezone_int (timezone_GROUPELEMENTS);

  for (; n < n_limit; n++) {
   char tmp[BUFFERSIZE];

   esprintf (tmp, BUFFERSIZE, "timezone%i", timezone_int (timezone_MODULES - timezone_GROUPS -1));
   elements = set_str_add (elements, tmp);
  }

  timezonemodules = set_str_add (timezonemodules, groupname);

  if (elements)
   timezone_add_update_group (groupname, elements, timezone_GROUP_TYPE);
 }

 timezone_add_update_group ("timezone", timezonemodules, "most");

 return status_ok;
}

int timezone_cleanup (struct lmodule *me) {
 return status_ok;
}

int timezone_configure (struct lmodule *me) {
 me->scanmodules = timezone_scanmodules;
 me->cleanup = timezone_cleanup;

 return status_ok;
}
