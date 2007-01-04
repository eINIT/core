/*
 *  compatibility-sysv-gentoo.c
 *  einit
 *
 *  Created by Magnus Deininger on 10/11/2006.
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

#define _MODULE

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

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

#define SH_PARSER_STATUS_LW              0
#define SH_PARSER_STATUS_READ            1
#define SH_PARSER_STATUS_IGNORE_TILL_EOL 2

#define PA_END_OF_FILE                   0x01
#define PA_NEW_CONTEXT                   0x02

const struct smodule self = {
    .eiversion    = EINIT_VERSION,
    .version      = 1,
    .mode         = EINIT_MOD_LOADER,
    .options      = 0,
    .name         = "System-V Compatibility: Gentoo Support",
    .rid          = "compatibility-sysv-gentoo",
    .si           = {
        .provides = NULL,
        .requires = NULL,
        .after    = NULL,
        .before   = NULL
    }
};

char  do_service_tracking = 0,
     *service_tracking_path = NULL,
      is_gentoo_system = 0,
     *init_d_dependency_scriptlet = NULL,
     *init_d_exec_scriptlet = NULL;
time_t profile_env_mtime = 0;

int scanmodules (struct lmodule *);
int init_d_enable (char *, struct einit_event *);
int init_d_disable (char *, struct einit_event *);
int init_d_reset (char *, struct einit_event *);
int init_d_reload (char *, struct einit_event *);
int configure (struct lmodule *);
int cleanup (struct lmodule *);
int parse_sh (char *, void (*)(char **, uint8_t));
void sh_add_environ_callback (char **, uint8_t);
void parse_gentoo_runlevels (char *, struct cfgnode *, char);
void einit_event_handler (struct einit_event *);
void ipc_event_handler (struct einit_event *);
int cleanup_after_module (struct lmodule *);

/* functions that module tend to need */
int configure (struct lmodule *irr) {
 exec_configure (irr);
 event_listen (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

int cleanup (struct lmodule *irr) {
 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
 event_ignore (EVENT_SUBSYSTEM_EINIT, einit_event_handler);
 exec_cleanup(irr);
}

// parse sh-style files and call back for each line
int parse_sh (char *data, void (*callback)(char **, uint8_t)) {
 if (!data) return -1;

 char *ndp = emalloc(strlen(data)), *cdp = ndp, *sdp = cdp,
      *cur = data-1,
      stat = SH_PARSER_STATUS_LW, squote = 0, dquote = 0, lit = 0,
      **command = NULL;

 while (*(cur +1)) {
  cur++;

  if (stat == SH_PARSER_STATUS_IGNORE_TILL_EOL) {
   if (*cur == '\n')
    stat = SH_PARSER_STATUS_LW;

   continue;
  }
//  putchar (*cur);

  if (lit) {
   lit = 0;
   *cdp = *cur;
   cdp++;
  } else switch (*cur) {
   case '#': stat = SH_PARSER_STATUS_IGNORE_TILL_EOL; break;
   case '\'': squote = !squote; break;
   case '\"': dquote = !dquote; break;
   case '\\': lit = 1; break;
   case '\n':
    if ((stat != SH_PARSER_STATUS_LW) && (cdp != sdp)) {
     *cdp = 0;
     command = (char**)setadd ((void**)command, (void*)sdp, SET_NOALLOC);
      cdp++;
      sdp = cdp;
    }

    stat = SH_PARSER_STATUS_LW;

    if (command) {
     callback (command, PA_NEW_CONTEXT);

     free (command);
     command = NULL;
    }

    break;
   default:
    if (dquote || squote) {
     *cdp = *cur;
     cdp++;
    } else if (isspace(*cur)) {
    if ((stat != SH_PARSER_STATUS_LW) && (cdp != sdp)) {
      *cdp = 0;
      command = (char**)setadd ((void**)command, (void*)sdp, SET_NOALLOC);
      cdp++;
      sdp = cdp;
     }
     stat = SH_PARSER_STATUS_LW;
    } else {
     *cdp = *cur;
     cdp++;
     stat = SH_PARSER_STATUS_READ;
    }

    break;
  }
 }

 callback (NULL, PA_END_OF_FILE);
 free (ndp);

 return 0;
}

void sh_add_environ_callback (char **data, uint8_t status) {
 char *x, *y;

// if (data)
//  puts (set2str(' ', data));

 if (status == PA_NEW_CONTEXT) {
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

    nnode.id = estrdup ("configuration-environment-global");
    nnode.arbattrs = (char **)setdup ((void **)&narb, SET_TYPE_STRING);
    nnode.svalue = nnode.arbattrs[3];
    nnode.source = self.rid;
//    nnode.source_file = "/etc/profile.env";

    cfg_addnode (&nnode);

    if (yt) {
     free (yt);
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

 if (dir = opendir (path)) {
  struct stat st;
  char **nservices = NULL;

  while (de = readdir (dir)) {
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
     base = (char **)setadd ((void **)base, (void *)"boot", SET_TYPE_STRING);

// if not exclusive, merge current mode base with the new base
    if (!exclusive) {
     if ((currentmode = cfg_findnode (de->d_name, EI_NODETYPE_MODE, NULL)) && currentmode->arbattrs) {
      char **curmodebase = NULL;

//      fprintf (stderr, " >> gentoo runlevels not exclusive, merging with what we have so far...\n");

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
         if (!inset ((void **)base, (void *)curmodebase[i], SET_TYPE_STRING)) {
          base = (char **)setadd ((void **)base, (void *)curmodebase[i], SET_TYPE_STRING);
         }
        }
        free (curmodebase);
       }
      }
     }

     currentmode = NULL;
    }

//    fprintf (stderr, " >> new mode: %s\n", de->d_name);

    memset (&newnode, 0, sizeof(struct cfgnode));

    arbattrs = (char **)setadd ((void **)arbattrs, (void *)"id", SET_TYPE_STRING);
    arbattrs = (char **)setadd ((void **)arbattrs, (void *)de->d_name, SET_TYPE_STRING);
    if (base) {
     char *nbase = set2str(':', base);
     if (nbase) {
      arbattrs = (char **)setadd ((void **)arbattrs, (void *)"base", SET_TYPE_STRING);
      arbattrs = (char **)setadd ((void **)arbattrs, (void *)nbase, SET_TYPE_STRING);
      free (nbase);
     }
    }

    newnode.nodetype = EI_NODETYPE_MODE;
    newnode.id = estrdup(arbattrs[1]);
    newnode.source   = self.rid;
    newnode.arbattrs = arbattrs;

    cfg_addnode (&newnode);

    if (currentmode = cfg_findnode (newnode.id, EI_NODETYPE_MODE, NULL)) {
     tmp[xplen-2] = '/';
     tmp[xplen-1] = 0;
     parse_gentoo_runlevels (tmp, currentmode, exclusive);
     tmp[xplen-2] = 0;
     currentmode = NULL;
    }
   } else {
//    fprintf (stderr, " >> new service: %s\n", de->d_name);
    nservices = (char **) setadd ((void **)nservices, (void *)de->d_name, SET_TYPE_STRING);
   }
  }

  if (nservices) {
   if (currentmode) {
    char **arbattrs = NULL;
    struct cfgnode newnode;

    if (!exclusive) {
     uint32_t i = 0;
     char **curmodeena = str2set (':', cfg_getstring ("enable/services", currentmode));

     if (curmodeena) {
      for (; curmodeena[i]; i++) {
       if (!inset ((void **)nservices, (void *)curmodeena[i], SET_TYPE_STRING)) {
        nservices = (char **)setadd ((void **)nservices, (void *)curmodeena[i], SET_TYPE_STRING);
       }
      }
      free (curmodeena);
     }
    }

    memset (&newnode, 0, sizeof(struct cfgnode));

    arbattrs = (char **)setadd ((void **)arbattrs, (void *)"services", SET_TYPE_STRING);
    arbattrs = (char **)setadd ((void **)arbattrs, (void *)set2str (':', nservices), SET_TYPE_STRING);

    newnode.nodetype = EI_NODETYPE_CONFIG;
    newnode.mode     = currentmode;
    newnode.id       = estrdup("mode-enable");
    newnode.source   = self.rid;
    newnode.arbattrs = arbattrs;

    cfg_addnode (&newnode);
   }

   free (nservices);
  }

  closedir (dir);
 } else {
  fprintf (stderr, " >> could not open gentoo runlevels directory \"%s\": %s\n", path, strerror (errno));
 }
}

