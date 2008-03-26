/*
 *  exec.c
 *  einit
 *
 *  Created by Magnus Deininger on 12/03/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

#include <einit/exec.h>
#include <einit/module.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <einit/config.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>

struct einit_exec_data **einit_exec_running = NULL;
pthread_mutex_t einit_exec_running_mutex = PTHREAD_MUTEX_INITIALIZER;

int einit_handle_pipe_fragment(struct einit_exec_data *x) {
 char buffer[BUFFERSIZE];
 int ret = read(x->readpipe, buffer, BUFFERSIZE-1);

// fprintf (stderr, "dumdidum\n");

 if (ret > 0) {
  buffer[ret] = 0;
  char **sp = str2set ('\n', buffer);
  int i = 0;
  for (; sp[i]; i++) {
   if (strprefix (sp[i], "einit|")) {
    char **cmd = str2set ('|', sp[i]);

    if (cmd[0] && cmd[1] && cmd[2]) {
     if (x->module && strmatch (cmd[1], "pid")) {
      x->module->pid = parse_integer (cmd[2]);
     }
    }

    efree (cmd);
   } else {
    if (!x->rid) {
     notice (5, "%s", buffer);
    } else {
     struct einit_event evx = evstaticinit(einit_feedback_module_status);
     evx.rid = x->rid;
     evx.string = sp[i];
     evx.status = x->module ? x->module->status : status_working;
     event_emit(&evx, einit_event_flag_broadcast);
     evstaticdestroy (evx);
    }
   }
  }
  efree (sp);
 }/* else if (ret < 0) {
  if (!x->rid) {
   notice (5, "%s", strerror(errno));
  } else {
   struct einit_event evx = evstaticinit(einit_feedback_module_status);
   evx.rid = x->rid;
   evx.string = str_stabilise(strerror(errno));
   evx.status = status_working;
   event_emit(&evx, einit_event_flag_broadcast);
   evstaticdestroy (evx);
  }
 }*/ /* else {
  fprintf (stderr, "end of file on pipe\n");
 }*/

// fprintf (stderr, "XdumdidumX\n");

 return ret;
}

