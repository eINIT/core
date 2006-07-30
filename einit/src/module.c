/*
 *  module.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

struct lmodule *mlist = NULL;
int mcount = 0;

char **provided = NULL;
char **required = NULL;

int mod_scanmodules () {
 DIR *dir;
 struct dirent *entry;
 char *tmp;
 int mplen;
 void *sohandle;
 struct lmodule *cmod = NULL, *nmod;
 char *modulepath = cfg_getpath ("module-path");
 if (!modulepath) return -1;

 mplen = strlen (modulepath) +1;
 dir = opendir (modulepath);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   tmp = (char *)emalloc ((mplen + strlen (entry->d_name))*sizeof (char));
	struct stat sbuf;
	struct smodule *modinfo;
    int (*func)(void *);
    *tmp = 0;
    strcat (tmp, modulepath);
    strcat (tmp, entry->d_name);
	dlerror ();
    if (stat (tmp, &sbuf) || !S_ISREG (sbuf.st_mode)) {
	 free (tmp);
	 continue;
	}

	sohandle = dlopen (tmp, RTLD_NOW);
	if (sohandle == NULL) {
	 puts (dlerror ());
	 free (tmp);
	 continue;
	}
	modinfo = (struct smodule *)dlsym (sohandle, "self");
	if (modinfo != NULL) {
     mod_add (sohandle, NULL, NULL, NULL, modinfo);
	 if (modinfo->mode & EINIT_MOD_LOADER) {
      func = (int (*)(void *)) dlsym (sohandle, "scanmodules");
	 }
    }
	
	free (tmp);
  }
  closedir (dir);
 } else {
  fputs ("couldn't open module directory\n", stderr);
  return bitch(BTCH_ERRNO);
 }
 return 1;
}

void mod_freedesc (struct lmodule *m) {
 if (m->next != NULL)
  mod_freedesc (m->next);
 if (m->cleanup)
  m->cleanup (m);
 if (m->sohandle)
  dlclose (m->sohandle);
 pthread_mutex_destroy (&m->mutex);
 free (m);
}

int mod_freemodules () {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 mcount = 0;
 return 1;
}

int mod_add (void *sohandle, int (*enable)(void *, struct einit_event *), int (*disable)(void *, struct einit_event *), void *param, struct smodule *module) {
 struct lmodule *nmod, *cur;
 int (*scanfunc)(struct lmodule *);
 int (*ftload)  (void *, struct einit_event *);
 int (*configfunc)(struct lmodule *);

 nmod = ecalloc (1, sizeof (struct lmodule));

 if (mlist == NULL) {
  mlist = nmod;
 } else {
  cur = mlist;
  while (cur->next)
   cur = cur->next;
  cur->next = nmod;
 }
 mcount++;

 nmod->sohandle = sohandle;
 nmod->module = module;
 nmod->param = param;
 nmod->enable = enable;
 nmod->disable = disable;
 pthread_mutex_init (&nmod->mutex, NULL);

// this will do additional initialisation functions for certain module-types
 if (module && sohandle) {
// look for and execute any configure() functions in modules
  configfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "configure");
  if (configfunc != NULL) {
   configfunc (nmod);
  }
// look for any cleanup() functions
  configfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "cleanup");
  if (configfunc != NULL) {
   nmod->cleanup = configfunc;
  }
// EINIT_MOD_LOADER modules will usually want to provide a function to scan
//  for modules so they can be included in the dependency chain
  if (module->mode & EINIT_MOD_LOADER) {
   scanfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "scanmodules");
   if (scanfunc != NULL) {
    scanfunc (mlist);
   }
   else bitch(BTCH_ERRNO + BTCH_DL);
  }
// we need to scan for load and unload functions if NULL was supplied for these
  if (enable == NULL) {
   ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "enable");
   if (ftload != NULL) {
    nmod->enable = ftload;
   }
  }
  if (disable == NULL) {
   ftload = (int (*)(void *, struct einit_event *)) dlsym (sohandle, "disable");
   if (ftload != NULL) {
    nmod->disable = ftload;
   }
  }
 }

 return 0;
}

struct lmodule *mod_find (char *rid, unsigned int modeflags) {
 struct lmodule *cur = mlist;
 if (mlist == NULL)
  return NULL;

 if (rid) {
  while (!cur->module || !cur->module->rid ||
    (modeflags && (cur->module->mode ^ modeflags)) ||
    strcmp(rid, cur->module->rid)) {
   if (!cur->next) return NULL;
   cur = cur->next;
  }
 } else {
  while (!cur->module ||
    (modeflags && (cur->module->mode ^ modeflags))) {
   if (!cur->next) return NULL;
   cur = cur->next;
  }
 }

 return cur;
}

int mod (unsigned int task, struct lmodule *module) {
 struct einit_event *fb;
 char providefeedback;
 struct smodule *t;
 int ti, errc;
 unsigned int ret;
 if (!module) return 0;
/* wait if the module is already being processed in a different thread */
 if (errc = pthread_mutex_lock (&module->mutex)) {
// this is bad...
  puts (strerror (errc));
 }

 module->status = module->status | STATUS_WORKING;