void einit_event_handler (struct einit_event *ev) {
 if (ev->type == EVE_UPDATE_CONFIGURATION) {
  struct stat st;

  init_d_dependency_scriptlet = cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/list-dependencies", NULL);
  init_d_exec_scriptlet = cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d-scriptlets/execute", NULL);

  char *cs = cfg_getstring("configuration-compatibility-sysv-distribution", NULL);
  if (is_gentoo_system || (cs && (!strcmp("gentoo", cs)) || ((!strcmp("auto", cs) && !stat("/etc/gentoo-release", &st))))) {
   if (!is_gentoo_system) {
    is_gentoo_system = 1;
    fputs (" >> gentoo system detected\n", stderr);
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
      parse_sh (data, sh_add_environ_callback);

      free (data);
     }
     ev->chain_type = EVE_CONFIGURATION_UPDATE;
    }
   }

/* runlevels */
   if (bpath = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-runlevels/path")) {
    parse_gentoo_runlevels (bpath, NULL, parse_boolean (cfg_getstring ("configuration-compatibility-sysv-distribution-gentoo-runlevels/exclusive", NULL)));
// need to add checks here for updated configuration
   }

/* service tracker */
   node = cfg_getnode ("configuration-compatibility-sysv-distribution-gentoo-service-tracker", NULL);
   if (do_service_tracking = (node && node->flag)) {
    service_tracking_path = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-service-tracker/path");
    if (!service_tracking_path) do_service_tracking = 0;
   }
  }
 } else if (ev->type == EVE_SERVICE_UPDATE) { // service tracking
  if (do_service_tracking && ev->set) {
   struct stat st;
   char tmp[256], tmpd[256], *base = NULL, *dbase = NULL,
// service is a daemon if providing module's rid begins with daemon-
        isdaemon = ev->string && (strstr (ev->string, "daemon-") == ev->string);
   uint32_t i = 0;

   if (ev->status & STATUS_OK) { // tried to do that, succeeded
    if (ev->task & MOD_ENABLE) {
     base = "started";
     dbase = "starting";
    }
   } else if (ev->status & STATUS_FAIL) { // tried to do that, but failed
    if (ev->task & MOD_ENABLE) {
     base = "failed";
     dbase = "starting";
    } else if (ev->task & MOD_DISABLE) {
     base = "started";
     dbase = "stopping";
    }
   } else { // trying to do something right now
    if (ev->task & MOD_ENABLE) {
     base = "starting";
     dbase = "failed";
    } else if (ev->task & MOD_DISABLE) {
     base = "stopping";
     dbase = "started";
    }
   }

   if (base || dbase) {
    for (; ev->set[i]; i++) {
     snprintf (tmp, 256, "%ssoftscripts/%s", service_tracking_path, ev->set[i]);
     if (lstat (tmp, &st)) {
      snprintf (tmpd, 256, "%ssoftscripts", service_tracking_path);
      if (stat (tmpd, &st)) {
       if (mkdir (tmpd, 0755)) perror (" >> could not create softscripts directory");
      }

      symlink ("/sbin/einit-control", tmp);
     }

     if (isdaemon) {
      snprintf (tmp, 256, "%sdaemons/%s", service_tracking_path, ev->set[i]);
      if (lstat (tmp, &st)) {
       snprintf (tmpd, 256, "%sdaemons", service_tracking_path);
       if (stat (tmpd, &st)) {
        if (mkdir (tmpd, 0755)) perror (" >> could not create daemons directory");
       }

       symlink ("/sbin/einit-control", tmp);
      }
     }

     if (base) {
      snprintf (tmp, 256, "%s%s/%s", service_tracking_path, base, ev->set[i]);
      if (lstat (tmp, &st)) {
       snprintf (tmpd, 256, "%s%s", service_tracking_path, base);
       if (stat (tmpd, &st)) {
        if (mkdir (tmpd, 0755)) perror (" >> could not create softscripts directory");
       }

       symlink ("/sbin/einit-control", tmp);
      }
     }

     if (dbase) {
      snprintf (tmp, 256, "%s%s/%s", service_tracking_path, dbase, ev->set[i]);
      unlink (tmp);
     }

    }
   }
  }
 } else if (ev->type == EVE_PLAN_UPDATE) { // set active "soft mode"
  if (do_service_tracking && ev->string) {
   char tmp[256];
   int slfile;

   snprintf (tmp, 256, "updating softlevel to %s\n", ev->string);
   notice (4, tmp);

   snprintf (tmp, 256, "%ssoftlevel", service_tracking_path);

   if ((slfile = open (tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) > 0) {
    write (slfile, amode->id, strlen(ev->string));
    write (slfile, "\n", 1);
    close (slfile);
   } else {
    perror (" >> creating softlevel file");
   }
  }
 }
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "examine") && !strcmp(ev->set[1], "configuration")) {
  if (!cfg_getstring("configuration-compatibility-sysv-distribution-gentoo-init.d/path", NULL)) {
   fdputs ("NOTICE: CV \"configuration-compatibility-sysv-distribution-gentoo-init.d/path\":\n  Not found: Gentoo Init Scripts will not be processed. (not a problem)\n", ev->integer);
   ev->task++;
  }

  ev->flag = 1;
 }
}

