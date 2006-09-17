/*
 *  module-logic.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
#include <einit/utility.h>
#include <pthread.h>
#include <einit/module-logic.h>

struct lmodule *mlist;

// create a plan for loading a set of atoms
struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
 uint32_t a = 0, b = 0;
 char
  **enable = NULL, **aenable = NULL,
  **disable = NULL, **adisable = NULL,
  **reset = NULL, **areset = NULL;
 struct cfgnode *rmode = mode;
 struct mloadplan_node nnode;
 struct uhash *ha;
 char da = 0, dabf = 0;

 if (!plan) {
  plan = ecalloc (1, sizeof (struct mloadplan));
  pthread_mutex_init (&plan->mutex, NULL);
 }
 pthread_mutex_lock (&plan->mutex);

 if (mode) {
  enable  = str2set (':', cfg_getstring ("enable/mod", mode)),
  disable = str2set (':', cfg_getstring ("disable/mod", mode)),
  reset   = str2set (':', cfg_getstring ("reset/mod", mode));

  if (mode->base) {
   int y = 0;
   struct cfgnode *cno;
   while (mode->base[y]) {
    cno = cfg_findnode (mode->base[y], EI_NODETYPE_MODE, NULL);
    if (cno) {
     char
      **denable  = str2set (':', cfg_getstring ("enable/mod", cno)),
      **ddisable = str2set (':', cfg_getstring ("disable/mod", cno)),
      **dreset   = str2set (':', cfg_getstring ("reset/mod", cno));

     if (denable) {
      char **t = (char **)setcombine ((void **)denable, (void **)enable, SET_TYPE_STRING);
      free (denable);
      free (enable);
      enable = t;
     }
     if (ddisable) {
      char **t = (char **)setcombine ((void **)ddisable, (void **)disable, SET_TYPE_STRING);
      free (ddisable);
      free (disable);
      disable = t;
     }
     if (dreset) {
      char **t = (char **)setcombine ((void **)dreset, (void **)reset, SET_TYPE_STRING);
      free (dreset);
      free (reset);
      reset = t;
     }
    }
    y++;
   }
  }
 } else {
  if (task & MOD_ENABLE) enable = atoms;
  else if (task & MOD_DISABLE) disable = atoms;
  else if (task & MOD_RESET) reset = atoms;
 }

/* find services that should be disabled */
 if (disable) {
  char **current = (char **)setdup ((void **)disable, SET_TYPE_STRING), **recurse = NULL;
  disa_rescan:
//  puts ("disable");

  if (current)
  for (a = 0; current[a]; a++) {
   struct lmodule *cur = mlist;
   if ((!da && (da = !strcmp (current[a], "all"))) || (!dabf && (dabf = !strcmp (current[a], "all-but-feedback")))) {
    free (current);
    current = service_usage_query_cr (SERVICE_GET_ALL_PROVIDED, NULL, NULL);

    goto disa_rescan;
   }

   memset (&nnode, 0, sizeof (struct mloadplan_node));
   pthread_mutex_init (&nnode.mutex, NULL);
   nnode.plan = plan;

   if (ha = hashfind (plan->services, current[a]))
    continue;

   while (cur) {
    struct smodule *mo = cur->module;
    if ((cur->status & STATUS_ENABLED) && mo &&
        (!dabf || !(mo->mode & EINIT_MOD_FEEDBACK)) &&
         inset ((void **)mo->provides, (void *)current[a], SET_TYPE_STRING)) {
     char **t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, cur, NULL);
     nnode.mod = (struct lmodule **)setadd ((void **)nnode.mod, (void *)cur, SET_NOALLOC);
     if (t) {
      recurse = (char **)setcombine ((void **)recurse, (void **)t, SET_TYPE_STRING);
      free (t);
     }
    }

    cur = cur->next;
   }

   if (!nnode.mod) {
    char tmp[2048]; tmp[0] = 0;
    strcat (tmp, current[a]);
    strcat (tmp, "/group");

    if (nnode.group = str2set (':', cfg_getstring (tmp, mode)))
     recurse = (char **)setcombine ((void **)recurse, (void **)nnode.group, SET_NOALLOC);
   }

   if (nnode.mod || nnode.group) {
    plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
    adisable = (char **)setadd ((void **)adisable, (void *)current[a], SET_TYPE_STRING);
   } else {
    plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
    pthread_mutex_destroy (&nnode.mutex);
   }
  }

  free (current); current = recurse; recurse = NULL;

  while (current) {
   for (a = 0; current[a]; a++) {
//    puts (current[a]);
    struct lmodule *cur = mlist;
    memset (&nnode, 0, sizeof (struct mloadplan_node));
    pthread_mutex_init (&nnode.mutex, NULL);
    nnode.plan = plan;

    if (ha = hashfind (plan->services, current[a]))
     continue;

    while (cur) {
     struct smodule *mo = cur->module;
     if ((cur->status & STATUS_ENABLED) && mo && inset ((void **)mo->provides, (void *)current[a], SET_TYPE_STRING)) {
      char **t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, cur, NULL);
      nnode.mod = (struct lmodule **)setadd ((void **)nnode.mod, (void *)cur, SET_NOALLOC);
//      recurse = (char **)setcombine ((void **)recurse, (void **)mo->requires, SET_NOALLOC);
      if (t) {
       recurse = (char **)setcombine ((void **)recurse, (void **)t, SET_TYPE_STRING);
       free (t);
      }
     }

     cur = cur->next;
    }

    if (!nnode.mod) {
     char tmp[2048]; tmp[0] = 0;
     strcat (tmp, current[a]);
     strcat (tmp, "/group");

     if (nnode.group = str2set (':', cfg_getstring (tmp, mode)))
      recurse = (char **)setcombine ((void **)recurse, (void **)nnode.group, SET_NOALLOC);
    }

    if (nnode.mod || nnode.group) {
     plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
     adisable = (char **)setadd ((void **)adisable, (void *)current[a], SET_TYPE_STRING);
//     puts (current[a]);
    } else {
     plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
     pthread_mutex_destroy (&nnode.mutex);
    }
   }

   free (current); current = recurse; recurse = NULL;
  }

 }

