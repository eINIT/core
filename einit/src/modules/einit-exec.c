/*
 *  einit-exec.c
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
#include <einit/scheduler.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <einit-modules/exec.h>
#include <ctype.h>

#ifdef POSIXREGEX
#include <regex.h>
#endif

const struct smodule self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .options   = 0,
 .name      = "pexec/dexec library module",
 .rid       = "einit-exec",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 }
};

// char hasslash = strchr(key, '/') ? 1 : 0;

/* variables */
struct daemonst * running = NULL;

char **shell = NULL;
char *dshell[] = {"/bin/sh", "-c", NULL};

char *safe_environment[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "TERM=dumb", NULL };

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
struct execst * pexec_running = NULL;
pthread_mutex_t pexec_running_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
int spawn_timeout = 5;
char kill_timeout_primary = 20, kill_timeout_secondary = 20;

int __pexec_function (char *command, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **local_environment, struct einit_event *status);
int __start_daemon_function (struct dexecinfo *shellcmd, struct einit_event *status);
int __stop_daemon_function (struct dexecinfo *shellcmd, struct einit_event *status);
char **__create_environment (char **environment, char **variables);
void ipc_event_handler (struct einit_event *);
struct lmodule *me;

int configure (struct lmodule *irr) {
 struct cfgnode *node;
 me = irr;
 if (!(shell = (char **)str2set (' ', cfg_getstring ("configuration-system-shell", NULL))))
  shell = dshell;
 exec_configure (irr);

 if (node = cfg_findnode ("configuration-system-daemon-spawn-timeout", 0, NULL))
  spawn_timeout = node->value;
 if (node = cfg_findnode ("configuration-system-daemon-term-timeout-primary", 0, NULL))
  kill_timeout_primary = node->value;
 if (node = cfg_findnode ("configuration-system-daemon-term-timeout-secondary", 0, NULL))
  kill_timeout_secondary = node->value;

 event_listen (EVENT_SUBSYSTEM_IPC, ipc_event_handler);

 function_register ("einit-execute-command", 1, __pexec_function);
 function_register ("einit-execute-daemon", 1, __start_daemon_function);
 function_register ("einit-stop-daemon", 1, __stop_daemon_function);
 function_register ("einit-create-environment", 1, __create_environment);
}

int cleanup (struct lmodule *irr) {
 if (shell && (shell != dshell)) free (shell);
 exec_cleanup (irr);

 function_unregister ("einit-execute-command", 1, __pexec_function);
 function_unregister ("einit-execute-daemon", 1, __start_daemon_function);
 function_unregister ("einit-stop-daemon", 1, __stop_daemon_function);
 function_unregister ("einit-create-environment", 1, __create_environment);

 event_ignore (EVENT_SUBSYSTEM_IPC, ipc_event_handler);
}

void ipc_event_handler (struct einit_event *ev) {
 if (ev && ev->set && ev->set[0] && ev->set[1] && !strcmp(ev->set[0], "exec")) {
  struct einit_event ee = evstaticinit (EVE_FEEDBACK_MODULE_STATUS);
  ev->flag = 1;
  ee.para = (void *)me;

  __pexec_function (ev->string, NULL, 0, 0, NULL, NULL, NULL, &ee);
  evstaticdestroy(ee);
 }
}

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
void *pexec_watcher (struct spidcb *spid) {
 pid_t pid = spid->pid;
 int status = spid->status;
// fprintf (stderr, "%i: called\n", (pid_t)pid);
 pthread_mutex_lock (&pexec_running_mutex);
 struct execst *cur = pexec_running;
// puts ("callback()");
 while (cur) {
  if (cur->pid == pid) {
/* save status and resume pexec() */
   cur->status = status;
   pthread_mutex_unlock (&pexec_running_mutex);
   pthread_mutex_unlock (&(cur->mutex));
//   fprintf (stderr, "%i: resuming\n", (pid_t)pid);
   return NULL;
  }
  cur = cur->next;
 }
 pthread_mutex_unlock (&pexec_running_mutex);
}
#endif

