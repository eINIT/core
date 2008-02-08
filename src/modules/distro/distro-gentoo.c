/*
 *  distro-gentoo.c
 *  einit
 *
 *  Created by Magnus Deininger on 10/11/2006.
 *  Renamed from compatibility-sysv-gentoo.c on 19/08/2007.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <einit-modules/exec.h>
#include <einit-modules/parse-sh.h>

#include <rc.h>
#if 0
#include <einfo.h>
#endif

#include <regex.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int compatibility_sysv_gentoo_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule compatibility_sysv_gentoo_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Distribution Support: Gentoo",
 .rid       = "distro-gentoo",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = compatibility_sysv_gentoo_configure
};

module_register(compatibility_sysv_gentoo_self);

#endif

struct stree *service_group_transformations = NULL;

struct service_group_transformation {
 char *out;
 regex_t *pattern;
};

char  do_service_tracking = 0,
     *service_tracking_path = NULL,
      is_gentoo_system = 0,
     *init_d_exec_scriptlet = NULL;
time_t profile_env_mtime = 0;

int compatibility_sysv_gentoo_scanmodules (struct lmodule *);
int compatibility_sysv_gentoo_init_d_enable (char *, struct einit_event *);
int compatibility_sysv_gentoo_init_d_disable (char *, struct einit_event *);
int compatibility_sysv_gentoo_init_d_reset (char *, struct einit_event *);
int compatibility_sysv_gentoo_init_d_custom (char *, char *, struct einit_event *);
int compatibility_sysv_gentoo_configure (struct lmodule *);
int compatibility_sysv_gentoo_cleanup (struct lmodule *);
void sh_add_environ_callback (char **, uint8_t, void *);
void parse_gentoo_runlevels (char *, struct cfgnode *, char);
void einit_event_handler (struct einit_event *);
void ipc_event_handler (struct einit_event *);
int compatibility_sysv_gentoo_cleanup_after_module (struct lmodule *);

#define SVCDIR "/lib/rcscripts/init.d"

char svcdir_init_done = 0;

void gentoo_fixname (char *name) {
 ssize_t i = 0;
 for (; name[i]; i++) {
  if (name[i] == '.') name[i] = '-';
 }
}

void gentoo_fixname_set (char **set) {
 uint32_t i = 0;

 for (; set[i]; i++) {
  gentoo_fixname(set[i]);
  if (strmatch (set[i], "*")) {
   set[i] = (char *)str_stabilise (".*");
  }
 }
}

/* functions that module tend to need */
int compatibility_sysv_gentoo_cleanup (struct lmodule *irr) {
 event_ignore (einit_ipc_request_generic, ipc_event_handler);
 event_ignore (einit_event_subsystem_core, einit_event_handler);

 parse_sh_cleanup (irr);
 exec_cleanup(irr);

 return 0;
}

void sh_add_environ_callback (char **data, uint8_t status, void *ignored) {
 char *x, *y;

// if (data)
//  puts (set2str(' ', data));

 if (status == pa_new_context) {
  if (data && (x = data[0])) {
   if ((!strcmp (data[0], "export") || (x = data[1])) && (y = strchr (x, '='))) {
// if we get here, we got ourselves a variable definition
    struct cfgnode nnode;
    memset (&nnode, 0, sizeof(struct cfgnode));
    char *narb[4] = { "id", x, "s", (y+1) }, *yt = NULL;

    *y = 0; y++;

// exception for the PATH and ROOTPATH variable (gentoo usually mangles that around in /etc/profile)
    if (!strcmp (x, "PATH")) {
     return;
    } else if (!strcmp (x, "ROOTPATH")) {
     x = narb[1] = "PATH";
     yt = emalloc (strlen (y) + 30);
     *yt = 0;
     strcat (yt, "/sbin:/bin:/usr/sbin:/usr/bin");
     strcat (yt, y);
     narb[3] = yt;
    }

    nnode.id = (char *)str_stabilise ("configuration-environment-global");
    nnode.arbattrs = set_str_dup_stable (&narb);
    nnode.svalue = nnode.arbattrs[3];
//    nnode.source = self->rid;
//    nnode.source_file = "/etc/profile.env";

    cfg_addnode (&nnode);

    if (yt) {
     efree (yt);
     yt = NULL;
    }
//    puts (x);
//    puts (y);
   }
  }
 }
}