char *einit_apply_environment (char *command, char **environment) {
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

char **einit_create_environment (char **environment, char **variables) {
 int i = 0;
 char *variablevalue = NULL;
 if (variables) for (i = 0; variables[i]; i++) {
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
 }

 return environment;
}

pid_t einit_exec (struct einit_exec_data *x) {
 char *sh[4] = { "/bin/sh", "-c", NULL, NULL };
 char **c = NULL;

 char **environment = set_str_dup_stable(einit_global_environment);
 if (x->environment) {
  int i = 0;
  for (; x->environment[i]; i++) {
   environment = set_str_add_stable (environment, x->environment[i]);
  }
 }
 environment = einit_create_environment (environment, x->variables);

 if (x->options & einit_exec_no_shell) {
  int i = 0;
  if (x->command_d) for (; x->command_d[i]; i++) {
   c = set_str_add_stable (c, einit_apply_environment(x->command_d[i], environment));
  }
 } else {
  sh[2] = (char *)einit_apply_environment(x->command, environment);
  c = sh;
 }

 int feedbackpipe[2];

 if (!(x->options & einit_exec_no_pipe)) {
  if (pipe (feedbackpipe) == -1) {
   x->options |= einit_exec_no_pipe;
  } else {
   fcntl (feedbackpipe[0], F_SETFL, O_NONBLOCK);
   fcntl (feedbackpipe[1], F_SETFL, O_NONBLOCK);
  }
 }

 pid_t p = efork();

 if (p < 0) {
  if (!(x->options & einit_exec_no_pipe)) {
   close (feedbackpipe [0]);
   close (feedbackpipe [1]);
  }

  return -1;
 }

 if (!p) { /* child process */
  pid_t sc = 0;
  if (x->options & einit_exec_daemonise) sc = efork();

  if (!sc) {
   if (x->options & einit_exec_create_session) {
    setsid();
   }

   if (x->gid && (setgid (x->gid) == -1)) perror ("setting gid");
   if (x->uid && (setuid (x->uid) == -1)) perror ("setting uid");

   if (!(x->options & einit_exec_keep_stdin)) close (0);

   close (1);

   if (!(x->options & einit_exec_no_pipe)) {
    close (feedbackpipe [0]);

    fcntl (feedbackpipe[1], F_SETFD, 0);
    close (2);
    dup2 (feedbackpipe [1], 1);
    dup2 (feedbackpipe [1], 2);
    close (feedbackpipe [1]);
   }

   dup2 (2, 1);

   execve (c[0], c, environment);

   _exit (EXIT_FAILURE);
  /* we don't return from this 'ere child process, evar */
  } else if (sc > 0) {
   char buffer[128];

   close (feedbackpipe [0]);

   snprintf (buffer, 128, "einit|pid|%i\n", sc);
   write (feedbackpipe [1], buffer, strlen(buffer));
   close (feedbackpipe [1]);

   _exit (EXIT_SUCCESS);
  } else { /* this means the second fork failed */
   char buffer[128];
   close (feedbackpipe [0]);

   snprintf (buffer, 128, "einit|error|efork|%i\n", errno);
   write (feedbackpipe [1], buffer, strlen(buffer));
   close (feedbackpipe [1]);

   _exit (EXIT_FAILURE);
  }
 }

 if (!(x->options & einit_exec_no_pipe)) {
  close (feedbackpipe [1]);
  x->readpipe = feedbackpipe [0];

  if (!x->handle_pipe_fragment)
   x->handle_pipe_fragment = einit_handle_pipe_fragment;
 }

 if (x->options & einit_exec_create_session) {
  int ctty = -1;
  pid_t curpgrp;

  setpgid (p, p);  // create a new process group for the new process
  if (((curpgrp = tcgetpgrp(ctty = 2)) < 0) ||
      ((curpgrp = tcgetpgrp(ctty = 0)) < 0) ||
      ((curpgrp = tcgetpgrp(ctty = 1)) < 0)) tcsetpgrp(ctty, p); // set foreground group
 }

 x->pid = p;

 emutex_lock (&einit_exec_running_mutex);
 einit_exec_running = (struct einit_exec_data **)set_noa_add ((void **)einit_exec_running, x);
 emutex_unlock (&einit_exec_running_mutex);

 einit_ping_core();

 return p;
}

pid_t einit_exec_without_shell (char ** c) {
 struct einit_exec_data *x = ecalloc (1, sizeof (struct einit_exec_data));

 x->command_d = c;
 x->options |= einit_exec_no_shell;

 pid_t p = einit_exec (x);

 return p;
}

pid_t einit_exec_without_shell_with_function_on_process_death (char ** c, void (*handle_dead_process)(struct einit_exec_data *), struct lmodule *module) {
 struct einit_exec_data *x = ecalloc (1, sizeof (struct einit_exec_data));

 x->command_d = c;
 x->options = einit_exec_no_shell;
 x->module = module;
 x->handle_dead_process = handle_dead_process;

 pid_t p = einit_exec (x);

 return p;
}

pid_t einit_exec_without_shell_with_function_on_process_death_keep_stdin (char ** c, void (*handle_dead_process)(struct einit_exec_data *), struct lmodule *module) {
 struct einit_exec_data *x = ecalloc (1, sizeof (struct einit_exec_data));

 x->command_d = c;
 x->options = einit_exec_no_shell | einit_exec_keep_stdi;
 x->module = module;
 x->handle_dead_process = handle_dead_process;

 pid_t p = einit_exec (x);

 return p;
}

void einit_exec_without_shell_sequence (char *** sequence) {
 while (*sequence) {
  einit_exec_without_shell (*sequence);

  sequence++;
 }
}

pid_t einit_exec_with_shell (char * c) {
 struct einit_exec_data *x = ecalloc (1, sizeof (struct einit_exec_data));

 x->command = c;
 x->options |= einit_exec_shell;

 pid_t p = einit_exec (x);

 return p;
}

pid_t einit_exec_auto (char * c) {
 struct einit_exec_data * x = einit_exec_create_exec_data_from_string (c);

 pid_t p = einit_exec (x);

 efree (x);

 return p;
}

struct einit_exec_data * einit_exec_create_exec_data_from_string (char * c) {
 struct einit_exec_data * x = ecalloc (1, sizeof (struct einit_exec_data));

 x->command = c;
 x->options |= einit_exec_shell;

 return x;
}


int einit_exec_pipe_prepare (fd_set *rfds) {
 int rv = 0, i = 0;

// fprintf (stderr, "einit_exec_pipe_prepare()\n");

 emutex_lock (&einit_exec_running_mutex);
 if (einit_exec_running)
  for (; einit_exec_running[i]; i++) if (einit_exec_running[i]->readpipe) {
   FD_SET(einit_exec_running[i]->readpipe, rfds);

   if (einit_exec_running[i]->readpipe > rv)
    rv = einit_exec_running[i]->readpipe;
  }
 emutex_unlock (&einit_exec_running_mutex);

 return rv;
}

void einit_exec_pipe_handle (fd_set *rfds) {
 int i = 0;

// fprintf (stderr, "einit_exec_pipe_handle()\n");

 struct einit_exec_data **needtohandle = NULL;
 emutex_lock (&einit_exec_running_mutex);
 if (einit_exec_running)
  needtohandle = (struct einit_exec_data **)set_noa_dup (einit_exec_running);
 emutex_unlock (&einit_exec_running_mutex);

 if (needtohandle) {
  for (; needtohandle[i]; i++) {
   if (needtohandle[i]->readpipe) {
    fprintf (stderr, "checking pipe: %i\n", needtohandle[i]->readpipe);
    if (FD_ISSET (needtohandle[i]->readpipe, rfds)) {
     fprintf (stderr, "reading pipe: %i\n", needtohandle[i]->readpipe);
     int r = needtohandle[i]->handle_pipe_fragment (needtohandle[i]);
     fprintf (stderr, "done reading pipe: %i\n", needtohandle[i]->readpipe);

     if ((r == 0) || ((r < 0) && (errno != EAGAIN) && (errno != EINTR))) {
      int p = needtohandle[i]->readpipe;
      fprintf (stderr, "pipe's dead: %i\n", needtohandle[i]->readpipe);

      needtohandle[i]->readpipe = 0;
      close (p);
     }
    }
   }

   if (needtohandle[i]->pid) { /* check if the pid has died */
    fprintf (stderr, "checking process: %i\n", needtohandle[i]->pid);
    int p;

    retry_waitpid:

    p = waitpid(needtohandle[i]->pid, &(needtohandle[i]->status), WNOHANG);

    if ((p > 0) && (WIFEXITED(needtohandle[i]->status) || WIFSIGNALED(needtohandle[i]->status))) {
     fprintf (stderr, "process dead: %i\n", needtohandle[i]->pid);

     if (needtohandle[i]->handle_dead_process) {
      needtohandle[i]->handle_dead_process (needtohandle[i]);
     }

     needtohandle[i]->pid = 0;
    }

    if (p < 0) {
     fprintf (stderr, "error while checking process's status: %i\n", needtohandle[i]->pid);

     if (errno != EINTR) {
      if (needtohandle[i]->handle_dead_process) {
       needtohandle[i]->handle_dead_process (needtohandle[i]);
      }

      needtohandle[i]->pid = 0;
     } else {
      goto retry_waitpid;
     }
    }
   }

   if (!needtohandle[i]->pid && !needtohandle[i]->readpipe) {
    emutex_lock (&einit_exec_running_mutex);
    einit_exec_running = (struct einit_exec_data **)setdel ((void **)einit_exec_running, needtohandle[i]);
    emutex_unlock (&einit_exec_running_mutex);
   }
  }

  efree (needtohandle);
 }
}
