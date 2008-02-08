/*
 *  exec.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 23/11/2006.
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
#include <einit-modules/process.h>
#include <ctype.h>
#include <sys/stat.h> 

#include <einit-modules/parse-sh.h>

#ifdef POSIXREGEX
#include <regex.h>
#endif

#ifdef LINUX
#include <sys/syscall.h>
#include <linux/sched.h>
#endif

int einit_exec_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_exec_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "pexec/dexec library module",
 .rid       = "einit-exec",
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

char *safe_environment[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=dumb", NULL };

extern char shutting_down;

pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
int spawn_timeout = 5;
char kill_timeout_primary = 20, kill_timeout_secondary = 20;

char **check_variables_f (const char *, const char **, FILE *);
char *apply_envfile_f (char *command, const char **environment);
int pexec_f (const char *command, const char **variables, uid_t uid, gid_t gid, const char *user, const char *group, char **local_environment, struct einit_event *status);
int qexec_f (char *command);
int start_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status);
int stop_daemon_f (struct dexecinfo *shellcmd, struct einit_event *status);
char **create_environment_f (char **environment, const char **variables);
void einit_exec_process_event_handler (struct einit_event *);

void *dexec_watcher (pid_t pid);

int einit_exec_cleanup (struct lmodule *irr) {
 if (shell && (shell != dshell)) efree (shell);
 exec_cleanup (irr);

 function_unregister ("einit-execute-command", 1, pexec_f);
 function_unregister ("einit-execute-daemon", 1, start_daemon_f);
 function_unregister ("einit-stop-daemon", 1, stop_daemon_f);
 function_unregister ("einit-create-environment", 1, create_environment_f);
 function_unregister ("einit-check-variables", 1, check_variables_f);
 function_unregister ("einit-apply-envfile", 1, apply_envfile_f);

 function_unregister ("einit-execute-command-q", 1, qexec_f);

 event_ignore (einit_process_died, einit_exec_process_event_handler);

 sched_cleanup(irr);

 return 0;
}

void einit_exec_update_daemons_from_pidfiles() {
 emutex_lock (&running_mutex);
 struct daemonst *cur = running;

 while (cur) {
  struct dexecinfo *dx = cur->dx;

  if (dx) {
   struct stat st;
   if (dx->pidfile && !stat (dx->pidfile, &st)) {
    if (st.st_mtime > dx->pidfiles_last_update) {
     char *contents = readfile (dx->pidfile);
     if (contents) {
      pid_t daemon_pid = parse_integer (contents);

      cur->pid = daemon_pid;
      dx->pidfiles_last_update = st.st_mtime;

      efree (contents);

      if (cur->module && cur->module->module && cur->module->module->rid) {
       notice (2, "exec: modules %s updated and is now known with pid %i.", cur->module->module->rid, cur->pid);
      } else {
       notice (2, "exec: anonymous daemon updated and is now known with pid %i.", cur->pid);
      }
     }
    }
   }
  }

  cur = cur->next;
 }
 emutex_unlock (&running_mutex);
}

void einit_exec_process_event_handler (struct einit_event *ev) {
 einit_exec_update_daemons_from_pidfiles();

 dexec_watcher(ev->integer);
}

char *apply_envfile_f (char *command, const char **environment) {
 uint32_t i = 0;
 char **variables = NULL;

 if (environment) {
  for (; environment[i]; i++) {
   char *r = estrdup (environment[i]);
   char *n = strchr (r, '=');

   if (n) {
    *n = 0;
    n++;

    if (*n && !inset ((const void **)variables, r, SET_TYPE_STRING)) {
     variables = set_str_add (variables, r);
     variables = set_str_add (variables, n);
    }
   }

   efree (r);
  }
 }

 if (variables) {
  command = apply_variables (command, (const char **)variables);

#ifdef DEBUG
  write (2, command, strlen (command));
  write (2, "\n", 1);
#endif

  efree (variables);
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
   x[0] = (char *)str_stabilise (e);
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

    eregfree (&pattern);
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

  if (x[0] != e) efree (x[0]);
  efree (e);
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
   name = (char *)str_stabilise(variables[i]);
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
      efree (subkey);

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

     eregfree (&pattern);
    }

    if (pvalue)  {
     uint32_t txi = 0;
     for (; pvalue[txi]; txi++) {
      if (!isalnum (pvalue[txi]) && (pvalue[txi] != ' ')) pvalue[txi] = '_';
     }
     *(key+bkeylen-2) = 0;
     environment = straddtoenviron (environment, key, pvalue);

     efree (pvalue);
    }
    efree (key);
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

struct exec_parser_data {
 int commands;
 char **command;
 char forkflag;
};

void exec_callback (char **data, enum einit_sh_parser_pa status, struct exec_parser_data *pd) {
 switch (status) {
  case pa_end_of_file: break;

  case pa_new_context:
  case pa_new_context_fork:
   if (pd->command) {
    efree (pd->command);
   }

   pd->command = set_str_dup_stable (data);
   pd->commands++;
   pd->forkflag = (status == pa_new_context_fork);
   break;

  default: break;
 }
}

char **exec_run_sh (char *command, enum pexec_options options, char **exec_environment) {
 struct exec_parser_data pd;
 char *ocmd = (char*)str_stabilise (command);

 memset (&pd, 0, sizeof (pd));

 command = strip_empty_variables (command);

 parse_sh_ud (command, (void (*)(const char **, enum einit_sh_parser_pa, void *))exec_callback, &pd);

 if ((pd.commands == 1) && pd.command && !pd.forkflag) {
  char **r = which (pd.command[0]);

  if (r && r[0]) {
   pd.command[0] = r[0];
  }

  char *cmdtx = set2str (',', (const char **)pd.command);
  if (cmdtx) {
   efree (cmdtx);
  }

  return pd.command;
 } else {
  char **cmd;

  if (pd.command) efree (pd.command);

  cmd = set_str_dup_stable (shell);
  cmd = set_str_add_stable (cmd, ocmd);

  return cmd;
 }

 return NULL;
}

int pexec_f (const char *command, const char **variables, uid_t uid, gid_t gid, const char *user, const char *group, char **local_environment, struct einit_event *status) {
 int pipefderr [2];
 pid_t child;
 int pidstatus = 0;
 enum pexec_options options = (status ? 0 : pexec_option_nopipe);
 uint32_t cs = status_ok;
 char have_waited = 0;

 lookupuidgid (&uid, &gid, user, group);

 if (!command) return status_failed;
// if the first command is pexec-options, then set some special options
 if (strprefix (command, "pexec-options")) {
  char *ocmds = (char*)str_stabilise(command),
  *rcmds = strchr (ocmds, ';'),
  **optx = NULL;
  if (!rcmds) {
   return status_failed;
  }

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

   efree (optx);
  }
 }
 if (!command || !command[0]) {
  return status_failed;
 }

 if (strmatch (command, "true")) {
  return status_ok;
 }

 if (!(options & pexec_option_nopipe)) {
  if (pipe (pipefderr)) {
   if (status) {
    status->string = "failed to create pipe";
    status_update (status);
    status->string = strerror (errno);
   }
   return status_failed;
  }
/* make sure the read end won't survive an exec*() */
  fcntl (pipefderr[0], F_SETFD, FD_CLOEXEC);
  if (fcntl (pipefderr[0], F_SETFL, O_NONBLOCK) == -1) {
   bitch (bitch_stdio, errno, "can't set pipe (read end) to non-blocking mode!");
  }
