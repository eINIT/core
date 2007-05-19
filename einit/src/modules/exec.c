/*
 *  exec.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 23/11/2006.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <einit-modules/exec.h>
#include <einit-modules/scheduler.h>
#include <ctype.h>
#include <sys/stat.h> 

#ifdef POSIXREGEX
#include <regex.h>
#endif

int einit_exec_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_exec_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "pexec/dexec library module",
 .rid       = "exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_exec_configure
};

module_register(einit_exec_self);

#endif

// char hasslash = strchr(key, '/') ? 1 : 0;

/* variables */
struct daemonst * running = NULL;

char **shell = NULL;
char *dshell[] = {"/bin/sh", "-c", NULL};
char *envfilebase = "/etc/econf.d/";

char *safe_environment[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=dumb", NULL };

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
struct execst * pexec_running = NULL;
pthread_mutex_t pexec_running_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
int spawn_timeout = 5;
char kill_timeout_primary = 20, kill_timeout_secondary = 20;

char **check_variables_f (const char *, const char **, FILE *);
char *apply_envfile_f (char *command, const char **environment);
int pexec_f (const char *command, const char **variables, uid_t uid, gid_t gid, const char *user, const char *group, char **local_environment, struct einit_event *status);
int start_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status);
int stop_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status);
char **create_environment_f (char **environment, const char **variables);
void einit_exec_ipc_event_handler (struct einit_event *);
void einit_exec_einit_event_handler (struct einit_event *);

int einit_exec_cleanup (struct lmodule *irr) {
 if (shell && (shell != dshell)) free (shell);
 exec_cleanup (irr);

 function_unregister ("einit-execute-command", 1, pexec_f);
 function_unregister ("einit-execute-daemon", 1, start_daemon_f);
 function_unregister ("einit-stop-daemon", 1, stop_daemon_f);
 function_unregister ("einit-create-environment", 1, create_environment_f);
 function_unregister ("einit-check-variables", 1, check_variables_f);
 function_unregister ("einit-apply-envfile", 1, apply_envfile_f);

 event_ignore (einit_event_subsystem_ipc, einit_exec_ipc_event_handler);
 event_ignore (einit_event_subsystem_core, einit_exec_einit_event_handler);

 sched_cleanup(irr);

 return 0;
}

void einit_exec_einit_event_handler (struct einit_event *ev) {
 if (ev->type == einit_core_configuration_update) {
  if (!(envfilebase = cfg_getpath("configuration-system-exec-envfile-base")))
   envfilebase = estrdup("/etc/econf.d/");
 }
}

