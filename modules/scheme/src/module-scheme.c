/*
 *  module-scheme.c
 *  einit
 *
 *  Created on 05/06/2007.
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>
#include <einit-modules/exec.h>

#include <einit-bundle/scheme.h>
#include <einit-bundle/scheme-private.h>

// this one doesn't seem to be defined
void scheme_call(scheme *sc, pointer func, pointer args);

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_scheme_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_scheme_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.scheme)",
 .rid       = "module-scheme",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_scheme_configure
};

module_register(module_scheme_self);

#endif

struct scheme_interface_data {
 scheme *interpreter;
 struct stree *hooks;
};

int module_scheme_scanmodules (struct lmodule *);

int module_scheme_cleanup (struct lmodule *pa) {
 return 0;
}

scheme **scheme_protected_interpreters = NULL;
char **scheme_have_modules = NULL;

pointer scheme_call_hook (scheme *sc, struct stree *hooks, char *hook) {
 pointer ahook;
 struct stree *st;

 notice (4, "scheme_call_hook(): want to call hook %s", hook);

 if (!sc) return NULL;
 if (!hooks) return sc->NIL;

 if ((st = streefind (hooks, hook, tree_find_first)) && (ahook = st->value)) {
  if (sc->vptr->is_symbol (ahook)) {
   scheme_apply0 (sc, sc->vptr->symname (ahook));
  } else {
   scheme_call (sc, ahook, sc->NIL);
  }

  return sc->value;
 }

 return sc->NIL;
}

/* enable, disable and custom functions for scheme submodules */
int scheme_enable (struct scheme_interface_data *sd, struct einit_event *status) {
 pointer retval;
 if (!sd) return status_failed;

 retval = scheme_call_hook (sd->interpreter, sd->hooks, "enable");

 if (retval == sd->interpreter->T) {
  return status_ok;
 }

 return status_failed;
}

int scheme_disable (struct scheme_interface_data *sd, struct einit_event *status) {
 pointer retval;
 if (!sd) return status_failed;

 retval = scheme_call_hook (sd->interpreter, sd->hooks, "disable");

 if (retval == sd->interpreter->T) {
  return status_ok;
 }

 return status_failed;
}

int scheme_custom (struct scheme_interface_data *sd, char *action, struct einit_event *status) {
 pointer retval;
 if (!sd) return status_failed;

 retval = scheme_call_hook (sd->interpreter, sd->hooks, action);

 if (retval == sd->interpreter->T) {
  return status_ok;
 }

 return status_failed;
}

/* wrapper for the notice() macro in the C api */
pointer scheme_notice (scheme *sc, pointer args) {
 if(args!=sc->NIL) {
  pointer carg = args;
  int severity = 9;

  if (sc->vptr->is_pair (carg) && sc->vptr->is_integer (sc->vptr->pair_car (carg))) {
   severity = sc->vptr->ivalue (sc->vptr->pair_car (carg));

   carg = sc->vptr->pair_cdr (carg);
  }

  while (sc->vptr->is_pair (carg)) {
   char *s = sc->vptr->string_value (sc->vptr->pair_car (carg));

   if (s) {
    notice (severity, s);
   }

   carg = sc->vptr->pair_cdr (carg);
  }
 }

 return sc->NIL;
}

/* wrapper for the pexec() call */
/* too simple right now, but it'll get better, promise ;) */
pointer scheme_pexec (scheme *sc, pointer args) {
 if(args!=sc->NIL) {
  char *command;

  if (sc->vptr->is_pair (args) && sc->vptr->is_string (sc->vptr->pair_car (args)) && (command = sc->vptr->string_value (sc->vptr->pair_car (args)))) {
//   pexec (command, (const char **)variables, uid, gid, user, group, environment, status);
   int retv = pexec (command, NULL, 0, 0, NULL, NULL, NULL, NULL);

   switch (retv) {
    case status_ok: return sc->T;
    case status_failed: return sc->F;
   }
  }
 }

 return sc->NIL;
}