/* check if the task requested has already been done (or if it can be done at all) */
 if ((task & MOD_ENABLE) && (!module->enable || (module->status & STATUS_ENABLED))) {
  wontload:
  pthread_mutex_unlock (&module->mutex);
  return STATUS_IDLE;
 }
 if ((task & MOD_DISABLE) && (!module->disable || (module->status & STATUS_DISABLED)))
  goto wontload;

 if (task & MOD_ENABLE) {
   if (t = module->module) {
    if (t->requires) for (ti = 0; t->requires[ti]; ti++)
     if (!strinset (provided, t->requires[ti])) {
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;
     }
   }
 } else if (task & MOD_DISABLE) {
   if (t = module->module) {
    if (t->provides) for (ti = 0; t->provides[ti]; ti++)
     if (strinset (required, t->provides[ti])) {
      pthread_mutex_unlock (&module->mutex);
      return STATUS_FAIL_REQ;
     }
   }
 }

 fb = (struct einit_event *)ecalloc (1, sizeof (struct einit_event));
 fb->para = (void *)module;
 fb->type = EINIT_EVENT_TYPE_FEEDBACK;
 fb->task = task | MOD_FEEDBACK_SHOW;
 fb->status = STATUS_WORKING;
 fb->flag = 0;
 fb->string = NULL;
 status_update (fb);

 if (task & MOD_ENABLE) {
   ret = module->enable (module->param, fb);
   if (ret & STATUS_OK) {
    if (t = module->module) {
     if (t->provides)
      provided = (char **)setcombine ((void **)provided, (void **)t->provides, -1);
     if (t->requires)
      required = (char **)setcombine ((void **)required, (void **)t->requires, -1);
    }
    module->status = STATUS_ENABLED;
    fb->status = STATUS_OK | STATUS_ENABLED;
   } else {
    fb->status = STATUS_FAIL;
   }
 } else if (task & MOD_DISABLE) {
   ret = module->disable (module->param, fb);
   if (ret & STATUS_OK) {
    if (t = module->module) {
     if (t->provides) for (ti = 0; provided && t->provides[ti]; ti++)
      provided = strsetdel (provided, t->provides[ti]);
     if (t->requires) for (ti = 0; required && t->requires[ti]; ti++)
      required = strsetdel (required, t->requires[ti]);
    }
    module->status = STATUS_DISABLED;
    fb->status = STATUS_OK | STATUS_DISABLED;
   } else {
    fb->status = STATUS_FAIL;
   }
 }

// printf ("%i:%i\n", required, provided);

 status_update (fb);
 free (fb);

 pthread_mutex_unlock (&module->mutex);
 return module->status;
}

/* helper functions for mod_plan should go right here */

int mod_plan_sort_by_preference (struct lmodule **cand, char *atom) {
 char *pstring = emalloc ((8 + strlen (atom)) * sizeof (char));
 char **preftab;
 struct cfgnode *node;
 unsigned int tn = 0, cn = 0, ci;
 pstring[0] = 0;
 strcat (pstring, "prefer-");
 strcat (pstring, atom);
 node = cfg_findnode (pstring, 0, NULL);
 if (!node || !node->svalue) return 0;
 free (pstring);
 preftab = str2set (':', node->svalue);

 for (; preftab[tn]; tn++) {
  for (ci = 0; cand[ci]; ci++) {
   if (cand[ci]->module && cand[ci]->module->rid &&
       !strcmp (cand[ci]->module->rid, preftab[tn])) {
    struct lmodule *tmp = cand[cn];
    cand[cn] = cand[ci];
    cand[ci] = tmp;
    cn++;
    break;
   }
  }
 }

 free (preftab);
 return 0;
}