void einit_exec_ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->argv && ev->argv[0] && ev->argv[1] && strmatch(ev->argv[0], "exec")) {
  struct einit_event ee = evstaticinit (einit_feedback_module_status);
  ee.para = (void *)thismodule;

  pexec_f (ev->command, NULL, 0, 0, NULL, NULL, NULL, &ee);
  evstaticdestroy(ee);

  ev->implemented = 1;
 }
}

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
void *pexec_watcher (struct spidcb *spid) {
 pid_t pid = spid->pid;
 int status = spid->status;
 emutex_lock (&pexec_running_mutex);
 struct execst *cur = pexec_running;
 while (cur) {
  if (cur->pid == pid) {
/* save status and resume pexec() */
   cur->status = status;
   emutex_unlock (&pexec_running_mutex);
   emutex_unlock (&(cur->mutex);
   return NULL;
  }
  cur = cur->next;
 }
 emutex_unlock (&pexec_running_mutex);
}
#endif

char *apply_envfile_f (char *command, const char **environment) {
 uint32_t i = 0;

 char **envfiles = NULL;

 if (environment) {
  for (; environment[i]; i++) {
   if (strstr(environment[i], "envfile=") == environment[i]) {
    if (envfiles) free (envfiles);

    envfiles = str2set (':', environment[i]+8);
    break;
   }
   if (strstr(environment[i], "services=") == environment[i]) {
    envfiles = str2set (':', environment[i]+9);
   }
  }
 }

 if (envfiles) {
  for (i = 0; envfiles[i]; i++) {
   char buffer[BUFFERSIZE];
   char *file = NULL;
   struct stat st;

   if (envfiles[i][0] == '/')
    file = envfiles[i];
   else {
    file = emalloc (strlen (envfilebase)+strlen(envfiles[i]) + 1);
    *file = 0;
    strcat (file, envfilebase);
    strcat (file, envfiles[i]);
   }

   if (file && !stat (file, &st)) {
    char *oc = command;

    esprintf (buffer, BUFFERSIZE, " . %s;\n", file);

    command = emalloc (strlen(buffer) + strlen (oc) + 1);
    *command = 0;
    strcat (command, buffer);
    strcat (command, oc);
   }
  }
 }

 return command;
}

char **check_variables_f (const char *id, const char **variables, FILE *output) {
 uint32_t u = 0;
 if (!variables) return (char **)variables;
 for (u = 0; variables[u]; u++) {
  char *e = estrdup (variables[u]), *ep = strchr (e, '/');
  char *x[] = { e, NULL, NULL };
  char node_found = 1;
  uint32_t variable_matches = 0;

  if (ep) {
   *ep = 0;
   x[0] = estrdup (e);
   *ep = '/';

   ep++;
   x[1] = ep;
  }

#ifndef POSIXREGEX
  if (!cfg_getnode (x[0], NULL)) {
   node_found = 0;
  } else if (cfg_getstring (e, NULL)) {
   variable_matches++;
  }
#else
  struct cfgnode *n;

  if (!(n = cfg_getnode (x[0], NULL))) {
   node_found = 0;
  } else if (x[1] && n->arbattrs) {
   regex_t pattern;
   if (!eregcomp(&pattern, x[1])) {
    uint32_t v = 0;
    for (v = 0; n->arbattrs[v]; v+=2) {
     if (!regexec (&pattern, n->arbattrs[v], 0, NULL, 0)) {
      variable_matches++;
     }
    }

    regfree (&pattern);
   }
  } else if (cfg_getstring (x[0], NULL)) {
   variable_matches++;
  }
#endif

  if (!node_found) {
   eprintf (output, " * module: %s: undefined node: %s\n", id, x[0]);
  } else if (!variable_matches) {
   eprintf (output, " * module: %s: undefined variable: %s\n", id, e);
  }

  if (x[0] != e) free (x[0]);
  free (e);
 }

 return (char **)variables;
}

char **create_environment_f (char **environment, const char **variables) {
 int i = 0;
 char *variablevalue = NULL;
 if (variables) for (i = 0; variables[i]; i++) {
#ifdef POSIXREGEX
  if ((variablevalue = strchr (variables[i], '/'))) {
/* special treatment if we have an attribue specifier in the variable name */
   char *name = NULL, *filter = variablevalue+1;
   struct cfgnode *node;
   *variablevalue = 0;
   name = estrdup(variables[i]);
   *variablevalue = '/';

   if ((node = cfg_getnode (name, NULL)) && node->arbattrs) {
    size_t bkeylen = strlen (name)+2, pvlen = 1;
    char *key = emalloc (bkeylen);
    char *pvalue = NULL;
    regex_t pattern;

    if (!eregcomp(&pattern, filter)) {
     int y = 0;
     *key = 0;
     strcat (key, name);
     *(key+bkeylen-2) = '/';
     *(key+bkeylen-1) = 0;

     for (y = 0; node->arbattrs[y]; y+=2) if (!regexec (&pattern, node->arbattrs[y], 0, NULL, 0)) {
      size_t attrlen = strlen (node->arbattrs[y])+1;
      char *subkey = emalloc (bkeylen+attrlen);
      *subkey = 0;
      strcat (subkey, key);
      strcat (subkey, node->arbattrs[y]);
      environment = straddtoenviron (environment, subkey, node->arbattrs[y+1]);
      free (subkey);

      if (pvalue) {
       pvalue=erealloc (pvalue, pvlen+attrlen);
       *(pvalue+pvlen-2) = ' ';
       *(pvalue+pvlen-1) = 0;
       strcat (pvalue, node->arbattrs[y]);
       pvlen += attrlen;
      } else {
       pvalue=emalloc (pvlen+attrlen);
       *pvalue = 0;
       strcat (pvalue, node->arbattrs[y]);
       pvlen += attrlen;
      }
     }

     regfree (&pattern);
    }

    if (pvalue)  {
     uint32_t txi = 0;
     for (; pvalue[txi]; txi++) {
      if (!isalnum (pvalue[txi]) && (pvalue[txi] != ' ')) pvalue[txi] = '_';
     }
     *(key+bkeylen-2) = 0;
     environment = straddtoenviron (environment, key, pvalue);

     free (pvalue);
    }
    free (key);
    free (name);
   }
  } else {
/* else: just add it */
   char *variablevalue = cfg_getstring (variables[i], NULL);
   if (variablevalue)
    environment = straddtoenviron (environment, variables[i], variablevalue);
  }
#else
  char *variablevalue = cfg_getstring (variables[i], NULL);
  if (variablevalue)
   environment = straddtoenviron (environment, variables[i], variablevalue);
#endif
 }

/*  if (variables) {
   int i = 0;
   for (; variables [i]; i++) {
    char *variablevalue = cfg_getstring (variables [i], NULL);
    if (variablevalue) {
     exec_environment = straddtoenviron (exec_environment, variables [i], variablevalue);
    }
   }
  }*/

 return environment;
}

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
// less features and verbosity in this one -- we only want it to work *somehow*

int pexec_f (const char *command, const char **variables, uid_t uid, gid_t gid, const char *user, const char *group, char **local_environment, struct einit_event *status) {
 pid_t child;
 int pidstatus = 0;
 char **cmd, **cmdsetdup;
 enum pexec_options options = (status ? 0 : pexec_option_nopipe);

 lookupuidgid (&uid, &gid, user, group);

 if (!command) return status_failed;
// if the first command is pexec-options, then set some special options
 if (strstr (command, "pexec-options") == command) {
  char *ocmds = command,
       *rcmds = strchr (ocmds, ';'),
       **optx = NULL;
  if (!rcmds) return status_failed;

  *rcmds = 0;
  optx = str2set (' ', ocmds);
  *rcmds = ';';

  command = rcmds+1;

  if (optx) {
   unsigned int x = 0;
   for (; optx[x]; x++) {
    if (strmatch (optx[x], "no-pipe")) {
     options |= pexec_option_nopipe;
    } else if (strmatch (optx[x], "safe-environment")) {
     options |= pexec_option_safe_environment;
    } else if (strmatch (optx[x], "dont-close-stdin")) {
     options |= pexec_option_dont_close_stdin;
    }
   }

   free (optx);
  }
 }
 if (!command || !command[0]) return status_failed;

 struct execst *new = ecalloc (1, sizeof (struct execst));
 emutex_init (&(new->mutex), NULL);
 emutex_lock (&(new->mutex));
 emutex_lock (&pexec_running_mutex);
 if (!pexec_running) pexec_running = new;
 else {
  new->next = pexec_running;
  pexec_running = new;
 }
 emutex_unlock (&pexec_running_mutex);

/* if (status) {
  status->string = command;
  status_update (status);
 }*/
 notice (10, (char *)command);

 if ((child = fork()) < 0) {
  if (status)
   status->string = strerror (errno);
  emutex_unlock (&(new->mutex));
  emutex_destroy (&(new->mutex));
  if (new == pexec_running) {
   pexec_running = new->next;
  } else {
   struct execst *cur = pexec_running;
   while (cur) {
    if (cur->next == new) {
     cur->next = new->next;
    }
    cur = cur->next;
   }
  }
  free (new);
  return status_failed;
 } else if (child == 0) {
  char **exec_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  eclose (1);

  dup2 (2, 1);
// we can safely play with the global environment here, since we fork()-ed earlier
  exec_environment = (char **)setcombine ((const void **)einit_global_environment, (const void **)local_environment, SET_TYPE_STRING);
  exec_environment = create_environment_f (exec_environment, variables);

  command = apply_envfile_f ((char *)command, (const char **)exec_environment);

  cmdsetdup = str2set ('\0', command);
  cmd = (char **)setcombine ((const void **)shell, (const void **)cmdsetdup, -1);

  if (options & pexec_option_safe_environment) {
   debugx(" >> \"%s\": NOT using environment {%s}, but {%s} instead.\n", set2str(':', cmd), set2str(':', exec_environment), set2str(':', safe_environment));
   execve (cmd[0], cmd, safe_environment);
  } else {
   execve (cmd[0], cmd, exec_environment);
  }
  perror (cmd[0]);
  free (cmd);
  free (cmdsetdup);
  exit (EXIT_FAILURE);
 } else {
  ssize_t br;
  ssize_t ic = 0;
  ssize_t i;
  new->pid = child;
// this loop can be used to create a race-condition
  sched_watch_pid (child, pexec_watcher);

  char buf[BUFFERSIZE+1];
  char lbuf[BUFFERSIZE+1];

  emutex_lock (&(new->mutex));
  emutex_unlock (&(new->mutex));
  emutex_destroy (&(new->mutex));
  pidstatus = new->status;

  emutex_lock (&pexec_running_mutex);
  if (new == pexec_running) {
   pexec_running = new->next;
  } else {
   struct execst *cur = pexec_running;
   while (cur) {
    if (cur->next == new) {
     cur->next = new->next;
    }
    cur = cur->next;
   }
  }
  emutex_unlock (&pexec_running_mutex);
  free (new);
 }

 if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS)) return status_ok;
 return status_failed;
}