/* parse gentoo runlevels and add them as nodes to the main configuration */
void parse_gentoo_runlevels (char *path, struct cfgnode *currentmode, char exclusive) {
 DIR *dir = NULL;
 struct dirent *de = NULL;
 uint32_t plen;
 char *tmp = NULL;

 if (!path) return;
 plen = strlen (path) +2;

 if ((dir = eopendir (path))) {
  struct stat st;
  char **nservices = NULL;

  while ((de = ereaddir (dir))) {
   uint32_t xplen = plen + strlen (de->d_name);

   if (de->d_name[0] == '.') continue;

   tmp = (char *)emalloc (xplen);
   *tmp = 0;
   strcat (tmp, path);
   strcat (tmp, de->d_name);

   if (!stat (tmp, &st) && S_ISDIR (st.st_mode)) {
    struct cfgnode newnode;
    char **arbattrs = NULL;
    char **base = NULL;

    if (strcmp (de->d_name, "boot"))
     base = set_str_add (base, (void *)"boot");

// if not exclusive, merge current mode base with the new base
    if (!exclusive) {
     if ((currentmode = cfg_findnode (de->d_name, einit_node_mode, NULL)) && currentmode->arbattrs) {
      char **curmodebase = NULL;

      uint32_t i = 0;
      for (; currentmode->arbattrs[i]; i+=2) {
       if (!strcmp (currentmode->arbattrs[i], "base")) {
        curmodebase = str2set (':', currentmode->arbattrs[i+1]);
        break;
       }
      }

      if (curmodebase) {
       if (!base) base = curmodebase;
       else {
         for (i = 0; curmodebase[i]; i++) {
         if (!inset ((const void **)base, (void *)curmodebase[i], SET_TYPE_STRING)) {
          base = set_str_add (base, (void *)curmodebase[i]);
         }
        }
        efree (curmodebase);
       }
      }
     }

     currentmode = NULL;
    }

//    fprintf (stderr, " >> new mode: %s\n", de->d_name);

    memset (&newnode, 0, sizeof(struct cfgnode));

    arbattrs = set_str_add (arbattrs, (void *)"id");
    arbattrs = set_str_add (arbattrs, (void *)de->d_name);
    if (base) {
     char *nbase = set2str(':', (const char **)base);
     if (nbase) {
      arbattrs = set_str_add (arbattrs, (void *)"base");
      arbattrs = set_str_add (arbattrs, (void *)nbase);
      efree (nbase);
     }
    }

    newnode.type = einit_node_mode;
    newnode.id = (char *)str_stabilise(arbattrs[1]);
//    newnode.source   = self->rid;
    newnode.arbattrs = arbattrs;

    cfg_addnode (&newnode);

    if ((currentmode = cfg_findnode (newnode.id, einit_node_mode, NULL))) {
     tmp[xplen-2] = '/';
     tmp[xplen-1] = 0;
     parse_gentoo_runlevels (tmp, currentmode, exclusive);
     tmp[xplen-2] = 0;
     currentmode = NULL;
    }
   } else {
    nservices = set_str_add (nservices, (void *)de->d_name);
   }
  }

  if (nservices) {
   if (service_group_transformations) {
    struct stree *cur = streelinear_prepare(service_group_transformations);

    while (cur) {
     struct service_group_transformation *trans =
       (struct service_group_transformation *)cur->value;

     if (trans) {
      char **workingset = set_str_dup (nservices);
      char **new_services_for_group = NULL;

      if (workingset) {
       ssize_t ci = 0;
       for (; workingset[ci]; ci++) {
        if (!regexec (trans->pattern, workingset[ci], 0, NULL, 0)) {
         ssize_t ip = 0;
         nservices = strsetdel(nservices, workingset[ci]);
         for (; workingset[ci][ip]; ip++) {
          if (workingset[ci][ip] == '.') workingset[ci][ip] = '-';
         }

         new_services_for_group = set_str_add(new_services_for_group, (void *)workingset[ci]);
        }
       }
       efree (workingset);
      }

      if (new_services_for_group) {
       char *cfgid = emalloc (strlen(cur->key) + 16);
       char curgroup_has_seq_attribute = 0;
       char **arbattrs = NULL;
       struct cfgnode *curgroup = NULL;
       struct cfgnode newnode;

       *cfgid = 0;
       strcat (cfgid, "services-alias-");
       strcat (cfgid, cur->key);

       curgroup = cfg_getnode (cfgid, NULL);
       if (curgroup && curgroup->arbattrs) {
        uint32_t y = 0;
        for (; curgroup->arbattrs[y]; y+= 2) {
         if (!strcmp (curgroup->arbattrs[y], "group")) {
          uint32_t z = 0;
          char **groupmembers = str2set (':', curgroup->arbattrs[y+1]);

          if (groupmembers) {
           for (; groupmembers[z]; z++) {
            if (!inset ((const void **)new_services_for_group, (void *)groupmembers[z], SET_TYPE_STRING)) {
             new_services_for_group = set_str_add (new_services_for_group, (void *)groupmembers[z]);
            }
           }
           efree (groupmembers);
          }

         } else if (!strcmp (curgroup->arbattrs[y], "seq")) {
          curgroup_has_seq_attribute = 1;
          arbattrs = set_str_add (arbattrs, (void *)curgroup->arbattrs[y]);
          arbattrs = set_str_add (arbattrs, (void *)curgroup->arbattrs[y+1]);
         } else {
          arbattrs = set_str_add (arbattrs, (void *)curgroup->arbattrs[y]);
          arbattrs = set_str_add (arbattrs, (void *)curgroup->arbattrs[y+1]);
         }
        }
       }

       arbattrs = set_str_add (arbattrs, (void *)"group");
       arbattrs = set_str_add (arbattrs, (void *)set2str (':', (const char **)new_services_for_group));

       if (!curgroup_has_seq_attribute) {
        arbattrs = set_str_add (arbattrs, (void *)"seq");
        arbattrs = set_str_add (arbattrs, (void *)"most");
       }

       memset (&newnode, 0, sizeof(struct cfgnode));

       newnode.type = einit_node_regular;
//       newnode.mode     = currentmode;
       newnode.id       = cfgid;
//       newnode.source   = self->rid;
       newnode.arbattrs = arbattrs;

       cfg_addnode (&newnode);

       if (!inset ((const void **)nservices, (void *)cur->key, SET_TYPE_STRING))
        nservices = set_str_add(nservices, (void *)cur->key);
      }
     }

     cur = streenext(cur);
    }
   }

   if (nservices && currentmode) {
    char **arbattrs = NULL;
    char **critical;
    struct cfgnode newnode;
    uint32_t y = 0;

    for (; nservices[y]; y++) {
     uint32_t x = 0;
     for (; nservices[y][x]; x++) {
      if (nservices[y][x] == '.') nservices[y][x] = '-';
     }
    }

    if (!exclusive) {
     uint32_t i = 0;
     char **curmodeena = str2set (':', cfg_getstring ("enable/services", currentmode));

     if (curmodeena) {
      for (; curmodeena[i]; i++) {
       if (!inset ((const void **)nservices, (void *)curmodeena[i], SET_TYPE_STRING)) {
        nservices = set_str_add (nservices, (void *)curmodeena[i]);
       }
      }

//      fprintf (stderr, " >> DEBUG: mode \"%s\": before merge: %s;\n    now: %s\n", currentmode->id, set2str(' ', curmodeena), set2str(' ', nservices));

      efree (curmodeena);
     }
    }

    memset (&newnode, 0, sizeof(struct cfgnode));

    arbattrs = set_str_add (arbattrs, (void *)"services");
    arbattrs = set_str_add (arbattrs, (void *)set2str (':', (const char **)nservices));

    if ((critical = str2set (':', cfg_getstring ("enable/critical", currentmode)))) {
     arbattrs = set_str_add (arbattrs, (void *)"critical");
     arbattrs = set_str_add (arbattrs, (void *)critical);
    }

    newnode.type = einit_node_regular;
    newnode.mode     = currentmode;
    newnode.id       = (char *)str_stabilise("mode-enable");
//    newnode.source   = self->rid;
    newnode.arbattrs = arbattrs;

    cfg_addnode (&newnode);
   } else {
    fputs (" >> wtf?.\n", stderr);
   }

   if (nservices)
    efree (nservices);
  }

  eclosedir (dir);
 }
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_update_configuration) {
  struct stat st;

  init_d_exec_scriptlet = cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/execute", NULL);

  char *cs = cfg_getstring("configuration-compatibility-sysv-distribution", NULL);
  if (is_gentoo_system || (cs && ((!strcmp("gentoo", cs)) || ((!strcmp("auto", cs) && !stat("/etc/gentoo-release", &st)))))) {
   if (!is_gentoo_system) {
    is_gentoo_system = 1;
    fputs (" >> gentoo system detected\n", stderr);
    ev->chain_type = einit_core_configuration_update;
   }
/* env.d data */
   struct cfgnode *node = cfg_getnode ("configuration-compatibility-sysv-distribution-gentoo-parse-env.d", NULL);
   char *bpath = NULL;

   if (node && node->flag) {
    if (!stat ("/etc/profile.env", &st) && (st.st_mtime > profile_env_mtime)) {
     char *data = readfile ("/etc/profile.env");
     profile_env_mtime = st.st_mtime;

     if (data) {
//      puts ("compatibility-sysv-gentoo: updating configuration with env.d");
      parse_sh (data, (void (*)(const char **, enum einit_sh_parser_pa, void *))sh_add_environ_callback);

      efree (data);
     }
     ev->chain_type = einit_core_configuration_update;
    }
   }

/* runlevels */
   if ((bpath = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-runlevels/path"))) {
    parse_gentoo_runlevels (bpath, NULL, parse_boolean (cfg_getstring ("configuration-compatibility-sysv-distribution-gentoo-runlevels/exclusive", NULL)));
// need to add checks here for updated configuration
   }

/* service tracker */
   node = cfg_getnode ("configuration-compatibility-sysv-distribution-gentoo-softlevel-tracker", NULL);
   if ((do_service_tracking = (node && node->flag))) {
    service_tracking_path = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-softlevel-tracker/path");
    if (!service_tracking_path) do_service_tracking = 0;
   }
  }
 } else if (ev->type == einit_core_configuration_update) {
  struct cfgnode *node = NULL;
  struct stree *new_transformations = NULL, *ca;

  while ((node = cfg_findnode ("configuration-compatibility-sysv-distribution-gentoo-service-group", 0, node))) {
   if (node->arbattrs) {
    struct service_group_transformation new_transformation;
    ssize_t sti = 0;
    char have_pattern = 0;

    memset (&new_transformation, 0, sizeof(struct service_group_transformation));

    for (; node->arbattrs[sti]; sti+=2) {
     if (!strcmp (node->arbattrs[sti], "put-into")) {
      new_transformation.out = node->arbattrs[sti+1];
     } else if (!strcmp (node->arbattrs[sti], "service")) {
      regex_t *buffer = emalloc (sizeof (regex_t));

      if ((have_pattern = !(eregcomp (buffer, node->arbattrs[sti+1])))) {
       new_transformation.pattern = buffer;
      }
     }
    }

    if (have_pattern && new_transformation.out) {
     new_transformations =
       streeadd (new_transformations, new_transformation.out, (void *)(&new_transformation), sizeof(new_transformation), new_transformation.pattern);
    }
   }
  }

  ca = service_group_transformations;
  service_group_transformations = new_transformations;
  if (ca)
   streefree (ca);
 } else if (ev->type == einit_core_service_update) { // update service status
  uint32_t i = 0;
//  eputs ("marking service status!\n", stderr);
  if (!ev->set) return;
  if (coremode & einit_mode_ipconly) return;
  if (coremode & einit_mode_sandbox) return;

  if (ev->status & status_working) {
   if (ev->task & einit_module_enable) {
    for (; ev->set[i]; i++)
     rc_mark_service (ev->set[i], rc_service_starting);
   } else if (ev->task & einit_module_disable) {
    for (; ev->set[i]; i++)
     rc_mark_service (ev->set[i], rc_service_stopping);
   }
  } else if (ev->status == status_idle) {
   for (; ev->set[i]; i++)
    rc_mark_service (ev->set[i], rc_service_inactive);
  } else {
   if (ev->status & status_enabled) {
    for (; ev->set[i]; i++)
     rc_mark_service (ev->set[i], rc_service_started);
   } else if (ev->status & status_disabled) {
    for (; ev->set[i]; i++)
     rc_mark_service (ev->set[i], rc_service_stopped);
   }
  }
 } else if (ev->type == einit_core_plan_update) { // set active "soft mode"
  if (do_service_tracking && ev->string) {
   char tmp[BUFFERSIZE];
   int slfile;

   snprintf (tmp, BUFFERSIZE, "updating softlevel to %s\n", ev->string);
   notice (4, tmp);

   snprintf (tmp, BUFFERSIZE, "%ssoftlevel", service_tracking_path);

   if ((slfile = open (tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) > 0) {
    errno = 0;

    write (slfile, amode->id, strlen(ev->string));
    write (slfile, "\n", 1);
    eclose (slfile);

    if (errno) {
     perror (" >> error writing to softlevel file");
     errno = 0;
    }
   } else {
    perror (" >> creating softlevel file");
   }
  }
 }
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && !strcmp(ev->argv[0], "examine") && !strcmp(ev->argv[1], "configuration")) {
  if (!cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d/path", NULL)) {
   fputs ("NOTICE: CV \"configuration-compatibility-sysv-distribution-gentoo-init.d/path\":\n  Not found: Gentoo Init Scripts will not be processed. (not a problem)\n", ev->output);
   ev->ipc_return++;
  } else if (!cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/svcdir-init", NULL)) {
   fputs ("NOTICE: CV \"configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/svcdir-init\":\n  Not found: Things might go haywire.\n", ev->output);
   ev->ipc_return++;
  }

  ev->implemented = 1;
 }
}