/* functions for configuration variables */
pointer scheme_configuration_get (scheme *sc, pointer args) {
 if(args!=sc->NIL) {
  char *name;

  if (sc->vptr->is_pair (args) && sc->vptr->is_string (sc->vptr->pair_car (args))) {
   if ((name = sc->vptr->string_value (sc->vptr->pair_car (args)))) {
    struct cfgnode *node = cfg_getnode (name, NULL);

    if (node && node->arbattrs) {
     uint32_t i = 0;
     pointer returnvalue = sc->NIL;

     for (; node->arbattrs[i]; i+=2) {
      returnvalue = immutable_cons (sc,
       immutable_cons (sc,
        sc->vptr->mk_string(sc, node->arbattrs[i]),
        sc->vptr->mk_string(sc, node->arbattrs[i+1])),
       returnvalue);
     }

     return returnvalue;
    }
   }
  }
 }

 return sc->NIL;
}

pointer scheme_configuration_add (scheme *sc, pointer args) {
 if(args!=sc->NIL) {
  char *name;

  if (sc->vptr->is_pair (args) && sc->vptr->is_string (sc->vptr->pair_car (args))) {
   if ((name = sc->vptr->string_value (sc->vptr->pair_car (args)))) {
    pointer carg = sc->vptr->pair_cdr (args);
    char **nargs = NULL;

    if (carg == sc->NIL) return sc->NIL;

    while (sc->vptr->is_pair (carg)) {
     pointer item = sc->vptr->pair_car (carg);

     if (sc->vptr->is_pair (item)) {
      pointer car = sc->vptr->pair_car (item);
      pointer cdr = sc->vptr->pair_cdr (item);

      if (sc->vptr->is_string (car) && sc->vptr->is_string (cdr)) {
       char *aname = sc->vptr->string_value(car);
       char *avalue = sc->vptr->string_value(cdr);

       if (aname && avalue) {
        nargs = (char **)setadd ((void **)nargs, aname, SET_TYPE_STRING);
        nargs = (char **)setadd ((void **)nargs, avalue, SET_TYPE_STRING);
       }
      }
     }
     sc->vptr->pair_car (carg);

     carg = sc->vptr->pair_cdr (carg);
    }

    if (nargs) {
     struct cfgnode newnode;
     memset (&newnode, 0, sizeof (struct cfgnode));

     newnode.id = estrdup (name);
     newnode.arbattrs = nargs;

     cfg_addnode (&newnode);
    }
   }
  }
 }

 return sc->NIL;
}

// now let's get to the actual interface :D
pointer scheme_make_module (scheme *sc, pointer args) {
 if(args!=sc->NIL) {
  struct smodule *sm = emalloc (sizeof (struct smodule));
  memset (sm, 0, sizeof (struct smodule));

  if (sc->vptr->is_pair (args) && sc->vptr->is_string (sc->vptr->pair_car (args)) && (sm->rid = estrdup(sc->vptr->string_value (sc->vptr->pair_car (args))))) {
   args = sc->vptr->pair_cdr (args);

   if ((args != sc->NIL) && sc->vptr->is_pair (args) && sc->vptr->is_string (sc->vptr->pair_car (args)) && (sm->name = estrdup(sc->vptr->string_value (sc->vptr->pair_car (args))))) {
    struct lmodule *lm = NULL;
    pointer cur = sc->vptr->pair_cdr (args);
    struct scheme_interface_data *ifd;

    if ((cur!=sc->NIL) && sc->vptr->is_pair (cur) && sc->vptr->is_pair (sc->vptr->pair_car (cur))) { /* parse dependency-block */
     char item = 0;
     pointer dcur = sc->vptr->pair_car (cur);

     while ((dcur!=sc->NIL) && sc->vptr->is_pair (dcur)) { /* parse list with reqs */
      pointer depcur = sc->vptr->pair_car (dcur);

      while ((depcur!=sc->NIL) && sc->vptr->is_pair (depcur)) { /* build a set from the data */
       if (sc->vptr->is_string (sc->vptr->pair_car (depcur))) {
        switch (item) {
         case 0:
          sm->si.provides = (char **)setadd ((void **)sm->si.provides, sc->vptr->string_value (sc->vptr->pair_car (depcur)), SET_TYPE_STRING);
          break;
         case 1:
          sm->si.requires = (char **)setadd ((void **)sm->si.requires, sc->vptr->string_value (sc->vptr->pair_car (depcur)), SET_TYPE_STRING);
          break;
         case 2:
          sm->si.after = (char **)setadd ((void **)sm->si.after, sc->vptr->string_value (sc->vptr->pair_car (depcur)), SET_TYPE_STRING);
          break;
         case 3:
          sm->si.before = (char **)setadd ((void **)sm->si.before, sc->vptr->string_value (sc->vptr->pair_car (depcur)), SET_TYPE_STRING);
          break;
        }
       }

       depcur = sc->vptr->pair_cdr (depcur);
      }

      item++;
      dcur = sc->vptr->pair_cdr (dcur);
     }

     cur = sc->vptr->pair_cdr (cur);
    }

    if ((lm = mod_add (NULL, sm))) {
     ifd = emalloc (sizeof (struct scheme_interface_data));
     memset (ifd, 0, sizeof (struct scheme_interface_data));
     lm->param = ifd;
     lm->enable = (int (*)(void *, struct einit_event *))scheme_enable;
     lm->disable = (int (*)(void *, struct einit_event *))scheme_disable;
     lm->custom = (int (*)(void *, char *, struct einit_event *))scheme_custom;

     ifd->interpreter = sc;

     if (!inset ((const void **)scheme_protected_interpreters, sc, SET_NOALLOC)) {
      scheme_protected_interpreters = (scheme **)setadd ((void **)scheme_protected_interpreters, sc, SET_NOALLOC);
     }

     while ((cur != sc->NIL) && sc->vptr->is_pair (cur)) {
      pointer dcur = sc->vptr->pair_car (cur);

      if ((dcur != sc->NIL) && sc->vptr->is_pair (dcur) && sc->vptr->is_string(sc->vptr->pair_car (dcur))) {
       ifd->hooks = streeadd (ifd->hooks, sc->vptr->string_value(sc->vptr->pair_car (dcur)), sc->vptr->pair_cdr (dcur), SET_NOALLOC, NULL);
      }

      cur = sc->vptr->pair_cdr (cur);
     }
    }
   }
  }
 }

 return sc->NIL;
}

