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

#define mod_plan_searchgroup(nnode,service) \
 if (!nnode.mod && (gnode = cfg_getnode (service, mode)) && gnode->arbattrs) {\
  for (r = 0; gnode->arbattrs[r]; r+=2) {\
   if (!strcmp (gnode->arbattrs[r], "group")) {\
    if (nnode.group = str2set (':', gnode->arbattrs[r+1]))\
     recurse = (char **)setcombine ((void **)recurse, (void **)nnode.group, SET_NOALLOC);\
   } else if (!strcmp (gnode->arbattrs[r], "seq")) {\
    if (!strcmp (gnode->arbattrs[r+1], "any"))\
     nnode.options |=  MOD_PLAN_GROUP_SEQ_ANY;\
    else if (!strcmp (gnode->arbattrs[r+1], "all"))\
     nnode.options |=  MOD_PLAN_GROUP_SEQ_ALL;\
    else if (!strcmp (gnode->arbattrs[r+1], "any-iop"))\
     nnode.options |=  MOD_PLAN_GROUP_SEQ_ANY_IOP;\
    else if (!strcmp (gnode->arbattrs[r+1], "most"))\
     nnode.options |=  MOD_PLAN_GROUP_SEQ_MOST;\
   }\
  }\
 }


// create a plan for loading a set of atoms
struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
 uint32_t a = 0, b = 0, r = 0;
 char
  **enable = NULL, **aenable = NULL,
  **disable = NULL, **adisable = NULL,
  **reset = NULL, **areset = NULL;
 struct cfgnode *rmode = mode, *gnode;
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

   mod_plan_searchgroup(nnode, current[a]);

   if (nnode.mod || nnode.group) {
    plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
    adisable = (char **)setadd ((void **)adisable, (void *)current[a], SET_TYPE_STRING);
   } else {
    plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
   }
  }

  free (current); current = recurse; recurse = NULL;

  while (current) {
   for (a = 0; current[a]; a++) {
//    puts (current[a]);
    struct lmodule *cur = mlist;
    memset (&nnode, 0, sizeof (struct mloadplan_node));

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

    mod_plan_searchgroup(nnode, current[a]);

    if (nnode.mod || nnode.group) {
     plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
     adisable = (char **)setadd ((void **)adisable, (void *)current[a], SET_TYPE_STRING);
    } else {
     plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
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

    mod_plan_searchgroup(nnode, current[a]);

    if (nnode.mod || nnode.group) {
     plan->services = hashadd (plan->services, current[a], (void *)&nnode, sizeof(struct mloadplan_node), nnode.group);
     aenable = (char **)setadd ((void **)aenable, (void *)current[a], SET_TYPE_STRING);
    } else {
     plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)current[a], SET_TYPE_STRING);
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
   ((struct mloadplan_node *)(ha->value))->plan = plan;
   (((struct mloadplan_node *)(ha->value))->mutex) = ecalloc (1, sizeof(pthread_mutex_t));
   pthread_mutex_init ((((struct mloadplan_node *)(ha->value))->mutex), NULL);

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

#define run_or_spawn_subthreads(set,function,plan,subthreads,tstatus) \
{ \
 uint32_t u; struct uhash *rha; pthread_t th; \
 if (set && set[0] && set[1]) { \
  for (u = 0; set[u]; u++) { \
   if ((rha = hashfind (plan->services, set[u])) && rha->value && !(((struct mloadplan_node *)rha->value)->status & STATUS_FAIL) && !(((struct mloadplan_node *)rha->value)->status & tstatus)) { \
    if (!pthread_create (&th, NULL, (void *(*)(void *))function, (void *)rha->value)) \
     subthreads = (pthread_t **)setadd ((void **)subthreads, (void *)&th, sizeof (pthread_t)); \
    else { \
     notice (2, "warning: subthread creation failed!"); \
     function ((struct mloadplan_node *)rha->value); \
    } \
   } \
  } \
 } else { \
  if ((rha = hashfind (plan->services, set[0])) && rha->value) { \
   function ((struct mloadplan_node *)rha->value); \
  } \
 } \
}

// the un-loader function
void *mod_plan_commit_recurse_disable (struct mloadplan_node *node) {
 pthread_mutex_lock (node->mutex);
 if ((node->status & STATUS_DISABLED) || (node->status & STATUS_FAIL)) {
  pthread_mutex_unlock (node->mutex);
  return;
 }
 node->status |= STATUS_WORKING;

 pthread_t **subthreads = NULL;
 struct uhash *ha, *rha = NULL;
 uint32_t i = 0, u = 0, j = 0;

 if (node->mod) {
  for (i = 0; node->mod[i]; i++) {
   if (!service_usage_query (SERVICE_NOT_IN_USE, node->mod[i], NULL)) {
    pthread_t th;
    char **t;
    if (t = service_usage_query_cr (SERVICE_GET_SERVICES_THAT_USE, node->mod[i], NULL)) {
     run_or_spawn_subthreads (t,mod_plan_commit_recurse_disable,node->plan,subthreads,STATUS_DISABLED);
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
  }
 } else if (node->group) {
  run_or_spawn_subthreads (node->group,mod_plan_commit_recurse_disable,node->plan,subthreads,STATUS_DISABLED);

  if (subthreads) {
   for (u = 0; subthreads[u]; u++)
    pthread_join (*(subthreads[u]), NULL);

   free (subthreads); subthreads = NULL;
  }
  node->status |= STATUS_DISABLED;
 }

 pthread_mutex_unlock (node->mutex);
 return &(node->status);
}

// the loader function
void *mod_plan_commit_recurse_enable (struct mloadplan_node *node) {
 pthread_mutex_lock (node->mutex);
 if (node->group && (node->status == STATUS_ENABLING))
  goto resume_group_enable;
 else if ((node->status & STATUS_ENABLED) || (node->status & STATUS_FAIL)) {
  pthread_mutex_unlock (node->mutex);
  return;
 }
 node->status |= STATUS_WORKING;

 pthread_t **subthreads = NULL;
 struct uhash *ha;
 uint32_t i = 0, u = 0;

 if (node->mod) {
//  fprintf (stderr, "enabling node 0x%zx\n", node);

  for (i = 0; node->mod[i]; i++) {
   char **services = (node->mod[i]->module) ? node->mod[i]->module->requires : NULL;

   if (services)
    run_or_spawn_subthreads (services,mod_plan_commit_recurse_enable,node->plan,subthreads,STATUS_ENABLED);

   if (subthreads) {
    for (u = 0; subthreads[u]; u++)
     pthread_join (*(subthreads[u]), NULL);

    free (subthreads); subthreads = NULL;
   }

   if ((node->status = mod (MOD_ENABLE, node->mod[i])) & STATUS_ENABLED) break;
  }
 } else if (node->group) {
/* implement proper group logic here */
/* BUG: need to find the module that was actually enabled, shouldn't assume 0 */
  resume_group_enable:
  if ((node->options & MOD_PLAN_GROUP_SEQ_ANY_IOP) || (node->options & MOD_PLAN_GROUP_SEQ_ANY)) {
   for (u = 0; node->group[u]; u++) {
    if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, node->group[u]) && (ha = hashfind (node->plan->services, node->group[u]))) {
     struct mloadplan_node  *cnode = (struct mloadplan_node  *)ha->value;

     mod_plan_commit_recurse_enable (cnode);

     if (cnode->status & STATUS_ENABLED) {
      service_usage_query_group (SERVICE_ADD_GROUP_PROVIDER, cnode->mod[0], node->service);
      node->status = STATUS_ENABLED;
      goto exit;
     }
    }
   }
  } else if (node->options & MOD_PLAN_GROUP_SEQ_MOST) {
   for (u = 0; node->group[u]; u++) {
    if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, node->group[u]) && (ha = hashfind (node->plan->services, node->group[u]))) {
     struct mloadplan_node  *cnode = (struct mloadplan_node  *)ha->value;

     mod_plan_commit_recurse_enable (cnode);

     if (cnode->status & STATUS_ENABLED) {
      service_usage_query_group (SERVICE_ADD_GROUP_PROVIDER, cnode->mod[0], node->service);
      node->status |= STATUS_ENABLED;
     }
    }
   }
  } else if (node->options & MOD_PLAN_GROUP_SEQ_ALL) {
   for (u = 0; node->group[u]; u++) {
    if (!service_usage_query(SERVICE_IS_PROVIDED, NULL, node->group[u]) && (ha = hashfind (node->plan->services, node->group[u]))) {
     struct mloadplan_node  *cnode = (struct mloadplan_node  *)ha->value;

     mod_plan_commit_recurse_enable (cnode);

     if (cnode->status & STATUS_ENABLED) {
      service_usage_query_group (SERVICE_ADD_GROUP_PROVIDER, cnode->mod[0], node->service);
      node->status |= STATUS_ENABLED;
     }
    }
   }
  }
 }

 exit:

 if (node->status & STATUS_WORKING) node->status ^= STATUS_WORKING;

 pthread_mutex_unlock (node->mutex);
// pthread_exit (NULL);
 return &(node->status);
}