/* gentoo init.d support functions */

int compatibility_sysv_gentoo_cleanup_after_module (struct lmodule *this) {
#if 0
 if (this->module) {
 if (this->module->provides)
 efree (this->module->provides);
 if (this->module->requires)
 efree (this->module->requires);
 if (this->module->notwith)
 efree (this->module->notwith);
 efree (this->module);
}

 if (this->param) {
 efree (this->param);
}
#endif
 return 0;
}

char **gentoo_resolve_dependency_type (rc_depinfo_t *deptree, char *type, char **current, char *runlevel, char *name) {
 char **serv = NULL;
 char **tmp_dependencies = NULL;
 char **tmp_type = set_str_add (NULL, type);
 char **tmp_service = set_str_add (NULL, name);

 if ((tmp_dependencies = rc_get_depends(deptree, tmp_type, tmp_service, runlevel, RC_DEP_START))) {
  serv = set_str_dup_stable (tmp_dependencies);
//  eprintf (stderr, "deps: %s %s\n", set2str (' ', dependencies->services), set2str (' ', serv));

  efree (tmp_dependencies);
 }

 efree (tmp_type);

 if (current) {
  if (serv) {
   serv = (char **)setcombine ((const void **)serv, (const void **)current, SET_TYPE_STRING);
   efree (current);
  } else {
   return current;
  }
 }

 return serv;
}