struct mloadplan **mod_planpath (struct mloadplan *plan, struct mloadplan *target, struct mloadplan **path) {
 return path;
}

struct uhash *mod_plan2hash (struct mloadplan *plan, struct uhash *hash, int flag) {
 struct uhash *ihash = hash;
 int i;

 if (!plan) return NULL;
 if ((flag != MOD_P2H_PROVIDES_NOBACKUP) && plan->left && plan->left[0]) {
  for (i = 0; plan->left[i]; i++)
   ihash = mod_plan2hash(plan->left[i], ihash, flag);
 }
 if (plan->right && plan->right[0]) {
  for (i = 0; plan->right[i]; i++)
   ihash = mod_plan2hash(plan->right[i], ihash, flag);
 }
 if (plan->orphaned && plan->orphaned[0]) {
  for (i = 0; plan->orphaned[i]; i++)
   ihash = mod_plan2hash(plan->orphaned[i], ihash, flag);
 }
 if (flag == MOD_P2H_LIST) {
//  ihash = hashadd (ihash, "", plan, sizeof (struct mloadplan));
  ihash = hashadd (ihash, "", plan, -1, NULL);
 } else {
   switch (flag) {
    case MOD_P2H_PROVIDES:
    case MOD_P2H_PROVIDES_NOBACKUP:
     if (plan->mod && plan->mod->module && plan->mod->module->rid)
//      ihash = hashadd (ihash, plan->mod->module->rid, plan, sizeof (struct mloadplan));
      ihash = hashadd (ihash, plan->mod->module->rid, plan, -1, NULL);
     if (plan->provides && plan->provides[0]) {
      for (i = 0; plan->provides[i]; i++) {
//       ihash = hashadd (ihash, plan->provides[i], (void *)plan, sizeof (struct mloadplan));
       ihash = hashadd (ihash, plan->provides[i], (void *)plan, -1, NULL);
      }
     }
     break;
    case MOD_P2H_REQUIRES:
     if (plan->requires && plan->requires[0]) {
      for (i = 0; plan->requires[i]; i++) {
//       ihash = hashadd (ihash, plan->requires[i], (void *)plan, sizeof (struct mloadplan));
       ihash = hashadd (ihash, plan->requires[i], (void *)plan, -1, NULL);
      }
     }
     break;
   }
 }

 return ihash;
}

struct mloadplan *mod_plan_restructure (struct mloadplan *plan) {
 struct uhash *hash_prov, *hash_req, *hash_prov_nb, *c, *d;
 struct mloadplan **orphans = NULL;
 struct mloadplan **curpl = NULL;
 unsigned int i, j;
 unsigned char pass = 0, adds;
 if (!plan) return NULL;
 else if (!plan->mod && !plan->right && !plan->left && !plan->orphaned) {
  free (plan);
  return NULL;
 }

 hash_req = mod_plan2hash (plan, NULL, MOD_P2H_REQUIRES);
 hash_prov = mod_plan2hash (plan, NULL, MOD_P2H_PROVIDES);
 hash_prov_nb = mod_plan2hash (plan, NULL, MOD_P2H_PROVIDES_NOBACKUP);

 d = hash_prov;

 while (d) {
  if (d->value) {
   struct mloadplan *v = (struct mloadplan *)d->value;
   if (v->right) free (v->right);
   v->right = NULL;
//   if (v->left) free (v->left);
//   v->left = NULL;
  }
  d = hashnext (d);
 }