// the reset function
void *mod_plan_commit_recurse_reset (struct mloadplan_node *node) {
 pthread_mutex_lock (node->mutex);
 node->status |= STATUS_WORKING;
 uint32_t i = 0;

 if (node->mod) {
  for (i = 0; node->mod[i]; i++) {
   node->status = mod (MOD_RESET, node->mod[i]);
  }
 }

 pthread_mutex_unlock (node->mutex);
 return &(node->status);
}

// the reload function
void *mod_plan_commit_recurse_reload (struct mloadplan_node *node) {
 pthread_mutex_lock (node->mutex);
 uint32_t i = 0;

 if (node->mod) {
  for (i = 0; node->mod[i]; i++) {
   node->status = mod (MOD_RELOAD, node->mod[i]);
  }
 }

 pthread_mutex_unlock (node->mutex);
 return &(node->status);
}

// actually do what the plan says
unsigned int mod_plan_commit (struct mloadplan *plan) {
 if (!plan) return;
 pthread_mutex_lock (&plan->mutex);

 if (plan->mode) cmode = plan->mode;

 pthread_t **subthreads = NULL;
 struct uhash *ha;
 uint32_t u = 0;

 if (plan->disable)
  run_or_spawn_subthreads (plan->disable,mod_plan_commit_recurse_disable,plan,subthreads,STATUS_DISABLED);

 if (plan->enable)
  run_or_spawn_subthreads (plan->enable,mod_plan_commit_recurse_enable,plan,subthreads,STATUS_ENABLED);

 if (plan->reset)
  run_or_spawn_subthreads (plan->reset,mod_plan_commit_recurse_reset,plan,subthreads,0);

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
   if (no = (struct mloadplan_node *)ha->value) {
    pthread_mutex_destroy (no->mutex);
    free (no->mutex);
   }

   ha = hashnext (ha);
  }
  hashfree (plan->services);
 }

 pthread_mutex_unlock (&plan->mutex);
 pthread_mutex_destroy (&plan->mutex);

 free (plan);
 return 0;
}