char **__create_environment (char **environment, char **variables) {
 int i = 0;
 char *variablevalue = NULL;
 if (variables) for (i = 0; variables[i]; i++) {
#ifdef POSIXREGEX
  if (variablevalue = strchr (variables[i], '/')) {
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
    uint32_t err = regcomp (&pattern, filter, REG_EXTENDED);

    if (err) {
     char errorcode [1024];
     regerror (err, &pattern, errorcode, 1024);
     fputs (errorcode, stderr);
    } else {
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
//      *(subkey+bkeylen+attrlen-1) = 0;
//      printf ("%s:%s:%s:%i:%i\n", subkey, key, node->arbattrs[y], attrlen, bkeylen + attrlen);
//      puts (node->arbattrs[y]);
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
     uint32_t len = strlen (pvalue), i = 0;
     for (; pvalue[i]; i++) {
      if (!isalnum (pvalue[i]) && (pvalue[i] != ' ')) pvalue[i] = '_';
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

int __pexec_function (char *command, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **local_environment, struct einit_event *status) {
 pid_t child;
 int pidstatus = 0;
 char **cmd, **cmdsetdup, options = (status ? 0 : PEXEC_OPTION_NOPIPE);

 lookupuidgid (&uid, &gid, user, group);

 if (!command) return STATUS_FAIL;
// if the first command is pexec-options, then set some special options
 if (strstr (command, "pexec-options") == command) {
  char *ocmds = command,
       *rcmds = strchr (ocmds, ';'),
       **optx = NULL;
  if (!rcmds) return STATUS_FAIL;

  *rcmds = 0;
  optx = str2set (' ', ocmds);
  *rcmds = ';';

  command = rcmds+1;

  if (optx) {
   unsigned int x = 0;
   for (; optx[x]; x++) {
    if (!strcmp (optx[x], "no-pipe")) {
     options |= PEXEC_OPTION_NOPIPE;
    } else if (!strcmp (optx[x], "safe-environment")) {
     options |= PEXEC_OPTION_SAFEENVIRONMENT;
    } else if (!strcmp (optx[x], "dont-close-stdin")) {
     options |= PEXEC_OPTION_DONTCLOSESTDIN;
    }
   }

   free (optx);
  }
 }
 if (!command || !command[0]) return STATUS_FAIL;

 cmdsetdup = str2set ('\0', command);
 cmd = (char **)setcombine ((void *)shell, (void **)cmdsetdup, -1);

 struct execst *new = ecalloc (1, sizeof (struct execst));
 pthread_mutex_init (&(new->mutex), NULL);
 pthread_mutex_lock (&(new->mutex));
 pthread_mutex_lock (&pexec_running_mutex);
 if (!pexec_running) pexec_running = new;
 else {
  new->next = pexec_running;
  pexec_running = new;
 }
 pthread_mutex_unlock (&pexec_running_mutex);

 if (status) {
  status->string = command;
  status_update (status);
 }

 if ((child = fork()) < 0) {
  if (status)
   status->string = strerror (errno);
  pthread_mutex_unlock (&(new->mutex));
  pthread_mutex_destroy (&(new->mutex));
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
  return STATUS_FAIL;
 } else if (child == 0) {
  char **exec_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  close (1);

  dup2 (2, 1);
// we can safely play with the global environment here, since we fork()-ed earlier
  exec_environment = (char **)setcombine ((void **)einit_global_environment, (void **)local_environment, SET_TYPE_STRING);
  exec_environment = __create_environment (exec_environment, variables);

  if (options & PEXEC_OPTION_SAFEENVIRONMENT) {
   fprintf (stderr, " >> \"%s\": NOT using environment {%s}, but {%s} instead.\n", set2str(':', cmd), set2str(':', exec_environment), set2str(':', safe_environment));
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

  pthread_mutex_lock (&(new->mutex));
  pthread_mutex_unlock (&(new->mutex));
  pthread_mutex_destroy (&(new->mutex));
  pidstatus = new->status;

  pthread_mutex_lock (&pexec_running_mutex);
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
  pthread_mutex_unlock (&pexec_running_mutex);
  free (new);
 }

 if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS)) return STATUS_OK;
 return STATUS_FAIL;
}

#else

int __pexec_function (char *command, char **variables, uid_t uid, gid_t gid, char *user, char *group, char **local_environment, struct einit_event *status) {
 int pipefderr [2];
 pid_t child;
 int pidstatus = 0;
 char **cmd, **cmdsetdup, options = (status ? 0 : PEXEC_OPTION_NOPIPE);
 uint32_t cs = STATUS_OK;

 lookupuidgid (&uid, &gid, user, group);

 if (!command) return STATUS_FAIL;
// if the first command is pexec-options, then set some special options
 if (strstr (command, "pexec-options") == command) {
  char *ocmds = command,
  *rcmds = strchr (ocmds, ';'),
  **optx = NULL;
  if (!rcmds) return STATUS_FAIL;

  *rcmds = 0;
  optx = str2set (' ', ocmds);
  *rcmds = ';';

  command = rcmds+1;

  if (optx) {
   unsigned int x = 0;
   for (; optx[x]; x++) {
    if (!strcmp (optx[x], "no-pipe")) {
     options |= PEXEC_OPTION_NOPIPE;
    } else if (!strcmp (optx[x], "safe-environment")) {
     options |= PEXEC_OPTION_SAFEENVIRONMENT;
    } else if (!strcmp (optx[x], "dont-close-stdin")) {
     options |= PEXEC_OPTION_DONTCLOSESTDIN;
    }
   }

   free (optx);
  }
 }
 if (!command || !command[0]) return STATUS_FAIL;

 if (!(options & PEXEC_OPTION_NOPIPE)) {
  if (pipe (pipefderr)) {
   status->string = strerror (errno);
   return STATUS_FAIL;
  }
 }

 cmdsetdup = str2set ('\0', command);
 cmd = (char **)setcombine ((void *)shell, (void **)cmdsetdup, -1);

 if (status) {
  status->string = command;
  status_update (status);
 }

 if ((child = fork()) < 0) {
  if (status)
   status->string = strerror (errno);
  return STATUS_FAIL;
 } else if (child == 0) {
  char **exec_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  if (!(options & PEXEC_OPTION_DONTCLOSESTDIN))
   close (0);

  close (1);
  if (!(options & PEXEC_OPTION_NOPIPE)) {
   close (2);
   close (pipefderr [0]);
   dup2 (pipefderr [1], 1);
   dup2 (pipefderr [1], 2);
  } else {
   dup2 (2, 1);
  }

// we can safely play with the global environment here, since we fork()-ed earlier
  exec_environment = (char **)setcombine ((void **)einit_global_environment, (void **)local_environment, SET_TYPE_STRING);
  exec_environment = __create_environment (exec_environment, variables);

  execve (cmd[0], cmd, exec_environment);
  perror (cmd[0]);
  free (cmd);
  free (cmdsetdup);
  exit (EXIT_FAILURE);
 } else {
  ssize_t br;
  ssize_t ic = 0;
  ssize_t i;
  FILE *fx;

  if (!(options & PEXEC_OPTION_NOPIPE)) {
   close (pipefderr[1]);
   char buf[BUFFERSIZE+1];
   char lbuf[BUFFERSIZE+1];

   if (fx = fdopen(pipefderr[0], "r")) {
    char rxbuffer[1024];
    setvbuf (fx, NULL, _IONBF, 0);

    while (!feof(fx)) {
     if (!fgets(rxbuffer, 1024, fx)) {
      if (errno == EAGAIN) continue;
      break;
     }

     char **fbc = str2set ('|', rxbuffer), orest = 1;
     strtrim (rxbuffer);

     if (fbc) {
      if (!strcmp (fbc[0], "feedback")) {
// suppose things are going fine until proven otherwise
       cs = STATUS_OK;

       if (!strcmp (fbc[1], "notice")) {
        orest = 0;
        status->string = fbc[2];
        status_update (status);
       } else if (!strcmp (fbc[1], "warning")) {
        orest = 0;
        status->string = fbc[2];
        status->flag++;
        status_update (status);
       } else if (!strcmp (fbc[1], "success")) {
        orest = 0;
        cs = STATUS_OK;
        status->string = fbc[2];
        status_update (status);
       } else if (!strcmp (fbc[1], "failure")) {
        orest = 0;
        cs = STATUS_FAIL;
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

     if (waitpid (child, &pidstatus, WNOHANG) == child) {
      if (WIFEXITED(pidstatus) || WIFSIGNALED(pidstatus))
       break;
     }
    }

    fclose (fx);
   } else {
    perror ("pexec(): open pipe");
   }
  } else {
   status->string = "NOT piping, check stderr for program output";
   status_update (status);
  }

  do {
//   puts (" >> calling a wait()");
   waitpid (child, &pidstatus, 0);
  } while (!WIFEXITED(pidstatus) && !WIFSIGNALED(pidstatus));
 }

 if (cs == STATUS_FAIL) return STATUS_FAIL;
 if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS)) return STATUS_OK;
 return STATUS_FAIL;
}
#endif

void *dexec_watcher (struct spidcb *spid) {
 pid_t pid = spid->pid;
 int status = spid->status;
 struct daemonst *prev = NULL;
 struct dexecinfo *dx = NULL;
 struct lmodule *module = NULL;
 pthread_mutex_lock (&running_mutex);
 struct daemonst *cur = running;
 char stmp[1024];

// puts (" ++ dexec_watcher() called");
 while (cur) {
  dx = cur->dx;
//  printf ("pid=%i,cap=%i;\n", pid, cur->pid);
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
 pthread_mutex_unlock (&running_mutex);

//   fprintf (stderr, "einit-mod-daemon: eliminated.\n");
 if (dx) {
//  puts ("match++");
  char *rid = (module && module->module && module->module->rid ? module->module->rid : "unknown");
/* if we're already deactivating this daemon, resume the original function */
  if (pthread_mutex_trylock (&cur->mutex)) {
   snprintf (stmp, 1024, "einit-mod-daemon: \"%s\" has died nicely, resuming.\n", rid);
   notice (8, stmp);
   pthread_mutex_unlock (&cur->mutex);
  } else if (dx->restart) {
/* don't try to restart if the daemon died too swiftly */
   pthread_mutex_unlock (&cur->mutex);
   if (((cur->starttime + spawn_timeout) < time(NULL))) {
    struct einit_event fb = evstaticinit(EVE_FEEDBACK_MODULE_STATUS);
    fb.para = (void *)module;
    fb.task = MOD_ENABLE | MOD_FEEDBACK_SHOW;
    fb.status = STATUS_WORKING;
    fb.flag = 0;

    snprintf (stmp, 1024, "einit-mod-daemon: resurrecting \"%s\".\n", rid);
    fb.string = stmp;
    fb.integer = module->fbseq+1;
    status_update ((&fb));

    dx->cb = NULL;
    __start_daemon_function (dx, &fb);
   } else {
    dx->cb = NULL;
    snprintf (stmp, 1024, "einit-mod-daemon: \"%s\" has died too swiftly, considering defunct.\n", rid);
    notice (5, stmp);
    if (module)
     mod (MOD_DISABLE, module);
   }
  } else {
   pthread_mutex_unlock (&cur->mutex);
   dx->cb = NULL;
   snprintf (stmp, 1024, "einit-mod-daemon: \"%s\" has died, but does not wish to be restarted.\n", rid);
   notice (5, stmp);
   if (module)
    mod (MOD_DISABLE, module);
  }
 }

// free (cur);
}

int __start_daemon_function (struct dexecinfo *shellcmd, struct einit_event *status) {
 pid_t child;
 uid_t uid;
 gid_t gid;
// char *cmddup;

 if (!shellcmd) return STATUS_FAIL;

 if (shellcmd->pidfile) {
  fprintf (stderr, " >> unlinking file \"%s\"\n.", shellcmd->pidfile);
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd->prepare) {
//  if (pexec (shellcmd->prepare, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status) == STATUS_FAIL) return STATUS_FAIL;
  if (pexec (shellcmd->prepare, shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status) == STATUS_FAIL) return STATUS_FAIL;
 }

// if ((status->task & MOD_ENABLE) && (!shellcmd || !shellcmd->command)) return STATUS_FAIL;

// if (status->task & MOD_ENABLE)
// else return STATUS_OK;

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
 pthread_mutex_init (&new->mutex, NULL);
 pthread_mutex_lock (&running_mutex);
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
  return STATUS_FAIL;
 } else if (child == 0) {
  char **cmd;
  char **cmdsetdup;
  char **daemon_environment;

  if (gid && (setgid (gid) == -1))
   perror ("setting gid");
  if (uid && (setuid (uid) == -1))
   perror ("setting uid");

  cmdsetdup = str2set ('\0', shellcmd->command);
  cmd = (char **)setcombine ((void *)shell, (void **)cmdsetdup, 0);
//  close (0);
//  close (1);
//  close (2);
  close (1);
  dup2 (2, 1);

  daemon_environment = (char **)setcombine ((void **)einit_global_environment, (void **)shellcmd->environment, SET_TYPE_STRING);
  daemon_environment = __create_environment (daemon_environment, shellcmd->variables);

  execve (cmd[0], cmd, daemon_environment);
  free (cmd);
  free (cmdsetdup);
  exit (EXIT_FAILURE);
 } else {
  new->pid = child;
  sched_watch_pid (child, dexec_watcher);
 }
 pthread_mutex_unlock (&running_mutex);

 return STATUS_OK;
}

void dexec_resume_timer (struct dexecinfo *dx) {
 time_t timer = ((dx && dx->cb) ? dx->cb->timer : 1);
 while (dx && dx->cb && (timer = sleep(timer)));

 if (dx && dx->cb) {
  dx->cb->timer = timer;
  pthread_mutex_trylock (&dx->cb->mutex); // make sure the thing is locked
  pthread_mutex_unlock (&dx->cb->mutex);  // unlock it
 }
}

int __stop_daemon_function (struct dexecinfo *shellcmd, struct einit_event *status) {
 if (shellcmd->cb) {
  pthread_t th;
//  puts ("control-block intact, trying to kill the daemon");
  pthread_mutex_trylock (&shellcmd->cb->mutex);
  shellcmd->cb->timer = kill_timeout_primary;
  kill (shellcmd->cb->pid, SIGTERM);

  pthread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
  pthread_mutex_lock (&shellcmd->cb->mutex);

  if (shellcmd->cb->timer <= 0) { // this means we came here because the timer ran out
   status->string = "SIGTERM timer ran out, killing...";
   status_update (status);

   pthread_cancel (th);
   pthread_mutex_trylock (&shellcmd->cb->mutex);
   shellcmd->cb->timer = kill_timeout_secondary;
   kill (shellcmd->cb->pid, SIGKILL);

   pthread_create (&th, NULL, (void *(*)(void *))dexec_resume_timer, shellcmd);
   pthread_mutex_lock (&shellcmd->cb->mutex);
  }
  pthread_cancel (th);

  pthread_mutex_unlock (&shellcmd->cb->mutex); // just in case
  pthread_mutex_destroy (&shellcmd->cb->mutex);
 }

 shellcmd->cb = NULL;

 if (shellcmd->pidfile) {
  fprintf (stderr, " >> unlinking file \"%s\"\n.", shellcmd->pidfile);
  unlink (shellcmd->pidfile);
  errno = 0;
 }

 if (shellcmd->cleanup) {
 // if (pexec (shellcmd->cleanup, shellcmd->variables, shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group, shellcmd->environment, status) == STATUS_FAIL) return STATUS_OK;
  if (pexec (shellcmd->cleanup, shellcmd->variables, 0, 0, NULL, NULL, shellcmd->environment, status) == STATUS_FAIL) return STATUS_OK;
 }

 return STATUS_OK;
}