 d = hash_prov_nb;
 while (d) {
  if (d->value) {
   struct mloadplan *v = (struct mloadplan *)d->value;

//   printf ("%s: %i\n", v->mod->module->rid, v->task);

//   if (v->mod && v->mod->module) {
    if (((v->task & MOD_ENABLE) && (v->requires)) ||
        ((v->task & MOD_DISABLE) && ((v->provides)))) {
     char **req = NULL;
     if (v->task & MOD_ENABLE) req = v->requires;
     if (v->task & MOD_DISABLE) req = v->provides;

     for (j = 0; req[j]; j++) {
      adds = 0;
      if (v->task & MOD_ENABLE) c = hash_prov;
      if (v->task & MOD_DISABLE) c = hash_req;
      while (c && (c = hashfind (c, req[j]))) {
       struct mloadplan *e = c->value;
       adds++;
       e->right = (struct mloadplan **)setadd ((void **)e->right, (void *)v, -1);
       c = c->next;
      }
      if (adds) {
       plan->orphaned = (struct mloadplan **)setdel ((void **)plan->orphaned, (void*)v);
      } else if (v->task & MOD_ENABLE) {
       if (!strinset (plan->unavailable, req[j]) && !strinset (plan->unsatisfied, req[j]))
        plan->unsatisfied = (char **)setadd ((void **)plan->unsatisfied, (void *)req[j], -1);
      } else if (v->task & MOD_DISABLE) {
       plan->right = (struct mloadplan **)setadd ((void **)plan->right, (void *)v, -1);
       plan->orphaned = (struct mloadplan **)setdel ((void **)plan->orphaned, (void*)v);
      }
     }
    } else {
     plan->right = (struct mloadplan **)setadd ((void **)plan->right, (void *)v, -1);
     plan->orphaned = (struct mloadplan **)setdel ((void **)plan->orphaned, (void*)v);
    }
//   }
  }
  d = hashnext (d);
 }

 hashfree (hash_prov_nb);
 hashfree (hash_prov);
 hashfree (hash_req);

 return plan;
}

/* end helper functions */

struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task) {
 struct lmodule *curmod;
 struct mloadplan **nplancand = NULL;
 int si = 0;
 if (!atoms && !(task & MOD_DISABLE_UNSPEC)) return NULL;

 if (!plan) {
  plan = (struct mloadplan *)ecalloc (1, sizeof (struct mloadplan));
  plan->task = task;
 }

 if (task & MOD_DISABLE_UNSPEC) {
  curmod = mlist;
  while (curmod) {
   struct smodule *tmp = curmod->module;
   if (curmod->status & STATUS_ENABLED) {
    if (tmp) {
     if ((tmp->mode & EINIT_MOD_FEEDBACK) && !(task & MOD_DISABLE_UNSPEC_FEEDBACK) ||
         (tmp->rid && strinset (atoms, tmp->rid)))
      goto skipcurmod;
     if (tmp->provides && atoms) {
      for (si = 0; atoms[si]; si++)
       if (strinset (tmp->provides, atoms[si]))
        goto skipcurmod;
     }
    }
    struct mloadplan *cplan = (struct mloadplan *)ecalloc (1, sizeof (struct mloadplan));
    cplan->task = MOD_DISABLE;
    cplan->mod = curmod;
    cplan->options = MOD_PLAN_IDLE;
    if (curmod->module) {
     if (curmod->module->requires) cplan->requires = (char **)setdup ((void **)curmod->module->requires, -1);
     if (curmod->module->provides) cplan->provides = (char **)setdup ((void **)curmod->module->provides, -1);
    }
    pthread_mutex_init (&cplan->mutex, NULL);
    plan->orphaned = (struct mloadplan **)setadd ((void **)plan->orphaned, (void *)cplan, -1);
   }
   skipcurmod:
   curmod = curmod->next;
  }
 }

 if (atoms) {
  atoms = (char **)setdup ((void **)atoms, -1);
  for (si = 0; atoms[si]; si++) {
   struct lmodule **cand = (struct lmodule **)ecalloc (mcount+1, sizeof (struct lmodule *));
   struct mloadplan *cplan = NULL;
   struct mloadplan *tcplan = NULL;
   struct mloadplan **planl = NULL;
   unsigned int cc = 0, npcc;
   curmod = mlist;
   char **groupatoms = NULL;
   uint32_t groupoptions = 0;
   struct cfgnode *gnode = NULL;

   while (gnode = cfg_findnode (atoms[si], 0, gnode)) {
    uint32_t gni = 0;
    if (gnode->arbattrs)
     for (; gnode->arbattrs[gni]; gni+=2) {
      if (!strcmp (gnode->arbattrs[gni], "group")) {
       char **gatoms = str2set (':', gnode->arbattrs[gni+1]);
       int32_t gatomi = 0;
       groupoptions |= MOD_PLAN_GROUP;
       if (gatoms) {
        for (; gatoms[gatomi]; gatomi++)
         groupatoms = (char **)setadd ((void **)groupatoms, (void *) gatoms[gatomi], -1);
//        free (gatoms);
       }
      } else if (!strcmp (gnode->arbattrs[gni], "seq")) {
       if (!strcmp (gnode->arbattrs[gni+1], "most")) {
        groupoptions |= MOD_PLAN_GROUP_SEQ_MOST;
       }
       else if (!strcmp (gnode->arbattrs[gni+1], "any"))
        groupoptions |= MOD_PLAN_GROUP_SEQ_ANY;
       else if (!strcmp (gnode->arbattrs[gni+1], "any-iop"))
        groupoptions |= MOD_PLAN_GROUP_SEQ_ANY_IOP;
       else if (!strcmp (gnode->arbattrs[gni+1], "all"))
        groupoptions |= MOD_PLAN_GROUP_SEQ_ALL;
      }
     }
   }

   while (curmod) {
    struct smodule *tmp = curmod->module;
    if (tmp) {
     uint32_t gai;
     if ((tmp->rid && !strcmp (tmp->rid, atoms[si])) ||
        (tmp->provides && strinset (tmp->provides, atoms[si]))) {
      addmodtocandidates:
      cand[cc] = curmod;
      cc++;
      curmod = curmod->next;
      continue;
     }
     if (groupatoms) {
      for (gai = 0; groupatoms[gai]; gai++) {
       if (tmp->provides && strinset (tmp->provides, groupatoms[gai])) {
        goto addmodtocandidates;
       }
      }
     }
    }
    curmod = curmod->next;
   }
   free (groupatoms);
//   printf ("looking for \"%s\": %i candidate(s)\n", atoms[si], cc);

   if (cc) {
    if (mod_plan_sort_by_preference (cand, atoms[si])) {
     return NULL;
    }
    cplan = (struct mloadplan *)ecalloc (1, sizeof (struct mloadplan));
    cplan->task = task;
    pthread_mutex_init (&cplan->mutex, NULL);
    if (cc == 1) {
     cplan->mod = cand[0];
     cplan->options = MOD_PLAN_IDLE;
     if (groupatoms) {
      cplan->provides = (char **)setadd ((void **)cplan->provides, (void **)atoms[si], -1);
      if (cand[0]->module) {
       if (cand[0]->module->requires) {
        int ir = 0;
        for (; cand[0]->module->requires[ir]; ir++) {
         cplan->requires = (char **)setadd ((void **)cplan->requires, (void *)cand[0]->module->requires[ir], -1);
        }
       }
       if (cand[0]->module->provides) {
        int ir = 0;
        for (; cand[0]->module->provides[ir]; ir++) {
         cplan->provides = (char **)setadd ((void **)cplan->provides, (void *)cand[0]->module->provides[ir], -1);
        }
       }
      }
     } else {
      if (cand[0]->module) {
       if (cand[0]->module->requires) cplan->requires = (char **)setdup ((void **)cand[0]->module->requires, -1);
       if (cand[0]->module->provides) cplan->provides = (char **)setdup ((void **)cand[0]->module->provides, -1);
      }
     }
    } else if (cc > 1) {
     unsigned int icc = 0;
     planl = (struct mloadplan **)ecalloc (cc+1, sizeof (struct mloadplan *));
     cplan->group = planl;
     if (groupoptions)
      cplan->options |= groupoptions;
     else
      cplan->options = MOD_PLAN_IDLE + MOD_PLAN_GROUP + MOD_PLAN_GROUP_SEQ_ANY_IOP;

     cplan->provides = (char **)setadd ((void **)cplan->provides, (void **)atoms[si], -1);
     for (; icc < cc; icc++) {
      tcplan = (struct mloadplan *)ecalloc (1, sizeof (struct mloadplan));
      tcplan->task = task;
      tcplan->mod = cand[icc];
      tcplan->options = MOD_PLAN_IDLE;
      pthread_mutex_init (&tcplan->mutex, NULL);
      if (cand[icc]->module) {
       if (cand[icc]->module->requires) {
        int ir = 0;
        for (; cand[icc]->module->requires[ir]; ir++) {
         cplan->requires = (char **)setadd ((void **)cplan->requires, (void *)cand[icc]->module->requires[ir], -1);
        }
       }
       if (cand[icc]->module->provides) {
        int ir = 0;
        for (; cand[icc]->module->provides[ir]; ir++) {
         cplan->provides = (char **)setadd ((void **)cplan->provides, (void *)cand[icc]->module->provides[ir], -1);
        }
       }
       cplan->provides = (char **)setadd ((void **)cplan->provides, (void **)cand[icc]->module->rid, -1);
      }
      cplan->group[icc] = tcplan;
     }
    }
    nplancand = (struct mloadplan **)setadd ((void **)nplancand, (void *)cplan, -1);

    if (plan && plan->unsatisfied) {
     plan->unsatisfied = strsetdel (plan->unsatisfied, atoms[si]);
    }
   } else {
    if (plan && plan->unsatisfied && strinset (plan->unsatisfied, atoms [si])) {
     char *tmpa = estrdup (atoms[si]);
     printf ("can't satisfy atom: %s\n", atoms[si]);
     plan->unsatisfied = strsetdel (plan->unsatisfied, atoms[si]);
     plan->unavailable = (char **)setadd ((void **)plan->unavailable, (void *)tmpa, -1);
     return plan;
    }
   }

   free (cand);
  }
  free (atoms);
 }

 plan->orphaned = (struct mloadplan **)setcombine ((void **)plan->orphaned, (void **)nplancand, -1);

 plan = mod_plan_restructure(plan);
 if (plan && plan->unsatisfied && plan->unsatisfied[0])
  mod_plan (plan, plan->unsatisfied, task);

 return plan;
}