#else

int pexec_f (const char *command, const char **variables, uid_t uid, gid_t gid, const char *user, const char *group, char **local_environment, struct einit_event *status) {
 int pipefderr [2];
 pid_t child;
 int pidstatus = 0;
 char **cmd, **cmdsetdup;
 enum pexec_options options = (status ? 0 : pexec_option_nopipe);
 uint32_t cs = status_ok;
 char have_waited = 0;

 lookupuidgid (&uid, &gid, user, group);

 if (!command) return status_failed;
// if the first command is pexec-options, then set some special options
 if (strstr (command, "pexec-options") == command) {
  char *ocmds = estrdup(command),
  *rcmds = strchr (ocmds, ';'),
  **optx = NULL;
  if (!rcmds) return status_failed;

  *rcmds = 0;
  optx = str2set (' ', ocmds);
  *rcmds = ';';

  command = rcmds+1;

  if (optx) {
   unsigned int x = 0;
   for (; optx[x]; x++) {
    if (strmatch (optx[x], "no-pipe")) {
     options |= pexec_option_nopipe;
    } else if (strmatch (optx[x], "safe-environment")) {
     options |= pexec_option_safe_environment;
    } else if (strmatch (optx[x], "dont-close-stdin")) {
     options |= pexec_option_dont_close_stdin;
    }
   }

   free (optx);
  }
 }
 if (!command || !command[0]) return status_failed;

 if (!(options & pexec_option_nopipe)) {
  if (pipe (pipefderr)) {
   if (status) {
    status->string = "failed to create pipe";
    status_update (status);
    status->string = strerror (errno);
   }
   return status_failed;
  }
  if (fcntl (pipefderr[0], F_SETFL, O_NONBLOCK) == -1) {
   bitch (bitch_stdio, errno, "can't set pipe (read end) to non-blocking mode!");
  }
  if (fcntl (pipefderr[1], F_SETFL, O_NONBLOCK) == -1) {
   bitch (bitch_stdio, errno, "can't set pipe (write end) to non-blocking mode!");
  }
 }

/* if (status) {
  status->string = (char *)command;
  status_update (status);
 }*/
// notice (10, (char *)command);

 if ((child = fork()) < 0) {
  if (status)
   status->string = strerror (errno);
  return status_failed;
 } else if (child == 0) {
/* cause segfault */
/*  sleep (1);
  *((char *)0) = 1;*/

/* make sure einit's thread is in a proper state */
  sched_yield();

  char **exec_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  if (!(options & pexec_option_dont_close_stdin))
   eclose (0);

  eclose (1);
  if (!(options & pexec_option_nopipe)) {
   eclose (2);
   eclose (pipefderr [0]);
   dup2 (pipefderr [1], 1);
   dup2 (pipefderr [1], 2);
  } else {
   dup2 (2, 1);
  }

// we can safely play with the global environment here, since we fork()-ed earlier
  exec_environment = (char **)setcombine ((const void **)einit_global_environment, (const void **)local_environment, SET_TYPE_STRING);
  exec_environment = create_environment_f (exec_environment, variables);

  command = apply_envfile_f ((char *)command, (const char **)exec_environment);

  cmdsetdup = str2set ('\0', command);
  cmd = (char **)setcombine ((const void **)shell, (const void **)cmdsetdup, -1);

  if (options & pexec_option_safe_environment) {
   execve (cmd[0], cmd, safe_environment);
  } else {
   execve (cmd[0], cmd, exec_environment);
  }
  perror (cmd[0]);
  free (cmd);
  free (cmdsetdup);
  exit (EXIT_FAILURE);
 } else {
  FILE *fx;

  if (!(options & pexec_option_nopipe) && status) {
   eclose (pipefderr[1]);
   errno = 0;

   if ((fx = fdopen(pipefderr[0], "r"))) {
    char rxbuffer[BUFFERSIZE];
    setvbuf (fx, NULL, _IONBF, 0);

    if ((waitpid (child, &pidstatus, WNOHANG) == child) &&
        (WIFEXITED(pidstatus) || WIFSIGNALED(pidstatus))) {
     have_waited = 1;
    } else while (!feof(fx)) {
     if (!fgets(rxbuffer, BUFFERSIZE, fx)) {
      if (errno == EAGAIN) {
       usleep(100);
       goto skip_read;
      }
      break;
     }

     char **fbc = str2set ('|', rxbuffer), orest = 1;
     strtrim (rxbuffer);

     if (fbc) {
      if (strmatch (fbc[0], "feedback")) {
// suppose things are going fine until proven otherwise
       cs = status_ok;

       if (strmatch (fbc[1], "notice")) {
        orest = 0;
        status->string = fbc[2];
        status_update (status);
       } else if (strmatch (fbc[1], "warning")) {
        orest = 0;
        status->string = fbc[2];
        status->flag++;
        status_update (status);
       } else if (strmatch (fbc[1], "success")) {
        orest = 0;
        cs = status_ok;
        status->string = fbc[2];
        status_update (status);
       } else if (strmatch (fbc[1], "failure")) {
        orest = 0;
        cs = status_failed;
        status->string = fbc[2];
        status->flag++;
        status_update (status);
       }
      }

      free (fbc);
     }

     if (orest) {
      status->string = rxbuffer;
      status_update (status);
     }

     continue;

     skip_read:

     if (waitpid (child, &pidstatus, WNOHANG) == child) {
      if (WIFEXITED(pidstatus) || WIFSIGNALED(pidstatus)) {
       have_waited = 1;
       break;
      }
     }
    }

    efclose (fx);
   } else {
    perror ("pexec(): open pipe");
   }
  }/* else if (status) {
   status->string = "NOT piping, check stderr for program output";
   status_update (status);
  }*/

  if (!have_waited) {
   do {
    waitpid (child, &pidstatus, 0);
   } while (!WIFEXITED(pidstatus) && !WIFSIGNALED(pidstatus));
  }
 }

 if (cs == status_failed) return status_failed;
 if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS)) return status_ok;
 return status_failed;
}
#endif