/* find services that should be enabled */
 if (enable) {
  char **current = (char **)setdup ((void **)enable, SET_TYPE_STRING);
  char **recurse = NULL;
  while (current) {
   for (a = 0; current[a]; a++) {
    struct lmodule *cur = mlist;
    memset (&nnode, 0, sizeof (struct mloadplan_node));
    pthread_mutex_init (&nnode.mutex, NULL);
    nnode.plan = plan;

    if (ha = hashfind (plan->services, current[a]))
     continue;

    while (cur) {
     struct smodule *mo = cur->module;
     if (!(cur->status & STATUS_ENABLED) && mo && inset ((void **)mo->provides, (void *)current[a], SET_TYPE_STRING)) {
      nnode.mod = (struct lmodule **)setadd ((void **)nnode.mod, (void *)cur, SET_NOALLOC);
      recurse = (char **)setcombine ((void **)recurse, (void **)mo->requires, SET_NOALLOC);
     }

     cur = cur->next;
    }

    if (!nnode.mod) {
     char tmp[2048]; tmp[0] = 0;
     strcat (tmp, current[a]);
     strcat (tmp, "/group");

     if (nnode.group = str2set (':', cfg_getstring (tmp, mode)))
      recurse = (char **)setcombine ((void **)recurse, (void **)nnode.group, SET_NOALLOC);
//     recurse = (char **)setcombine ((void **)recurse, (void **)str2set (':', cfg_getstring (tmp, mode)), SET_NOALLOC);
    }

    if (nnode.mod || nnode.group) {
     plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
     aenable = (char **)setadd ((void **)aenable, (void *)current[a], SET_TYPE_STRING);
//     puts (current[a]);
    } else {
     plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
     pthread_mutex_destroy (&nnode.mutex);
    }
   }

   free (current); current = recurse; recurse = NULL;
  }
 }
// modules to be reset
 if (reset) {
  char **current = (char **)setdup ((void **)reset, SET_TYPE_STRING);
//  puts ("reset:");
  for (a = 0; current[a]; a++) {
//   puts (current[a]);
  }
  if (current) free (current);
 }

 if (plan->services) {
  struct uhash *ha = plan->services;

  while (ha) {
   ((struct mloadplan_node *)(ha->value))->service = ha->key;

   ha = hashnext (ha);
  }
 }

 plan->enable = enable;

 if (da || dabf)
  plan->disable = adisable;
 else
  plan->disable = disable;
 plan->reset = reset;
 plan->mode = mode;

 pthread_mutex_unlock (&plan->mutex);
 return plan;
}