void gentoo_add_dependencies (struct smodule *module, rc_depinfo_t *gentoo_deptree, char *name) {
// rc_depinfo_t *depinfo;
 char **runlevels = str2set (':', cfg_getstring ("compatibility-sysv-distribution-gentoo-runlevels-for-dependencies", NULL));
 int i = 0;

 if (!runlevels) runlevels = set_str_add (set_str_add (NULL, "boot"), "default");

 memset (&module->si, 0, sizeof(module->si));

 for (; runlevels[i]; i++) {
  module->si.requires = gentoo_resolve_dependency_type(gentoo_deptree, "ineed", module->si.requires, runlevels[i], name);
  module->si.provides = gentoo_resolve_dependency_type(gentoo_deptree, "iprovide", module->si.provides, runlevels[i], name);
  module->si.before = gentoo_resolve_dependency_type(gentoo_deptree, "ibefore", module->si.before, runlevels[i], name);
  module->si.after = gentoo_resolve_dependency_type(gentoo_deptree, "after", module->si.after, runlevels[i], name);
  module->si.after = gentoo_resolve_dependency_type(gentoo_deptree, "iuse", module->si.after, runlevels[i], name);
 }

 efree (runlevels);

// modinfo->si.after = str2set (' ', serv);

/* seems to be included already */
// module->si.provides = set_str_add (module->si.provides, (void *)name);

 if (module->si.requires) gentoo_fixname_set (module->si.requires);
 if (module->si.provides) gentoo_fixname_set (module->si.provides);
 if (module->si.before) gentoo_fixname_set (module->si.before);
 if (module->si.after) gentoo_fixname_set (module->si.after);
}