void *dexec_watcher (struct spidcb *spid) {
 pid_t pid = spid->pid;
 struct daemonst *prev = NULL;
 struct dexecinfo *dx = NULL;
 struct lmodule *module = NULL;
 emutex_lock (&running_mutex);
 struct daemonst *cur = running;
 char stmp[BUFFERSIZE];

 while (cur) {
  dx = cur->dx;
  if (cur->pid == pid) {
/* check whether to restart, and do so if the answer is yes... */
   module = cur->module;
   if (prev != NULL) {
    prev->next = cur->next;
   } else {
    running = cur->next;
   }

   break;
  }
  prev = cur;
  cur = cur->next;
 }
 emutex_unlock (&running_mutex);

 if (dx) {
  char *rid = (module && module->module && module->module->rid ? module->module->rid : "unknown");
/* if we're already deactivating this daemon, resume the original function */
  if (pthread_mutex_trylock (&cur->mutex)) {
   esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: \"%s\" has died nicely, resuming.\n", rid);
   notice (8, stmp);
   emutex_unlock (&cur->mutex);
  } else if (dx->restart) {
/* don't try to restart if the daemon died too swiftly */
   emutex_unlock (&cur->mutex);
   if (((cur->starttime + spawn_timeout) < time(NULL))) {
    struct einit_event fb = evstaticinit(einit_feedback_module_status);
    fb.para = (void *)module;
    fb.task = einit_module_enable | einit_module_feedback_show;
    fb.status = status_working;
    fb.flag = 0;

    esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: resurrecting \"%s\".\n", rid);
    fb.string = stmp;
    fb.integer = module->fbseq+1;
    status_update ((&fb));

    dx->cb = NULL;
    start_daemon_f (dx, &fb);
   } else {
    dx->cb = NULL;
    esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: \"%s\" has died too swiftly, considering defunct.\n", rid);
    notice (5, stmp);
    if (module)
     mod (einit_module_disable, module, NULL);
   }
  } else {
   emutex_unlock (&cur->mutex);
   dx->cb = NULL;
   esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: \"%s\" has died, but does not wish to be restarted.\n", rid);
   notice (5, stmp);
   if (module)
    mod (einit_module_disable, module, NULL);
  }
 }

 return NULL;
}