// the un-loader function
void *mod_plan_commit_recurse_disable (struct mloadplan_node *node) {
 pthread_mutex_lock (&node->mutex);
 pthread_t **subthreads = NULL;
 struct uhash *ha, *rha = NULL;
 uint32_t i = 0, u = 0, j = 0;

// notice (3, node->service);

 if (node->mod) {
//  fprintf (stdout, "disabling node 0x%zx\n", node);

  for (i = 0; node->mod[i]; i++) {
//   puts ("+");
   if (!service_usage_query (SERVICE_NOT_IN_USE, node->mod[i], NULL)) {
    pthread_t th;
    char **t;
    if (t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, node->mod[i], NULL)) {
     for (u = 0; t[u]; u++) {
      if (rha = hashfind (node->plan->services, t[u])) {
       if (!pthread_create (&th, NULL, (void *(*)(void *))mod_plan_commit_recurse_disable, (void *)rha->value))
        subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
       else {
        notice (2, "warning: subthread creation failed!");
        mod_plan_commit_recurse_disable ((struct mloadplan_node *)rha->value);
       }
      }
     }
     free (t);
     t = NULL;
    }
   }

   if (subthreads) {
    for (u = 0; subthreads[u]; u++)
     pthread_join (*(subthreads[u]), NULL);

    free (subthreads); subthreads = NULL;
   }

   node->status = mod (MOD_DISABLE, node->mod[i]);
//   puts ("-");
  }
 } else if (node->group) {
  fputs ("---- SUPPOSED TO DISABLE GROUP ---- ; disabling groups not implemented properly", stderr);
  for (u = 0; node->group[u]; u++) {
   if (ha = hashfind (node->plan->services, node->group[u])) {
    struct mloadplan_node *cnode = (struct mloadplan_node *)ha->value;

    mod_plan_commit_recurse_disable (cnode);

    if (cnode->status & STATUS_DISABLED) {
     node->status |= STATUS_DISABLED;
    }
   }
  }
 } // this could actually be redundant...

 pthread_mutex_unlock (&node->mutex);
// pthread_exit (NULL);
 return NULL;
}

// the loader function
void *mod_plan_commit_recurse_enable (struct mloadplan_node *node) {
 pthread_mutex_lock (&node->mutex);
 pthread_t **subthreads = NULL;
 struct uhash *ha;
 uint32_t i = 0, u = 0;

 if (node->mod) {
//  fprintf (stderr, "enabling node 0x%zx\n", node);

  for (i = 0; node->mod[i]; i++) {
   char **services = (node->mod[i]->module) ? node->mod[i]->module->requires : NULL;

   if (services) {
    uint32_t j = 0;
    for (; services[j]; j++)
     if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, services[j]) && (ha = hashfind (node->plan->services, services[j]))) {
      pthread_t th;
      if (!pthread_create (&th, NULL, (void *(*)(void *))mod_plan_commit_recurse_enable, (void *)ha->value))
       subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof(pthread_t));
      else {
       notice (2, "warning: subthread creation failed!");
       mod_plan_commit_recurse_enable (ha->value);
      }
     }
   }

   if (subthreads) {
    for (u = 0; subthreads[u]; u++)
     pthread_join (*(subthreads[u]), NULL);

    free (subthreads); subthreads = NULL;
   }

   if ((node->status = mod (MOD_ENABLE, node->mod[i])) & STATUS_ENABLED) break;
  }
 } else if (node->group) {
  for (u = 0; node->group[u]; u++) {
   if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, node->group[u]) && (ha = hashfind (node->plan->services, node->group[u]))) {
    struct mloadplan_node  *cnode = (struct mloadplan_node  *)ha->value;

    mod_plan_commit_recurse_enable (cnode);

    if (cnode->status & STATUS_ENABLED) {
     service_usage_query_group (SERVICE_ADD_GROUP_PROVIDER, cnode->mod[i], node->service);
     node->status |= STATUS_ENABLED;
    }
   }
  }
 }

 pthread_mutex_unlock (&node->mutex);
// pthread_exit (NULL);
 return NULL;
}