/* make sure we unset this flag after fork()-ing */
  fcntl (pipefderr[1], F_SETFD, FD_CLOEXEC);
  if (fcntl (pipefderr[1], F_SETFL, O_NONBLOCK) == -1) {
   bitch (bitch_stdio, errno, "can't set pipe (write end) to non-blocking mode!");
  }
 }

/* if (status) {
  status->string = (char *)command;
  status_update (status);
 }*/
// notice (10, (char *)command);

 char **exec_environment;

 exec_environment = (char **)setcombine ((const void **)einit_global_environment, (const void **)local_environment, SET_TYPE_STRING);
 exec_environment = create_environment_f (exec_environment, variables);

 command = apply_envfile_f ((char *)command, (const char **)exec_environment);

 char **exvec = exec_run_sh ((char *)command, options, exec_environment);

/* efree ((void *)command);
 command = NULL;*/

#ifdef LINUX
// void *stack = emalloc (4096);
// if ((child = syscall(__NR_clone, CLONE_PTRACE | CLONE_STOPPED, stack+4096)) < 0) {

 if ((child = syscall(__NR_clone, CLONE_STOPPED | SIGCHLD, 0, NULL, NULL, NULL)) < 0) {
  if (status)
   status->string = strerror (errno);
  return status_failed;
 }
