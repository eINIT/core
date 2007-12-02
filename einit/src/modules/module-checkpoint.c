/*
 *  module-checkpoint.c
 *  einit
 *
 *  Created by Magnus Deininger on 23/07/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

int checkpoint_cleanup (struct lmodule *);
int checkpoint_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_checkpoint_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Checkpoint Support",
 .rid       = "einit-module-checkpoint",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = checkpoint_configure
};

module_register(einit_checkpoint_self);

#endif

/* use letter names from the NATO phonetic alphabet to name checkpoints */

const char *checkpoint_names[] = {
 "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot", "Golf", "Hotel",
 "India", "Juliet", "Kilo", "Lima", "Mike", "November", "Oscar", "Papa",
 "Quebec", "Romeo", "Sierra", "Tango", "Uniform", "Victor", "Whiskey", "Xray",
 "Yankee", "Zulu"
};

#define CHECKPOINT_NAME_COUNT 26

int checkpoint_count = 0;

int checkpoint_module_enable (uintptr_t cooldown, struct einit_event *status) {
 if (cooldown) {
  fbprintf (status, "checkpoint reached, waiting for things to cool down (%i microseconds)", (int)cooldown);
  usleep (cooldown);
 }

 return status_ok;
}

int checkpoint_module_disable (uintptr_t cooldown, struct einit_event *status) {
 return status_ok;
}

int checkpoint_module_cleanup (struct lmodule *me) {
 return status_ok;
}

int checkpoint_module_configure (struct lmodule *me) {
 me->enable = (int (*)(void *, struct einit_event *))checkpoint_module_enable;
 me->disable = (int (*)(void *, struct einit_event *))checkpoint_module_disable;
 me->cleanup = checkpoint_module_cleanup;

 return status_ok;
}

int checkpoint_scanmodules_check_update (struct lmodule *list, char *rid) {
 struct lmodule *lm = list;

 while (lm) {
  if (lm->module && strmatch (lm->module->rid, rid)) {
   mod_update (lm);
   return 1;
  }

  lm = lm->next;
 }

 return 0;
}

char **checkpoint_scanmodules_find_services_from_mode (char **base_services, char *name) {
 if (name) {
  struct cfgnode *node = NULL;

  while ((node = cfg_findnode ("mode-enable", 0, node))) {
   if (node->arbattrs && node->mode && strmatch (name, node->mode->id)) {
    size_t i;

    for (i = 0; node->arbattrs[i]; i+=2) {
     if (strmatch (node->arbattrs[i], "services")) {
      char **serv = str2set (':', node->arbattrs[i+1]);
      size_t y;

      for (y = 0; serv[y]; y++) {
       if (!inset ((const void **)base_services, serv[y], SET_TYPE_STRING))
        base_services = (char **)setadd ((void **)base_services, serv[y], SET_TYPE_STRING);
      }
     }
    }
   }
  }
 }

 return base_services;
}

char **checkpoint_scanmodules_find_services_from_modes (char **base_services, char *base) {
 if (base) {
  char **base_split = str2set (':', base);

  if (base_split) {
   struct cfgnode *node = NULL;

   while ((node = cfg_findnode ("mode-enable", 0, node))) {
    if (node->arbattrs && node->mode && inset ((const void **)base_split, node->mode->id, SET_TYPE_STRING)) {
     size_t i;

     for (i = 0; node->arbattrs[i]; i+=2) {
      if (strmatch (node->arbattrs[i], "services")) {
       char **serv = str2set (':', node->arbattrs[i+1]);
       size_t y;

       for (y = 0; serv[y]; y++) {
        if (!inset ((const void **)base_services, serv[y], SET_TYPE_STRING))
         base_services = (char **)setadd ((void **)base_services, serv[y], SET_TYPE_STRING);
       }
      }
     }
    }
   }
  }
 }

 return base_services;
}

int checkpoint_scanmodules (struct lmodule *list) {
 struct cfgnode *node = NULL;

/* scan all modes... */
 while ((node = cfg_findnode ("mode-enable", 0, node))) {
  if (node->mode && node->mode->arbattrs) {
   size_t i = 0;
   char do_add = 0;
   char *base = NULL;
   uintptr_t cooldown = 0;

   for (; node->mode->arbattrs[i]; i+=2) {
    if (strmatch (node->mode->arbattrs[i], "wait-for-base") && parse_boolean (node->mode->arbattrs[i+1])) {
     do_add = 1;
    } else if (strmatch (node->mode->arbattrs[i], "cooldown")) {
     cooldown = parse_integer (node->mode->arbattrs[i+1]);
    } else if (strmatch (node->mode->arbattrs[i], "base")) {
     base = node->mode->arbattrs[i+1];
    }
   }

   if (do_add) {
    char buffer[BUFFERSIZE];

    esprintf (buffer, BUFFERSIZE, "checkpoint-mode-%s", node->mode->id);

    if (checkpoint_scanmodules_check_update (list, buffer)) {
     continue;
    } else {
     struct smodule *sm = emalloc (sizeof (struct smodule));
     char **base_services = checkpoint_scanmodules_find_services_from_modes (NULL, base);
     char **services = checkpoint_scanmodules_find_services_from_mode (NULL, node->mode->id);
     struct lmodule *nm;

     memset (sm, 0, sizeof (struct smodule));

     sm->rid = estrdup (buffer);
     if (checkpoint_count < CHECKPOINT_NAME_COUNT) {
      esprintf (buffer, BUFFERSIZE, "Checkpoint %s", checkpoint_names[checkpoint_count]);
      checkpoint_count++;
     } else {
      esprintf (buffer, BUFFERSIZE, "Checkpoint %i", checkpoint_count);
      checkpoint_count++;
     }
     sm->name = estrdup (buffer);

     if (base_services) {
      char *comb = set2str ('|', (const char **)base_services);
      size_t aflen = strlen (comb) + 5;
      char *af = emalloc (aflen);

      esprintf (af, aflen, "^(%s)$", comb);

      sm->si.after = str2set ('\0', af);

      efree (comb);
      efree (base_services);
      efree (af);
     }

     if (services) {
      char *comb = set2str ('|', (const char **)services);
      size_t belen = strlen (comb) + 5;
      char *be = emalloc (belen);

      esprintf (be, belen, "^(%s)$", comb);

      sm->si.before = str2set ('\0', be);

      efree (comb);
      efree (services);
      efree (be);
     }

     sm->eiversion = EINIT_VERSION;
     sm->eibuild = BUILDNUMBER;
     sm->configure = checkpoint_module_configure;
     sm->mode = einit_module_generic | einit_feedback_job;

     if ((nm = mod_add (NULL, sm))) {
      nm->param = (void *)cooldown;
     }
    }
   }
  }
 }

 return status_ok;
}

int checkpoint_cleanup (struct lmodule *me) {
 return status_ok;
}

int checkpoint_configure (struct lmodule *me) {
 me->scanmodules = checkpoint_scanmodules;
 me->cleanup = checkpoint_cleanup;

 return status_ok;
}
