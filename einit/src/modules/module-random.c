/*
 *  module-random.c
 *  einit
 *
 *  Created by Magnus Deininger on 25/08/2007.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <einit/bitch.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <einit/set.h>
#include <einit/tree.h>

int random_cleanup (struct lmodule *);
int random_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_random_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Random Module Generator",
 .rid       = "einit-module-random",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = random_configure
};

module_register(einit_random_self);

#endif

#define RANDOM_MODULES 50
#define RANDOM_SEED 200
#define RANDOM_DEPENDENCIES 4
#define RANDOM_SLEEP_MAX 3
#define RANDOM_GROUPS 10
#define RANDOM_GROUPELEMENTS 20

char random_haverun = 0;

void random_add_update_group (char *groupname, char **elements, char *seq) {
 struct cfgnode newnode;
 char tmp[BUFFERSIZE];

 esprintf (tmp, BUFFERSIZE, "services-alias-%s", groupname);

 struct cfgnode *onode = cfg_getnode (tmp, NULL);
 if (onode) {
  char *jele = NULL;
  if (!onode->source || !strmatch (onode->source, self->rid)) {
   uint32_t i = 0;

   for (; onode->arbattrs[i]; i+=2) {
    if (strmatch (onode->arbattrs[i], "group")) {
     char **nele = str2set (':', onode->arbattrs[i+1]);

     nele = (char **)setcombine_nc ((void **)nele, (const void **)elements, SET_TYPE_STRING);

     jele = set2str (':', (const char **)nele);

     break;
    }
   }
  }

  if (!jele) {
   jele = set2str (':', (const char **)elements);
  }

  char **oarb = onode->arbattrs;
  char **narb = NULL;

  narb = (char **)setadd ((void **)narb, (void *)"group", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)jele, SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)"seq", SET_TYPE_STRING);
  narb = (char **)setadd ((void **)narb, (void *)seq, SET_TYPE_STRING);

  onode->arbattrs = narb;

  if (oarb) {
   free (oarb);
  }
  free (jele);

 } else {
  char *jele = set2str (':', (const char **)elements);
  memset (&newnode, 0, sizeof(struct cfgnode));

  newnode.id = estrdup (tmp);
  newnode.source = self->rid;
  newnode.type = einit_node_regular;

  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"group", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)jele, SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)"seq", SET_TYPE_STRING);
  newnode.arbattrs = (char **)setadd ((void **)newnode.arbattrs, (void *)seq, SET_TYPE_STRING);

  cfg_addnode (&newnode);
  free (jele);
 }
}

int random_int (int max) {
 long n = random();
 double d = ((double)n) / ((double)RAND_MAX);

 return (int)(d*max);
}

int random_module_enable (void *ignored, struct einit_event *status) {
 char sleeptime = random_int (10);

// while ((sleeptime = sleep (sleeptime)));

 return (random_int (10) <= 6) ? status_ok : status_failed;
}

int random_module_disable (void *ignored, struct einit_event *status) {
 char sleeptime = random_int (10);

// while ((sleeptime = sleep (sleeptime)));

 return (random_int (10) <= 6) ? status_ok : status_failed;
}

int random_module_cleanup (struct lmodule *me) {
 return status_ok;
}

int random_module_configure (struct lmodule *me) {
 me->enable = random_module_enable;
 me->disable = random_module_disable;
 me->cleanup = random_module_cleanup;

 return status_ok;
}

int random_scanmodules (struct lmodule *list) {
 if (random_haverun || !cfg_getnode("configuration", NULL)) return status_ok; /* make sure we only run ONCE */

 random_haverun = 1;

 char **randommodules = NULL;
 uint32_t r = 0;

 for (; r < RANDOM_MODULES; r++) if (r < (RANDOM_MODULES - RANDOM_GROUPS)) {
  char module_name[BUFFERSIZE];
  char tmp[BUFFERSIZE];
  char module_rid[BUFFERSIZE];

  esprintf (module_name, BUFFERSIZE, "Random Module (%i)", r);
  esprintf (module_rid, BUFFERSIZE, "random-%i", r);

  struct smodule *nsm = emalloc (sizeof (struct smodule));

  memset (nsm, 0, sizeof (struct smodule));

  nsm->name = estrdup(module_name);
  nsm->rid = estrdup(module_rid);

  esprintf (tmp, BUFFERSIZE, "random%i", r);
  nsm->si.provides = (char **)setadd ((void **)nsm->si.provides, tmp, SET_TYPE_STRING);
  randommodules = (char **)setadd ((void **)randommodules, tmp, SET_TYPE_STRING);

  nsm->configure = random_module_configure;

/* cross-deps: */
  if (r > 0) { /* need at least a single entry point */
   uint32_t n = 0;
   uint32_t n_limit = random_int (RANDOM_DEPENDENCIES);

   for (; n < n_limit; n++) {
    esprintf (tmp, BUFFERSIZE, "random%i", random_int (RANDOM_MODULES -1));
    nsm->si.requires = (char **)setadd ((void **)nsm->si.requires, tmp, SET_TYPE_STRING);
   }
  }

  mod_add (NULL, nsm);
 } else {
  char groupname[BUFFERSIZE];
  char **elements = NULL;

  esprintf (groupname, BUFFERSIZE, "random%i", r);

  uint32_t n = 0;
  uint32_t n_limit = random_int (RANDOM_GROUPELEMENTS);

  for (; n < n_limit; n++) {
   char tmp[BUFFERSIZE];

   esprintf (tmp, BUFFERSIZE, "random%i", random_int (RANDOM_MODULES - RANDOM_GROUPS -1));
   elements = (char **)setadd ((void **)elements, tmp, SET_TYPE_STRING);
  }

  randommodules = (char **)setadd ((void **)randommodules, groupname, SET_TYPE_STRING);

  if (elements)
   random_add_update_group (groupname, elements, "most");
 }


 random_add_update_group ("random", randommodules, "most");

 return status_ok;
}

int random_cleanup (struct lmodule *me) {
 return status_ok;
}

int random_configure (struct lmodule *me) {
 me->scanmodules = random_scanmodules;
 me->cleanup = random_cleanup;

 srandom(RANDOM_SEED);
// srandomdev();

 return status_ok;
}