/* gentoo init.d support functions */

int cleanup_after_module (struct lmodule *this) {
#if 0
 if (this->module) {
 if (this->module->provides)
 free (this->module->provides);
 if (this->module->requires)
 free (this->module->requires);
 if (this->module->notwith)
 free (this->module->notwith);
 free (this->module);
}

 if (this->param) {
 free (this->param);
}
#endif
}

int scanmodules (struct lmodule *modchain) {
 DIR *dir;
 struct dirent *de;
 char *nrid = NULL,
 *init_d_path = cfg_getpath ("configuration-compatibility-sysv-distribution-gentoo-init.d/path"),
 *tmp = NULL;
 uint32_t plen;
 struct smodule *modinfo;

 if (!init_d_path || !init_d_dependency_scriptlet || !is_gentoo_system) return -1;

 plen = strlen (init_d_path) +1;

 if (dir = opendir (init_d_path)) {
  while (de = readdir (dir)) {
   char doop = 1;

// filter .-files
   if (de->d_name[0] == '.') continue;
// filter *.sh
   uint32_t xl = strlen(de->d_name);
   if ((xl > 3) && (de->d_name[xl-3] == '.') &&
       (de->d_name[xl-2] == 's') && (de->d_name[xl-1] == 'h')) continue;

   tmp = (char *)emalloc (plen + strlen (de->d_name));
   struct stat sbuf;
   struct lmodule *lm;
   *tmp = 0;
   strcat (tmp, init_d_path);
   strcat (tmp, de->d_name);
   if (!stat (tmp, &sbuf) && S_ISREG (sbuf.st_mode)) {
    char  tmpx[1024];

    modinfo = emalloc (sizeof (struct smodule));
    memset (modinfo, 0, sizeof(struct smodule));

    nrid = emalloc (8 + strlen(de->d_name));
    *nrid = 0;
    strcat (nrid, "gentoo-");
    strcat (nrid, de->d_name);

    snprintf (tmpx, 1024, "Gentoo-Style init.d Script (%s)", de->d_name);
    modinfo->name = estrdup (tmpx);
    modinfo->rid = estrdup(nrid);

    char *variables[5] = { "script-path", tmp,
                           "script-name", (nrid + 7), NULL};

    if (variables) {
     char *depscript = apply_variables (init_d_dependency_scriptlet, variables),
// meh, gentoo's init scripts are always parsed via bash -- should work anyway
//          *pcommand[4] = { "/bin/bash", "-c", depscript, NULL },
          pbuffer[1024];
     FILE *datapipe = popen (depscript, "r");

     while (fgets(pbuffer, 1024, datapipe)) {
      strtrim (pbuffer);
//      puts(pbuffer);
      char *val = strchr (pbuffer, '=');
      if (val) {
       *val = 0;
       val++;

       if (!strcmp (pbuffer, "need")) // need == requires
        modinfo->si.requires = str2set (' ', val);
       else if (!strcmp (pbuffer, "provide")) // provide == provides
        modinfo->si.provides = str2set (' ', val);
       else if (!strcmp (pbuffer, "before")) // before == before
        modinfo->si.before = str2set (' ', val);
       else if (!strcmp (pbuffer, "use") || !strcmp (pbuffer, "after")) { // use & after == after
        char **new = str2set (' ', val);
        modinfo->si.before = (char **)setcombine ((void **)modinfo->si.before, (void **)new, SET_TYPE_STRING);
       } else fputs (" >> gentoo init scripts: invalid input (unknown token)\n", stderr);
      }
      else fputs (" >> gentoo init scripts: invalid input\n", stderr);
     }

     pclose (datapipe);
    }

    modinfo->si.provides = (char **)setadd ((void **)modinfo->si.provides, (void *)(nrid + 7), SET_TYPE_STRING);

    struct lmodule *lm = modchain;
    while (lm) {
     if (lm->source && !strcmp(lm->source, tmp)) {
      lm->param = (void *)estrdup (tmp);
      lm->enable = (int (*)(void *, struct einit_event *))init_d_enable;
      lm->disable = (int (*)(void *, struct einit_event *))init_d_disable;
      lm->reset = (int (*)(void *, struct einit_event *))init_d_reset;
      lm->reload = (int (*)(void *, struct einit_event *))init_d_reload;
      lm->cleanup = cleanup_after_module;
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
      new->source = estrdup (tmp);
      new->param = (void *)estrdup (tmp);
      new->enable = (int (*)(void *, struct einit_event *))init_d_enable;
      new->disable = (int (*)(void *, struct einit_event *))init_d_disable;
      new->reset = (int (*)(void *, struct einit_event *))init_d_reset;
      new->reload = (int (*)(void *, struct einit_event *))init_d_reload;
      new->cleanup = cleanup_after_module;
     }
    }

   }
   if (nrid) free (nrid);
   if (tmp) free (tmp);
  }

  closedir (dir);
 } else {
  fprintf (stderr, "couldn't open init.d directory \"%s\"\n", init_d_path);
 }
}