#else
 retry_fork:

 if ((child = fork()) < 0) {
  if (status)
   status->string = strerror (errno);

  goto retry_fork;

  return status_failed;
 }
#endif
 else if (child == 0) {
/* make sure einit's thread is in a proper state */
#ifndef LINUX
  sched_yield();
#endif

  nice (-einit_core_niceness_increment);
  nice (einit_task_niceness_increment);

  disable_core_dumps ();

/* cause segfault */
/*  sleep (1);
  *((char *)0) = 1;*/

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  if (!(options & pexec_option_dont_close_stdin))
   close (0);

  close (1);
  if (!(options & pexec_option_nopipe)) {
/* unset this flag after fork()-ing */
   fcntl (pipefderr[1], F_SETFD, 0);
   close (2);
   close (pipefderr [0]);
   dup2 (pipefderr [1], 1);
   dup2 (pipefderr [1], 2);
   close (pipefderr [1]);
  } else {
   dup2 (2, 1);
  }

  if (options & pexec_option_safe_environment) {
   execve (exvec[0], exvec, safe_environment);
  } else {
   execve (exvec[0], exvec, exec_environment);
  }
 } else {
  FILE *fx;

  if (exec_environment) efree (exec_environment);
  if (exvec) efree (exvec);

  if (!(options & pexec_option_nopipe) && status) {
/* tag the fd as close-on-exec, just in case */
   fcntl (pipefderr[1], F_SETFD, FD_CLOEXEC);
   close (pipefderr[1]);
   errno = 0;

   if ((fx = fdopen(pipefderr[0], "r"))) {
    char rxbuffer[BUFFERSIZE];
    setvbuf (fx, NULL, _IONBF, 0);

#ifdef LINUX
    kill (child, SIGCONT);
#endif

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

      efree (fbc);
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
  }
#ifdef LINUX
  else kill (child, SIGCONT);
#endif

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

int qexec_f (char *command) {
 size_t len;
 strtrim (command);
 char do_fork = 0;
 int pidstatus = 0;

 if (!command[0]) return status_failed;

 len = strlen (command);
 if (command[len-1] == '&') {
  command[len-1] = 0;
  do_fork = 1;
  if (!command[0]) return status_failed;
 }

 char **exvec = str2set (' ', command);
 pid_t child;

#ifdef LINUX
 if ((child = syscall(__NR_clone, CLONE_STOPPED | SIGCHLD, 0, NULL, NULL, NULL)) < 0) {
  return status_failed;
 }
#else
 retry_fork:

 if ((child = fork()) < 0) {
  goto retry_fork;

  return status_failed;
 }
#endif
 else if (child == 0) {
/* make sure einit's thread is in a proper state */
#ifndef LINUX
  sched_yield();
#endif

  nice (-einit_core_niceness_increment);
  nice (einit_task_niceness_increment);

  disable_core_dumps ();

  close (1);
  dup2 (2, 1);

  execve (exvec[0], exvec, einit_global_environment);
 } else {
#ifdef LINUX
  kill (child, SIGCONT);
#endif

  while (waitpid (child, &pidstatus, WNOHANG) != child) ;
 }

 if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS)) return status_ok;
 return status_failed;
}