int module_scheme_scanmodules ( struct lmodule *modchain ) {
 char **modules = NULL;
 char *initfile = cfg_getstring ("subsystem-scheme-init-file", NULL);
 char *init = initfile ? readfile (initfile) : NULL;

 modules = readdirfilter(cfg_getnode ("subsystem-scheme-import", NULL),
                                "/lib/einit/bootstrap-scheme/",
                                ".*\\.scheme$", NULL, 0);

 if (modules) {
  uint32_t i = 0;

  for (; modules[i]; i++) if (!inset ((const void **)scheme_have_modules, modules[i], SET_TYPE_STRING)) {
   char *data = readfile(modules[i]);
   if (data) {
    scheme *interpreter = scheme_init_new();

    if (scheme_init (interpreter)) {
     scheme_set_input_port_file(interpreter, stdin);
     scheme_set_output_port_file(interpreter, stderr);

     if (init) scheme_load_string(interpreter, init);

     interpreter->vptr->scheme_define (interpreter, interpreter->global_env, mk_symbol (interpreter, "notice"), mk_foreign_func (interpreter, scheme_notice));
     interpreter->vptr->scheme_define (interpreter, interpreter->global_env, mk_symbol (interpreter, "cfg-get"), mk_foreign_func (interpreter, scheme_configuration_get));
     interpreter->vptr->scheme_define (interpreter, interpreter->global_env, mk_symbol (interpreter, "cfg-add"), mk_foreign_func (interpreter, scheme_configuration_add));
     interpreter->vptr->scheme_define (interpreter, interpreter->global_env, mk_symbol (interpreter, "make-module"), mk_foreign_func (interpreter, scheme_make_module));
     interpreter->vptr->scheme_define (interpreter, interpreter->global_env, mk_symbol (interpreter, "pexec"), mk_foreign_func (interpreter, scheme_pexec));

//     notice (3, "have new scheme file: %s\n%s\n", modules[i], data);

     scheme_load_string(interpreter, data);
     free (data);

     if (!inset ((const void **)scheme_protected_interpreters, interpreter, SET_NOALLOC)) {
      scheme_deinit (interpreter);
     } else {
      scheme_have_modules = (char **)setadd ((void **)scheme_have_modules, modules[i], SET_TYPE_STRING);
     }
    } else {
     notice (1, "could not initialise scheme interpreter!");
    }
   }
  }
 } else {
  notice (2, "no scheme modules found.");
 }

 if (init) free (init);

 return 1;
}

int module_scheme_configure (struct lmodule *pa) {
 module_init (pa);
 exec_configure (pa);

 pa->scanmodules = module_scheme_scanmodules;
 pa->cleanup = module_scheme_cleanup;

 return 0;
}