// int __pexec_function (char *command, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **local_environment, struct einit_event *status);

int init_d_enable (char *init_script, struct einit_event *status) {
 char *variables[7] = {
  "script-path", init_script,
  "script-name", init_script,
  "action", "start", NULL },
  *cmdscript = NULL,
  *xrev = NULL;

 if (!init_script || !init_d_exec_scriptlet) return STATUS_FAIL;
 if (xrev = strrchr(init_script, '/')) variables[3] = xrev+1;

 cmdscript = apply_variables (init_d_exec_scriptlet, variables);

 return pexec (cmdscript, NULL, 0, 0, NULL, NULL, NULL, status);
}

int init_d_disable (char *init_script, struct einit_event *status) {
 char *variables[7] = {
  "script-path", init_script,
  "script-name", init_script,
  "action", "stop", NULL },
  *cmdscript = NULL,
  *xrev = NULL;

  if (!init_script || !init_d_exec_scriptlet) return STATUS_FAIL;
  if (xrev = strrchr(init_script, '/')) variables[3] = xrev+1;

  cmdscript = apply_variables (init_d_exec_scriptlet, variables);

  return pexec (cmdscript, NULL, 0, 0, NULL, NULL, NULL, status);
}

int init_d_reset (char *init_script, struct einit_event *status) {
 char *variables[7] = {
  "script-path", init_script,
  "script-name", init_script,
  "action", "restart", NULL },
  *cmdscript = NULL,
  *xrev = NULL;

  if (!init_script || !init_d_exec_scriptlet) return STATUS_FAIL;
  if (xrev = strrchr(init_script, '/')) variables[3] = xrev+1;

  cmdscript = apply_variables (init_d_exec_scriptlet, variables);

  return pexec (cmdscript, NULL, 0, 0, NULL, NULL, NULL, status);
}

int init_d_reload (char *init_script, struct einit_event *status) {
 char *variables[7] = {
  "script-path", init_script,
  "script-name", init_script,
  "action", "reload", NULL },
  *cmdscript = NULL,
  *xrev = NULL;

  if (!init_script || !init_d_exec_scriptlet) return STATUS_FAIL;
  if (xrev = strrchr(init_script, '/')) variables[3] = xrev+1;

  cmdscript = apply_variables (init_d_exec_scriptlet, variables);

  return pexec (cmdscript, NULL, 0, 0, NULL, NULL, NULL, status);
}

// no enable/disable functions: this is a passive module
