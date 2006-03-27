/***************************************************************************
 *            module.c
 *
 *  Mon Feb  6 15:27:39 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
/*
Copyright (c) 2006, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/module.h>
/*
 dynamic linker functions (POSIX)

 void *dlopen(const char *filename, int flag);
 char *dlerror(void);
 void *dlsym(void *handle, const char *symbol);
 int dlclose(void *handle);
*/

#include <pthread.h>

void *mod_comment_thread (struct mfeedback *);

struct lmodule *mlist = NULL;
struct lmodule mdefault = {
	NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL
};
int mcount = 0;

int mod_scanmodules () {
 DIR *dir;
 struct dirent *entry;
 char *tmp;
 int mplen;
 void *sohandle;
 struct lmodule *cmod = NULL, *nmod;

 if (sconfiguration == NULL) return bitch(BTCH_ERRNO);
 if (sconfiguration->modulepath == NULL) return bitch(BTCH_ERRNO);

 mplen = strlen (sconfiguration->modulepath) +1;
 dir = opendir (sconfiguration->modulepath);
 if (dir != NULL) {
  while (entry = readdir (dir)) {
   if (entry->d_name[0] == '.') continue;
   tmp = (char *)malloc (mplen + strlen (entry->d_name));
   if (tmp != NULL) {
	struct smodule *modinfo;
    int (*func)(void *);
    *tmp = 0;
    strcat (tmp, sconfiguration->modulepath);
    strcat (tmp, entry->d_name);
	dlerror ();
    sohandle = dlopen (tmp, RTLD_NOW);
	if (sohandle == NULL) {
	 puts (dlerror ());
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
   } else {
	closedir (dir);
	return bitch(BTCH_ERRNO);
   }
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
 free (m);
}

int mod_freemodules () {
 if (mlist != NULL)
  mod_freedesc (mlist);
 mlist = NULL;
 return 1;
}

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

int mod_add (void *sohandle, int (*enable)(void *, struct mfeedback *), int (*disable)(void *, struct mfeedback *), void *param, struct smodule *module) {
 struct lmodule *nmod, *cur;
 int (*scanfunc)(struct lmodule *);
 int (*comment) (struct mfeedback *);
 int (*ftload)  (void *, struct mfeedback *);
 int (*configfunc)(struct lmodule *);

 nmod = calloc (1, sizeof (struct lmodule));
 if (!nmod) return bitch(BTCH_ERRNO);

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

// this will do additional initialisation functions for certain module-types
 if (module && sohandle) {
// EINIT_MOD_LOADER modules will usually want to provide a function to scan
//  for modules so they can be included in the dependency chain
  if (module->mode & EINIT_MOD_LOADER) {
   scanfunc = (int (*)(struct lmodule *)) dlsym (sohandle, "scanmodules");
   if (scanfunc != NULL) {
    scanfunc (mlist);
   }
   else bitch(BTCH_ERRNO + BTCH_DL);
  }
// EINIT_MOD_FEEDBACK-type modules will usually want to provide a comment()-
//  function in order to provide feedback about how a module is loading...
  if (module->mode & EINIT_MOD_FEEDBACK) {
   comment = (int (*)(struct mfeedback *)) dlsym (sohandle, "comment");
   if (comment != NULL) {
    nmod->comment = comment;
   }
   else bitch(BTCH_ERRNO + BTCH_DL);
  }
// we need to scan for load and unload functions if NULL was supplied for these
  if (enable == NULL) {
   ftload = (int (*)(void *, struct mfeedback *)) dlsym (sohandle, "enable");
   if (ftload != NULL) {
    nmod->enable = ftload;
   }
  }
  if (disable == NULL) {
   ftload = (int (*)(void *, struct mfeedback *)) dlsym (sohandle, "disable");
   if (ftload != NULL) {
    nmod->disable = ftload;
   }
  }
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

int mod_enable (struct lmodule *module) {
 struct mfeedback *fb = (struct mfeedback *)malloc (sizeof (struct mfeedback));
 pthread_t *th = calloc (1, sizeof (pthread_t));
 int r = 0;
 if (!module) return 0;
 if (!fb) return bitch (BTCH_ERRNO);
 if (!th) return bitch (BTCH_ERRNO);
 if (!module->enable) return 0;

 if (mdefault.comment) {
  pthread_create (th, NULL, (void * (*)(void *))mod_comment_thread, (void*)fb);
  fb->module = module;
  fb->task = EI_VIS_TASK_LOAD;
  fb->status = STATUS_WORKING;
  fb->progress = 0;
 }

 r = module->enable (module->param, fb);
 pthread_join (*th, NULL);
 if (r == LOAD_OK) module->status = STATUS_LOADED;

 return r;
}

void *mod_comment_thread (struct mfeedback *p) {
 mdefault.comment (p);
 free (p);
 return NULL;
}

int mod_disable (struct lmodule *module) {
}

int mod_configure () {
 if (sconfiguration->feedbackmodule) {
  struct lmodule *feedback = mod_find(sconfiguration->feedbackmodule, EINIT_MOD_FEEDBACK);
  if (feedback && feedback->comment) mdefault.comment = feedback->comment;
 }
}

struct mdeptree *mod_create_deptree (char **modules) {
 struct mdeptree *root = (struct mdeptree *)calloc (1, sizeof(struct mdeptree));
 struct mdeptree *cur = root;
 struct lmodule *curmod = NULL;
 int si = 0;

 for (; modules[si] != NULL; si++) {
  struct lmodule **candidates = (struct lmodule **)calloc (mcount+1, sizeof (struct lmodule *));
  curmod = mlist;

  if ( !cur ) {
   bitch (BTCH_ERRNO);
   mod_free_deptree (root);
   return NULL;
  }
 }

 return root;
}

void mod_free_deptree (struct mdeptree *node) {
 if (node->alternative) mod_free_deptree(node->alternative);
 if (node->next) mod_free_deptree(node->next);
 if (node->down) mod_free_deptree(node->down);
 free (node);
}