unsigned int mod_plan_commit (struct mloadplan *plan) {
 int32_t i, status;
 pthread_t **childthreads = NULL;
 if (!plan) return STATUS_FAIL;
 pthread_mutex_lock(&plan->mutex);
 if (plan->options & MOD_PLAN_OK) return STATUS_OK;
 if (plan->options & MOD_PLAN_FAIL) return STATUS_FAIL;
 if (!plan->mod) {
//  puts ("starting group");
  if  (!(plan->options & MOD_PLAN_GROUP)) status = STATUS_OK;
  else if (!plan->group) status = STATUS_FAIL;
  else {
   if (plan->options & MOD_PLAN_GROUP_SEQ_MOST) {
    plan->position = 0;
//    puts ("assuming OK unless something happens");
    status = STATUS_OK;
   } else
    status = STATUS_FAIL;
   for (; plan->group[plan->position]; plan->position++) {
    uint32_t retval;
    retval = mod_plan_commit (plan->group[plan->position]);
    if (retval == STATUS_ENABLED) retval = STATUS_OK;
    if (retval == STATUS_DISABLED) retval = STATUS_OK;
    if (plan->options & MOD_PLAN_GROUP_SEQ_ALL) {
     switch (retval) {
      case STATUS_FAIL_REQ:
       status = STATUS_FAIL_REQ;
       goto endofmodaction;
       break;
      case STATUS_FAIL:
       status = STATUS_FAIL;
       goto endofmodaction;
       break;
      case STATUS_IDLE:
      case STATUS_OK:
       status = STATUS_OK;
       break;
     }
    } else if (plan->options & MOD_PLAN_GROUP_SEQ_ANY_IOP) {
     switch (retval) {
      case STATUS_FAIL_REQ:
       status = STATUS_FAIL_REQ;
       goto endofmodaction;
       break;
      case STATUS_FAIL:
       break;
      case STATUS_IDLE:
      case STATUS_OK:
       status = STATUS_OK;
       goto endofmodaction;
       break;
     }
    } else if (plan->options & MOD_PLAN_GROUP_SEQ_ANY) {
     switch (retval) {
      case STATUS_FAIL_REQ:
      case STATUS_FAIL:
       break;
      case STATUS_IDLE:
      case STATUS_OK:
       status = STATUS_OK;
       goto endofmodaction;
       break;
     }
    } else if (plan->options & MOD_PLAN_GROUP_SEQ_MOST) {
     switch (retval) {
      case STATUS_FAIL_REQ:
//       puts ("status set to FAIL_REQ");
       status = STATUS_FAIL_REQ;
       break;
      case STATUS_FAIL:
      case STATUS_OK:
      case STATUS_IDLE:
//       puts ("status unchanged");
       break;
     }
    }
   }
  }
 } else {
  status = mod(plan->task, plan->mod);
  if ((status == STATUS_ENABLED) && (plan->task & MOD_ENABLE)) status = STATUS_OK;
  else if ((status == STATUS_DISABLED) && (plan->task & MOD_DISABLE)) status = STATUS_OK;
 }
 endofmodaction:

 if (status & STATUS_OK) {
//  if (plan->group && (plan->options & MOD_PLAN_GROUP_SEQ_MOST)) {
//   puts ("group/most OK");
/* this will need to be reworked a little... no way to figure out when the group is not being provided anymore */
  if (plan->provides && plan->provides [0] && (plan->task & MOD_ENABLE))
   provided = (char **)setadd ((void **)provided, (void *)plan->provides[0], -1);
//  }
  if (plan->right)
   for (i = 0; plan->right[i]; i++) {
    pthread_t *th = ecalloc (1, sizeof (pthread_t));
    if (!pthread_create (th, NULL, (void * (*)(void *))mod_plan_commit, (void*)plan->right[i])) {
     childthreads = (pthread_t **)setadd ((void **)childthreads, (void *)th, -1);
    }
//    mod_plan_commit (plan->right[i]);
   }
 } else if (plan->left) {
  for (status = 0; plan->left[i]; i++) {
   pthread_t *th = ecalloc (1, sizeof (pthread_t));
   if (!pthread_create (th, NULL, (void * (*)(void *))mod_plan_commit, (void*)plan->left[i])) {
    childthreads = (pthread_t **)setadd ((void **)childthreads, (void *)th, -1);
   }
  }
 }

 if (childthreads) {
  for (i = 0; childthreads[i]; i++) {
   int32_t returnvalue;
   pthread_join (*(childthreads[i]), (void**) &returnvalue);
/*   switch (returnvalue) {
    case STATUS_OK:
     puts ("success."); break;
    case STATUS_FAIL_REQ:
     puts ("can't load yet."); break;
   }*/
   free (childthreads[i]);
  }
  free (childthreads);
 }

 switch (status) {
  case STATUS_OK:
   plan->options &= MOD_PLAN_OK;
   plan->options |= MOD_PLAN_IDLE;
   plan->options ^= MOD_PLAN_IDLE;
   break;
  case STATUS_FAIL:
   plan->options &= MOD_PLAN_FAIL;
   plan->options |= MOD_PLAN_IDLE;
   plan->options ^= MOD_PLAN_IDLE;
   break;
 }

 pthread_mutex_unlock(&plan->mutex);
 return status;
// return STATUS_OK;
 panic:
  pthread_mutex_unlock(&plan->mutex);
  bitch (BTCH_ERRNO);
  return STATUS_FAIL;
}