int compatibility_sysv_gentoo_scanmodules (struct lmodule *modchain) {
 DIR *dir;
 struct dirent *de;
 char *nrid = NULL,
      *init_d_path = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-init.d/path"),
      *tmp = NULL;
 uint32_t plen;
 struct smodule *modinfo;
 char *spattern;
 regex_t allowpattern, disallowpattern;
 unsigned char haveallowpattern = 0, havedisallowpattern = 0;

 if (!init_d_path || !is_gentoo_system) {
//  fprintf (stderr, " >> not parsing gentoo scripts: 0x%x, 0x%x, 0x%x\n", init_d_path, init_d_dependency_scriptlet, is_gentoo_system);
  return 0;
 }/* else {
  fprintf (stderr, " >> parsing gentoo scripts\n");
 }*/

 if (!svcdir_init_done && (coremode != einit_mode_ipconly) && (coremode != einit_mode_sandbox)) {
  char *cmd = cfg_getstring ("configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/svcdir-init", NULL);

  if (cmd) pexec(cmd, NULL, 0, 0, NULL, NULL, NULL, NULL);
  svcdir_init_done = 1;
 }

 if ((spattern = cfg_getstring ("configuration-compatibility-sysv-distribution-gentoo-init.d/pattern-allow", NULL))) {
  haveallowpattern = !eregcomp (&allowpattern, spattern);
 }

 if ((spattern = cfg_getstring ("configuration-compatibility-sysv-distribution-gentoo-init.d/pattern-disallow", NULL))) {
  havedisallowpattern = !eregcomp (&disallowpattern, spattern);
 }

 plen = strlen (init_d_path) +1;

 if ((dir = eopendir (init_d_path))) {
// load gentoo's default dependency information using librc.so's functions
  rc_depinfo_t *gentoo_deptree = rc_load_deptree();
  if (!gentoo_deptree) {
   fputs (" >> unable to load gentoo's dependency information.\n", stderr);
  }

#ifdef DEBUG
  puts (" >> reading directory");
#endif
  while ((de = ereaddir (dir))) {
   char doop = 1;

//   puts (de->d_name);

// filter .- and *.sh-files (or apply regex patterns)
   if (haveallowpattern && regexec (&allowpattern, de->d_name, 0, NULL, 0)) continue;
   if (havedisallowpattern && !regexec (&disallowpattern, de->d_name, 0, NULL, 0)) continue;

   tmp = (char *)emalloc (plen + strlen (de->d_name));
   struct stat sbuf;
//   struct lmodule *lm;
   *tmp = 0;
   strcat (tmp, init_d_path);
   strcat (tmp, de->d_name);
   if (!stat (tmp, &sbuf) && S_ISREG (sbuf.st_mode)) {
    char  tmpx[BUFFERSIZE];

    modinfo = emalloc (sizeof (struct smodule));
    memset (modinfo, 0, sizeof(struct smodule));
// make sure this module is only a last resort:
    modinfo->mode |= einit_module_deprecated;

    nrid = emalloc (8 + strlen(de->d_name));
    *nrid = 0;
    strcat (nrid, "gentoo-");
    strcat (nrid, de->d_name);

    snprintf (tmpx, BUFFERSIZE, "Gentoo-Style init.d Script (%s)", de->d_name);
    modinfo->name = (char *)str_stabilise (tmpx);
    modinfo->rid = (char *)str_stabilise(nrid);

    gentoo_add_dependencies (modinfo, gentoo_deptree, de->d_name);

    struct lmodule *lm = modchain;
    while (lm) {
     if (lm->source && !strcmp(lm->source, tmp)) {
      lm->param = (void *)str_stabilise (tmp);
      lm->enable = (int (*)(void *, struct einit_event *))compatibility_sysv_gentoo_init_d_enable;
      lm->disable = (int (*)(void *, struct einit_event *))compatibility_sysv_gentoo_init_d_disable;
      lm->custom = (int (*)(void *, char *, struct einit_event *))compatibility_sysv_gentoo_init_d_custom;
      lm->cleanup = compatibility_sysv_gentoo_cleanup_after_module;
      lm->module = modinfo;

      lm = mod_update (lm);
      doop = 0;
      break;
     }
     lm = lm->next;
    }

    if (doop) {
     struct lmodule *new = mod_add (NULL, modinfo);
     if (new) {
      new->source = (char *)str_stabilise (tmp);
      new->param = (char *)str_stabilise (tmp);
      new->enable = (int (*)(void *, struct einit_event *))compatibility_sysv_gentoo_init_d_enable;
      new->disable = (int (*)(void *, struct einit_event *))compatibility_sysv_gentoo_init_d_disable;
      new->custom = (int (*)(void *, char *, struct einit_event *))compatibility_sysv_gentoo_init_d_custom;
      new->cleanup = compatibility_sysv_gentoo_cleanup_after_module;
     }
    }

   }
   if (nrid) {
    efree (nrid);
    nrid = NULL;
   }
   if (tmp) {
    efree (tmp);
    tmp = NULL;
   }
  }

  rc_free_deptree (gentoo_deptree);

  eclosedir (dir);
 }

 if (haveallowpattern) { haveallowpattern = 0; eregfree (&allowpattern); }
 if (havedisallowpattern) { havedisallowpattern = 0; eregfree (&disallowpattern); }

 return 0;
}