// the reset function
void *mod_plan_commit_recurse_reset (struct mloadplan_node *node) {
 pthread_mutex_lock (&node->mutex);
 uint32_t i = 0;

 if (node->mod) {
  for (i = 0; node->mod[i]; i++) {
   node->status = mod (MOD_RESET, node->mod[i]);
  }
 }

 pthread_mutex_unlock (&node->mutex);
 return NULL;
}

// the reload function
void *mod_plan_commit_recurse_reload (struct mloadplan_node *node) {
 pthread_mutex_lock (&node->mutex);
 uint32_t i = 0;

 if (node->mod) {
  for (i = 0; node->mod[i]; i++) {
   node->status = mod (MOD_RELOAD, node->mod[i]);
  }
 }

 pthread_mutex_unlock (&node->mutex);
 return NULL;
}

// actually do what the plan says
unsigned int mod_plan_commit (struct mloadplan *plan) {
 if (!plan) return;
 pthread_mutex_lock (&plan->mutex);

 if (plan->mode) cmode = plan->mode;

 pthread_t **subthreads = NULL;
 struct uhash *ha;
 uint32_t u = 0;

 if (plan->disable) {
  for (u = 0; plan->disable[u]; u++) {
//   puts (plan->disable[u]);
   if (ha = hashfind (plan->services, plan->disable[u])) {
    pthread_t th;

    if (!pthread_create (&th, NULL, (void *(*)(void *))mod_plan_commit_recurse_disable, (void *)ha->value))
     subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
    else {
     notice (2, "warning: subthread creation failed!");
     mod_plan_commit_recurse_disable (ha->value);
    }
   }
  }
 }

 if (plan->enable) {
  for (u = 0; plan->enable[u]; u++) {
   if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, plan->enable[u]) && (ha = hashfind (plan->services, plan->enable[u]))) {
    pthread_t th;
    if (!pthread_create (&th, NULL, (void *(*)(void *))mod_plan_commit_recurse_enable, (void *)ha->value))
     subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
    else {
     notice (2, "warning: subthread creation failed!");
     mod_plan_commit_recurse_enable (ha->value);
    }
   }
  }
 }

 if (plan->reset) {
  for (u = 0; plan->reset[u]; u++) {
//   puts (plan->reset[u]);
   if (ha = hashfind (plan->services, plan->reset[u])) {
    pthread_t th;

    printf ("resetting service %s", plan->reset[u]);
    if (!pthread_create (&th, NULL, (void *(*)(void *))mod_plan_commit_recurse_reset, (void *)ha->value))
     subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t));
    else {
     notice (2, "warning: subthread creation failed!");
     mod_plan_commit_recurse_reset (ha->value);
    }
   }
  }
 }

 if (subthreads) {
  for (u = 0; subthreads[u]; u++)
   pthread_join (*(subthreads[u]), NULL);

  free (subthreads); subthreads = NULL;
 }

 if (plan->unavailable) {
  char tmp[2048], tmp2[2048];
  snprintf (tmp, 2048, "WARNING: unavailable services - %s", plan->unavailable[0]);

  for (u = 1; plan->unavailable[u]; u++) {
   strcpy (tmp2, tmp);
   snprintf (tmp, 2048, "%s, %s", tmp2, plan->unavailable[u]);
  }

//  puts (tmp);
  notice (2, tmp);
 }

 if (plan->mode) amode = plan->mode;

 pthread_mutex_unlock (&plan->mutex);
 return 0;
}

// free all of the resources of the plan
int mod_plan_free (struct mloadplan *plan) {
 pthread_mutex_lock (&plan->mutex);
 if (plan->enable) free (plan->enable);
 if (plan->disable) free (plan->disable);
 if (plan->reset) free (plan->reset);
 if (plan->unavailable) free (plan->unavailable);

 if (plan->services) {
  struct uhash *ha = plan->services;
  struct mloadplan_node *no = NULL;

  while (ha) {
   if (no = (struct mloadplan_node *)ha->value)
    pthread_mutex_destroy (&no->mutex);

   ha = hashnext (ha);
  }
  hashfree (plan->services);
 }

 pthread_mutex_unlock (&plan->mutex);
 pthread_mutex_destroy (&plan->mutex);

 free (plan);
 return 0;
}