/* free all elements in the plan */
int mod_plan_free (struct mloadplan *plan) {
 struct uhash *hash_list;
 struct uhash *d;

 hash_list = mod_plan2hash (plan, NULL, MOD_P2H_LIST);

 d = hash_list;
 while (d) {
  if (d->value) {
   struct mloadplan *v = (struct mloadplan *)d->value;
//   if (v->right) free (v->right);
//   if (v->left) free (v->left);
//   if (v->orphaned) free (v->orphaned);
//   free (d->value);
//   d->value = NULL;
  }
  d = hashnext (d);
 }

 hashfree (hash_list);
}

#ifdef DEBUG
/* debugging functions: only available if DEBUG is set (obviously...) */
void mod_ls () {
 struct lmodule *cur = mlist;
 do {
  if (cur->module != NULL) {
   if (cur->module->rid)
    fputs (cur->module->rid, stdout);
   if (cur->module->name)
    printf (" (%s)", cur->module->name, stdout);
   puts ("");
  } else
   puts ("(NULL)");
  cur = cur->next;
 } while (cur != NULL);
}

void mod_plan_ls (struct mloadplan *plan) {
 char *rid = "n/a", *name = "unknown", *action;
 static int recursion;
 unsigned char pass = 0;
 struct mloadplan **cur;
 int i;
 if (!recursion) puts ("committing this plan will...");
 if (!plan) return;
 if (plan->mod) {
  if (plan->mod->module) {
   if (plan->mod->module->rid)
    rid = plan->mod->module->rid;
   if (plan->mod->module->name)
    name = plan->mod->module->name;
  }
 } else if (plan->options & MOD_PLAN_GROUP) {
  if (plan->provides && plan->provides[0])
   rid = plan->provides[0];
  else
   rid = "group";
  if (plan->options & MOD_PLAN_GROUP_SEQ_ANY)
   name = "any element";
  else if (plan->options & MOD_PLAN_GROUP_SEQ_ANY_IOP)
   name = "any element, in order of preference";
  else if (plan->options & MOD_PLAN_GROUP_SEQ_MOST)
   name = "most elements";
  else if (plan->options & MOD_PLAN_GROUP_SEQ_ALL)
   name = "all elements";
 }
 for (i = 0; i < recursion; i++)
  fputs (" ", stdout);
 switch (plan->task) {
  case MOD_ENABLE:
   action = "enable"; break;
  case MOD_DISABLE:
   action = "disable"; break;
  default:
   action = "do something with..."; break;
 }
 printf ("%s %s (%s)\n", action, rid, name);
 while (pass < 4) {
  recursion++;
  switch (pass) {
   case 0:
    if (plan->left && plan->left[0]) {
     for (i = 0; i < recursion; i++)
      fputs (" ", stdout);
     cur = plan->left;
     puts ("on failure {");
     break;
    }
    pass++;
   case 1:
    if (plan->right && plan->right[0]) {
     for (i = 0; i < recursion; i++)
      fputs (" ", stdout);
     cur = plan->right;
     puts ("on success {");
     break;
    }
    pass++;
   case 2:
    if (plan->orphaned && plan->orphaned[0]) {
     for (i = 0; i < recursion; i++)
      fputs (" ", stdout);
	 cur = plan->orphaned;
     puts ("orphans {");
	 break;
    }
	pass++;
   case 3:
    if (plan->group && plan->group[0]) {
     for (i = 0; i < recursion; i++)
      fputs (" ", stdout);
	 cur = plan->group;
     puts ("GROUP {");
	 break;
    }
	pass++;
   default:
    recursion--;
    goto unsat;
  }

  recursion++;
  for (i = 0; cur[i]; i++) {
   if (cur[i] != plan)
    mod_plan_ls (cur[i]);
   else {
    puts ("Circular dependency detected, aborting.");
	recursion-=2;
    return;
   }
  }
  recursion--;
  for (i = 0; i < recursion; i++)
   fputs (" ", stdout);
  puts ("}");

  pass++;
  recursion--;
 }

 unsat:

/* if (plan->provides && plan->provides[0]) {
  for (i = 0; plan->provides[i]; i++) {
   int ic = -1;
   for (; ic < recursion; ic++)
    fputs (" ", stdout);
   printf ("provides: %s\n", plan->provides[i]);
  }
 }


 if (plan->requires && plan->requires[0]) {
  for (i = 0; plan->requires[i]; i++) {
   int ic = -1;
   for (; ic < recursion; ic++)
    fputs (" ", stdout);
   printf ("requires: %s\n", plan->requires[i]);
  }
 }

 if (plan->unsatisfied && plan->unsatisfied[0]) {
  for (i = 0; plan->unsatisfied[i]; i++) {
   int ic = -1;
   for (; ic < recursion; ic++)
    fputs (" ", stdout);
   printf ("unsatisfied dependency: %s\n", plan->unsatisfied[i]);
  }
 }*/

 if (plan->unavailable && plan->unavailable[0]) {
  for (i = 0; plan->unavailable[i]; i++) {
   int ic = -1;
   for (; ic < recursion; ic++)
    fputs (" ", stdout);
   printf ("unavailable dependency: %s\n", plan->unavailable[i]);
  }
 }
}
#endif