void *dexec_watcher (pid_t pid) {
 struct daemonst *prev = NULL;
 struct dexecinfo *dx = NULL;
 struct lmodule *module = NULL;
 char stmp[BUFFERSIZE];

// notice (1, "trying to find out if we know about %i.", pid);

 emutex_lock (&running_mutex);
 struct daemonst *cur = running;

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

 if (dx && cur && (cur->pid == pid)) {
  char *rid = (module && module->module && module->module->rid ? module->module->rid : "unknown");
/* if we're already deactivating this daemon, resume the original function */
  if (pthread_mutex_trylock (&cur->mutex)) {
   esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: \"%s\" has died nicely, resuming.\n", rid);
   notice (8, stmp);
   emutex_unlock (&cur->mutex);
  } else if (!shutting_down && (dx->restart)) {
/* don't try to restart if the daemon died too swiftly */
/* also make sure to NOT respawn something when shutting down */
   emutex_unlock (&cur->mutex);
   if (((cur->starttime + spawn_timeout) < time(NULL))) {
    struct einit_event fb = evstaticinit(einit_feedback_module_status);
    fb.para = (void *)module;
    fb.task = einit_module_enable | einit_module_feedback_show;
    fb.status = status_working;
    fb.flag = 0;

    esprintf (stmp, BUFFERSIZE, "einit-mod-daemon: resurrecting \"%s\".\n", rid);
    fb.string = stmp;
    if (module)
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

 char *pidfile = NULL;
 if ((shellcmd->options & daemon_did_recovery) && shellcmd->pidfile && (pidfile = readfile (shellcmd->pidfile))) {
  pid_t pid = parse_integer (pidfile);

  efree (pidfile);
  pidfile = NULL;

  if (pidexists (pid)) {
   if (status) {
    fbprintf (status, "Module's PID-file already exists and is valid.");
    status_update (status);
   }

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
   new->pid = pid;

   emutex_unlock (&running_mutex);

   return status_ok;
  }
 }

/* check if needed files are available */
 if (shellcmd->need_files) {
  uint32_t r = 0;
  struct stat st;

  for (; shellcmd->need_files[r]; r++) {
   if (shellcmd->need_files[r][0] == '/') {
    if (stat (shellcmd->need_files[r], &st)) {
     notice (4, "can't bring up daemon \"%s\", because file \"%s\" does not exist.", shellcmd->id ? shellcmd->id : "unknown", shellcmd->need_files[r]);

     return status_failed;
    }
   } else {
    char **w = which (shellcmd->need_files[r]);
    if (!w) {
     notice (4, "can't bring up daemon \"%s\", because executable \"%s\" does not exist.", shellcmd->id ? shellcmd->id : "unknown", shellcmd->need_files[r]);

     return status_failed;
    } else {
     efree (w);
    }
   }
  }
 }

 if (shellcmd->pidfile) {
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd && shellcmd->script && shellcmd->script_actions && inset ((const void **)shellcmd->script_actions, "prepare", SET_TYPE_STRING)) {
  int retval;
  ssize_t nclen = strlen (shellcmd->script) + 9;
  char *ncommand = emalloc (nclen);

  esprintf (ncommand, nclen, "%s %s", shellcmd->script, "prepare");

  retval = pexec (ncommand, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);

  efree (ncommand);

  if (retval == status_failed) return status_failed;
 } else if (shellcmd->prepare) {
//  if (pexec (shellcmd->prepare, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status) == status_failed) return status_failed;
  if (pexec (shellcmd->prepare, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status) == status_failed) return status_failed;
 }

// if ((status->task & einit_module_enable) && (!shellcmd || !shellcmd->command)) return status_failed;

// if (status->task & einit_module_enable)
// else return status_ok;


 uid = shellcmd->uid;
 gid = shellcmd->gid;

 lookupuidgid (&uid, &gid, shellcmd->user, shellcmd->group);

 if (shellcmd->options & daemon_model_forking) {
  int retval;
  if (status) {
   fbprintf (status, "forking daemon");
   status_update (status);
  }

  if (shellcmd && shellcmd->script && shellcmd->script_actions && inset ((const void **)shellcmd->script_actions, "daemon", SET_TYPE_STRING)) {
   ssize_t nclen = strlen (shellcmd->script) + 8;
   char *ncommand = emalloc (nclen);

   esprintf (ncommand, nclen, "%s %s", shellcmd->script, "daemon");

   retval = pexec (ncommand, (const char **)shellcmd->variables, uid, gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);

   efree (ncommand);
  } else retval = pexec_f (shellcmd->command, (const char **)shellcmd->variables, uid, gid, shellcmd->user, shellcmd->group, shellcmd->environment, status);

  if (retval == status_ok) {
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

   shellcmd->pidfiles_last_update = 0;

   emutex_unlock (&running_mutex);

//   einit_exec_update_daemons_from_pidfiles(); /* <-- that one didn't make sense? */

   if (status) {
    fbprintf (status, "daemon started OK");
    status_update (status);
   }

   return status_ok;
  } else
   return status_failed;
 } else {
  struct daemonst *new = ecalloc (1, sizeof (struct daemonst));
  new->starttime = time (NULL);
  new->dx = shellcmd;
  if (status)
   new->module = (struct lmodule*)status->para;
  else
   new->module = NULL;
  emutex_init (&new->mutex, NULL);

  shellcmd->cb = new;

  if (status) {
   status->string = shellcmd->command;
   status_update (status);
  }

  char **daemon_environment;

  daemon_environment = (char **)setcombine ((const void **)einit_global_environment, (const void **)shellcmd->environment, SET_TYPE_STRING);
  daemon_environment = create_environment_f (daemon_environment, (const char **)shellcmd->variables);

  char *command = apply_envfile_f (shellcmd->command, (const char **)daemon_environment);

  char **exvec = exec_run_sh (command, 0, daemon_environment);

/*  efree (command);
  command = NULL;*/

#ifdef LINUX
  if ((child = syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL)) < 0) {
   if (status) {
    status->string = strerror (errno);
   }
   return status_failed;
  }
#else
  retry_fork:

  if ((child = fork()) < 0) {
   if (status) {
    status->string = strerror (errno);
   }

   goto retry_fork;
   return status_failed;
  }
#endif
  else if (child == 0) {
   nice (-einit_core_niceness_increment);
   nice (einit_task_niceness_increment);

   disable_core_dumps ();

   if (gid && (setgid (gid) == -1))
    perror ("setting gid");
   if (uid && (setuid (uid) == -1))
    perror ("setting uid");

   close (1);
   dup2 (2, 1);

   execve (exvec[0], exvec, daemon_environment);
  } else {
   if (daemon_environment) efree (daemon_environment);
   if (exvec) efree (exvec);

   new->pid = child;
//  sched_watch_pid (child, dexec_watcher);

   emutex_lock (&running_mutex);
   new->next = running;
   running = new;
   emutex_unlock (&running_mutex);

   sched_watch_pid (child);
  }

  if (shellcmd->is_up) {
   return pexec (shellcmd->is_up, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);
  }

  return status_ok;
 }
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
 einit_exec_update_daemons_from_pidfiles();

 pid_t pid = shellcmd->cb ? shellcmd->cb->pid : 0;
 if (shellcmd->cb && pidexists(pid)) {
  pthread_t th;
  pthread_mutex_trylock (&shellcmd->cb->mutex);
  shellcmd->cb->timer = kill_timeout_primary;

  if (status) {
   fbprintf (status, "sending SIGTERM, daemon has %i seconds to exit...", kill_timeout_primary);
   status_update (status);
  }
  if (kill (shellcmd->cb->pid, SIGTERM)) {
   if (status) {
    fbprintf (status, "failed to send SIGTERM: %s", strerror (errno));
    status_update (status);
   }
   notice (1, "failed to send SIGTERM to a daemon: %s; assuming it's dead.", strerror (errno));

   goto assume_dead;
  }

  ethread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
  emutex_lock (&shellcmd->cb->mutex);

  if (pidexists(pid)) { // still up?
   if (shellcmd->cb->timer <= 0) { // this means we came here because the timer ran out
    if (status) {
     status->string = "SIGTERM timer ran out, killing...";
     status_update (status);
    }

    ethread_cancel (th);
    pthread_mutex_trylock (&shellcmd->cb->mutex);
    shellcmd->cb->timer = kill_timeout_secondary;
    kill (shellcmd->cb->pid, SIGKILL);

    ethread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
    emutex_lock (&shellcmd->cb->mutex);
   }
  } else {
   if (status) {
    fbprintf (status, "daemon seems to have exited gracefully.");
    status_update (status);
   }
  }
  ethread_cancel (th);

  emutex_unlock (&shellcmd->cb->mutex); // just in case
  emutex_destroy (&shellcmd->cb->mutex);
 } else {
  if (status) {
   fbprintf (status, "No control block or PID invalid, skipping the killing.");
   status_update (status);
  }
 }

 assume_dead:

 shellcmd->cb = NULL;

 if (shellcmd->pidfile) {
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd && shellcmd->script && shellcmd->script_actions && inset ((const void **)shellcmd->script_actions, "cleanup", SET_TYPE_STRING)) {
  int retval;
  ssize_t nclen = strlen (shellcmd->script) + 9;
  char *ncommand = emalloc (nclen);

  esprintf (ncommand, nclen, "%s %s", shellcmd->script, "cleanup");

  retval = pexec (ncommand, (const char **)shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status);

  efree (ncommand);

  if (retval == status_failed) return status_ok;
 } else if (shellcmd->cleanup) {
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

 event_listen (einit_process_died, einit_exec_process_event_handler);

 function_register ("einit-execute-command", 1, pexec_f);
 function_register ("einit-execute-daemon", 1, start_daemon_f);
 function_register ("einit-stop-daemon", 1, stop_daemon_f);
 function_register ("einit-create-environment", 1, create_environment_f);
 function_register ("einit-check-variables", 1, check_variables_f);
 function_register ("einit-apply-envfile", 1, apply_envfile_f);

 function_register ("einit-execute-command-q", 1, qexec_f);

 return 0;
}