// int __pexec_function (char *command, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **local_environment, struct einit_event *status);

int compatibility_sysv_gentoo_init_d_enable (char *init_script, struct einit_event *status) {
 char *bn = strrchr(init_script, '/');
 char *variables[7] = {
  "script-path", init_script,
  "script-name", bn ? bn+1 : init_script,
  "action", "start", NULL },
  *cmdscript = NULL,
  *xrev = NULL;
 char **env = rc_config_env (NULL);

 if (!init_script || !init_d_exec_scriptlet) return status_failed;
 if ((xrev = strrchr(init_script, '/'))) variables[3] = xrev+1;

 cmdscript = apply_variables (init_d_exec_scriptlet, (const char **)variables);

 return pexec (cmdscript, NULL, 0, 0, NULL, NULL, env, status);
}

int compatibility_sysv_gentoo_init_d_disable (char *init_script, struct einit_event *status) {
 char *bn = strrchr(init_script, '/');
 char *variables[7] = {
  "script-path", init_script,
  "script-name", bn ? bn+1 : init_script,
  "action", "stop", NULL },
  *cmdscript = NULL,
  *xrev = NULL;
 char **env = rc_config_env (NULL);

 if (!init_script || !init_d_exec_scriptlet) return status_failed;
 if ((xrev = strrchr(init_script, '/'))) variables[3] = xrev+1;

 cmdscript = apply_variables (init_d_exec_scriptlet, (const char **)variables);

 return pexec (cmdscript, NULL, 0, 0, NULL, NULL, env, status);
}

int compatibility_sysv_gentoo_init_d_custom (char *init_script, char *action, struct einit_event *status) {
 char *bn = strrchr(init_script, '/');
 char *variables[7] = {
  "script-path", init_script,
  "script-name", bn ? bn+1 : init_script,
  "action", action, NULL },
  *cmdscript = NULL,
  *xrev = NULL;
 char **env = rc_config_env (NULL);

 if (!init_script || !init_d_exec_scriptlet) return status_failed;
 if ((xrev = strrchr(init_script, '/'))) variables[3] = xrev+1;

 cmdscript = apply_variables (init_d_exec_scriptlet, (const char **)variables);

 return pexec (cmdscript, NULL, 0, 0, NULL, NULL, env, status);
}

int compatibility_sysv_gentoo_configure (struct lmodule *irr) {
 module_init (irr);

 thismodule->cleanup = compatibility_sysv_gentoo_cleanup;
 thismodule->scanmodules = compatibility_sysv_gentoo_scanmodules;

 exec_configure (irr);
 parse_sh_configure (irr);

 event_listen (einit_event_subsystem_core, einit_event_handler);
 event_listen (einit_ipc_request_generic, ipc_event_handler);

 return 0;
}

// no enable/disable functions: this is a passive module