int start_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status) {
 pid_t child;
 uid_t uid;
 gid_t gid;
// char *cmddup;

 if (!shellcmd) return status_failed;

/* check if needed files are available */
 if (shellcmd->need_files) {
  uint32_t r = 0;
  struct stat st;

  for (; shellcmd->need_files[r]; r++) {
   if (stat (shellcmd->need_files[r], &st)) {
    notice (4, "can't bring up daemon \"%s\", because file \"%s\" does not exist.", shellcmd->id ? shellcmd->id : "unknown", shellcmd->need_files[r]);

    return status_failed;
   }
  }
 }

 if (shellcmd->pidfile) {
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd->prepare) {
//  if (pexec (shellcmd->prepare, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status) == status_failed) return status_failed;
  if (pexec (shellcmd->prepare, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status) == status_failed) return status_failed;
 }

// if ((status->task & einit_module_enable) && (!shellcmd || !shellcmd->command)) return status_failed;

// if (status->task & einit_module_enable)
// else return status_ok;

// cmddup = estrdup (shellcmd->command);

 uid = shellcmd->uid;
 gid = shellcmd->gid;

 lookupuidgid (&uid, &gid, shellcmd->user, shellcmd->group);

 struct daemonst *new = ecalloc (1, sizeof (struct daemonst));
 new->starttime = time (NULL);
 new->dx = shellcmd;
 if (status)
  new->module = (struct lmodule*)status->para;
 else
  new->module = NULL;
 emutex_init (&new->mutex, NULL);
 emutex_lock (&running_mutex);
 new->next = running;
 running = new;

 shellcmd->cb = new;

 if (status) {
  status->string = shellcmd->command;
  status_update (status);
 }

 if ((child = fork()) < 0) {
  if (status) {
   status->string = strerror (errno);
  }
  return status_failed;
 } else if (child == 0) {
  char **cmd;
  char **cmdsetdup;
  char **daemon_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  daemon_environment = (char **)setcombine ((const void **)einit_global_environment, (const void **)shellcmd->environment, SET_TYPE_STRING);
  daemon_environment = create_environment_f (daemon_environment, (const char **)shellcmd->variables);

  shellcmd->command = apply_envfile_f (shellcmd->command, (const char **)daemon_environment);

  cmdsetdup = str2set ('\0', shellcmd->command);
  cmd = (char **)setcombine ((const void **)shell, (const void **)cmdsetdup, 0);
//  close (0);
//  close (1);
//  close (2);
  eclose (1);
  dup2 (2, 1);

  execve (cmd[0], cmd, daemon_environment);
  free (cmd);
  free (cmdsetdup);
  exit (EXIT_FAILURE);
 } else {
  new->pid = child;
  sched_watch_pid (child, dexec_watcher);
 }
 emutex_unlock (&running_mutex);

 if (shellcmd->is_up) {
  return pexec (shellcmd->is_up, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
 }

 return status_ok;
}

void dexec_resume_timer (struct dexecinfo *dx) {
 time_t timer = ((dx && dx->cb) ? dx->cb->timer : 1);
 while (dx && dx->cb && (timer = sleep(timer)));

 if (dx && dx->cb) {
  dx->cb->timer = timer;
  pthread_mutex_trylock (&dx->cb->mutex); // make sure the thing is locked

  emutex_unlock (&dx->cb->mutex);  // unlock it
 }
}

int stop_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status) {
 if (shellcmd->cb) {
  pthread_t th;
  pthread_mutex_trylock (&shellcmd->cb->mutex);
  shellcmd->cb->timer = kill_timeout_primary;
  kill (shellcmd->cb->pid, SIGTERM);

  ethread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
  emutex_lock (&shellcmd->cb->mutex);

  if (shellcmd->cb->timer <= 0) { // this means we came here because the timer ran out
   status->string = "SIGTERM timer ran out, killing...";
   status_update (status);

   ethread_cancel (th);
   pthread_mutex_trylock (&shellcmd->cb->mutex);
   shellcmd->cb->timer = kill_timeout_secondary;
   kill (shellcmd->cb->pid, SIGKILL);

   ethread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
   emutex_lock (&shellcmd->cb->mutex);
  }
  ethread_cancel (th);

  emutex_unlock (&shellcmd->cb->mutex); // just in case
  emutex_destroy (&shellcmd->cb->mutex);
 }

 shellcmd->cb = NULL;

 if (shellcmd->pidfile) {
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd->cleanup) {
 // if (pexec (shellcmd->cleanup, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status) == status_failed) return status_ok;
  if (pexec (shellcmd->cleanup, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status) == status_failed) return status_ok;
 }

 if (shellcmd->is_down) {
  return pexec (shellcmd->is_down, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
 }

 return status_ok;
}

int einit_exec_configure (struct lmodule *irr) {
 module_init(irr);

 sched_configure(irr);

 irr->cleanup = einit_exec_cleanup;

 struct cfgnode *node;
 if (!(shell = (char **)str2set (' ', cfg_getstring ("configuration-system-shell", NULL))))
  shell = dshell;
 exec_configure (irr);

 if ((node = cfg_findnode ("configuration-system-daemon-spawn-timeout", 0, NULL)))
  spawn_timeout = node->value;
 if ((node = cfg_findnode ("configuration-system-daemon-term-timeout-primary", 0, NULL)))
  kill_timeout_primary = node->value;
 if ((node = cfg_findnode ("configuration-system-daemon-term-timeout-secondary", 0, NULL)))
  kill_timeout_secondary = node->value;

 event_listen (einit_event_subsystem_ipc, einit_exec_ipc_event_handler);
 event_listen (einit_event_subsystem_core, einit_exec_einit_event_handler);

 function_register ("einit-execute-command", 1, pexec_f);
 function_register ("einit-execute-daemon", 1, start_daemon_f);
 function_register ("einit-stop-daemon", 1, stop_daemon_f);
 function_register ("einit-create-environment", 1, create_environment_f);
 function_register ("einit-check-variables", 1, check_variables_f);
 function_register ("einit-apply-envfile", 1, apply_envfile_f);

 return 0;
}
