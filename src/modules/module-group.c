/*
 *  module-group.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/12/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
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

#include <einit/module.h>
#include <einit/set.h>
#include <einit/event.h>
#include <einit/tree.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>

#include <string.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_group_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_group_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module,
 .name      = "Module Support (Service Groups)",
 .rid       = "einit-module-group",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_group_configure
};

module_register(module_group_self);

#endif

#define MODULES_PREFIX "services-alias-"
#define MODULES_PREFIX_SIZE (sizeof (MODULES_PREFIX) -1)

enum seq_type {
 sq_any,
 sq_most,
 sq_all
};

int module_group_module_enable (char *nodename, struct einit_event *status) {
 struct cfgnode *cn = cfg_getnode (nodename, NULL);

 if (cn && cn->arbattrs) {
  int i = 0;
  char **group = NULL;
  enum seq_type seq = sq_all;

  for (; cn->arbattrs[i]; i+=2) {
   if (strmatch (cn->arbattrs[i], "group")) {
    group = str2set (':', cn->arbattrs[i+1]);
   } else if (strmatch (cn->arbattrs[i], "seq")) {
    if (strmatch (cn->arbattrs[i+1], "any") || strmatch (cn->arbattrs[i+1], "any")) {
     seq = sq_any;
    } else if (strmatch (cn->arbattrs[i+1], "most")) {
     seq = sq_most;
    } else if (strmatch (cn->arbattrs[i+1], "all")) {
     seq = sq_all;
    }
   }
  }

  if (group) {
   if ((seq == sq_all) || !group[1])
/* we can bail at this point:
   if we only had one member, that one was set as requires=, if we had seq=all, then
   all of the members were set as requires=. */
    return status_ok;

   if (seq == sq_any) {
/* see if any of the items are enabled */
    for (i = 0; group[i]; i++) {
     if (mod_service_is_provided(group[i])) goto exit_good;
    }

    for (i = 0; group[i]; i++) {
     char **set = str2set (0, group[i]);
     int y;

     struct einit_event eml = evstaticinit(einit_core_manipulate_services);
     eml.stringset = set;
     eml.task = einit_module_enable;

     event_emit (&eml, einit_event_flag_broadcast);
     evstaticdestroy(eml);

     efree (set);

     for (y = 0; group[y]; y++) {
      if (mod_service_is_provided(group[y])) goto exit_good;
     }
    }
   }

   if (seq == sq_most) {
/* see if all of these are enabled... */
    int enabled = 0;
    for (i = 0; group[i]; i++) {
     if (mod_service_is_provided(group[i])) enabled++;
    }

    if (enabled == i)
/* if everything is enabled, then we do know for sure that this group is up. */
     goto exit_good;

/* (try to... ) enable all of the group members... */
    struct einit_event eml = evstaticinit(einit_core_manipulate_services);
    eml.stringset = group;
    eml.task = einit_module_enable;

    event_emit (&eml, einit_event_flag_broadcast);
    /* this will block until all of this has been tried at least once... */
    evstaticdestroy(eml);

    /* see if we have at least one enabled item */
    for (i = 0; group[i]; i++) {
     if (mod_service_is_provided(group[i])) goto exit_good;
    }
   }

   efree (group);
   return status_failed;

   exit_good:
   efree (group);
   return status_ok;
  }
 }

 return status_failed;
}

int module_group_module_disable (char *nodename, struct einit_event *status) {
 return status_ok;
}

int module_group_module_cleanup (struct lmodule *tm) {
 return 0;
}

int module_group_module_configure (struct lmodule *tm) {
 module_init(tm);

 tm->enable = (int (*)(void *, struct einit_event *))module_group_module_enable;
 tm->disable = (int (*)(void *, struct einit_event *))module_group_module_disable;
 tm->cleanup = module_group_module_cleanup;

 return 0;
}

void module_group_scanmodules (struct einit_event *ev) {
 struct stree *module_nodes = cfg_prefix(MODULES_PREFIX);

 if (module_nodes) {
  struct stree *cur = streelinear_prepare(module_nodes);

  for (; cur; cur = streenext (cur)) {
   struct cfgnode *cn = cur->value;

   if (cn && cn->arbattrs) {
    int i = 0;
    char **group = NULL, **before = NULL, **after = NULL;
    enum seq_type seq = sq_all;

    for (; cn->arbattrs[i]; i+=2) {
     if (strmatch (cn->arbattrs[i], "group")) {
      group = str2set (':', cn->arbattrs[i+1]);
     } else if (strmatch (cn->arbattrs[i], "seq")) {
      if (strmatch (cn->arbattrs[i+1], "any") || strmatch (cn->arbattrs[i+1], "any")) {
       seq = sq_any;
      } else if (strmatch (cn->arbattrs[i+1], "most")) {
       seq = sq_most;
      } else if (strmatch (cn->arbattrs[i+1], "all")) {
       seq = sq_all;
      }
     } else if (strmatch (cn->arbattrs[i], "before")) {
      before = str2set (':', cn->arbattrs[i+1]);
     } else if (strmatch (cn->arbattrs[i], "after")) {
      after = str2set (':', cn->arbattrs[i+1]);
     }
    }

    if (group) {
     char **requires = NULL, **provides = NULL;
     char t[BUFFERSIZE];

     if ((seq == sq_all) || !group[1]) {
      if (!strmatch (group[0], "none"))
       requires = set_str_dup_stable (group);
     } else {
      char *member_string = set2str ('|', (const char **)group);

      esprintf (t, BUFFERSIZE, "^(%s)$", member_string);

      after = set_str_add (after, t);

      efree (member_string);
     }

     provides = set_str_add (provides, (cur->key + MODULES_PREFIX_SIZE));

     struct smodule *sm = emalloc (sizeof (struct smodule));
     memset (sm, 0, sizeof (struct smodule));

     esprintf (t, BUFFERSIZE, "group-%s", cur->key + MODULES_PREFIX_SIZE);
     sm->rid = (char *)str_stabilise (t);
     sm->configure = module_group_module_configure;

     struct lmodule *lm = NULL;

     esprintf (t, BUFFERSIZE, "Group (%s)", cur->key + MODULES_PREFIX_SIZE);
     sm->name = (char *)str_stabilise (t);
     sm->si.requires = requires;
     sm->si.provides = provides;
     sm->si.before = before;
     sm->si.after = after;

     lm = mod_add_or_update (NULL, sm, substitue_and_prune);
     lm->param = (char *)str_stabilise (cur->key);
    }
   }
  }

  streefree (module_nodes);
 }

 return;
}

int module_group_cleanup (struct lmodule *tm) {
 event_listen (einit_core_update_modules, module_group_scanmodules);

 return 0;
}

int module_group_configure (struct lmodule *tm) {
 module_init(tm);

 event_listen (einit_core_update_modules, module_group_scanmodules);

 tm->cleanup = module_group_cleanup;

 module_group_scanmodules(NULL);

 return 0;
}
