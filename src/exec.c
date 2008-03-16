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

struct einit_exec_data **einit_exec_running = NULL;
pthread_mutex_t einit_exec_running_mutex = PTHREAD_MUTEX_INITIALIZER;

int einit_handle_pipe_fragment(struct einit_exec_data *x) {
 char buffer[BUFFERSIZE];
 int ret = read(x->readpipe, buffer, BUFFERSIZE-1);

 if (ret > 0) {
  buffer[ret] = 0;
  if (!x->rid) {
   notice (5, "%s", buffer);
  } else {
   struct einit_event evx = evstaticinit(einit_feedback_module_status);
   evx.rid = x->rid;
   evx.string = buffer;
   evx.status = status_working;
   event_emit(&evx, einit_event_flag_broadcast);
   evstaticdestroy (evx);
  }
 }

 return ret;
}

pid_t einit_exec (struct einit_exec_data *x) {
 pid_t p = efork();

 char *sh[4] = { "/bin/sh", "-c", NULL, NULL };
 char **c = NULL;

 char **environment = set_str_dup_stable(einit_global_environment);

 if (x->options & einit_exec_no_shell) {
  c = x->command_d;
 } else {
  sh[2] = (char *)x->command;
  c = sh;
 }

 int feedbackpipe[2];

 if (!(x->options & einit_exec_no_pipe)) {
  if (pipe (feedbackpipe) == -1) {
   x->options |= einit_exec_no_pipe;
  }
 }

 if (p < 0) {
  if (!(x->options & einit_exec_no_pipe)) {
   close (feedbackpipe [0]);
   close (feedbackpipe [1]);
  }

  return -1;
 }

 if (!p) { /* child process */
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

 emutex_lock (&einit_exec_running_mutex);
 einit_exec_running = (struct einit_exec_data **)set_noa_add ((void **)einit_exec_running, x);
 emutex_unlock (&einit_exec_running_mutex);

 return p;
}

int einit_exec_wait(pid_t p) {
 
}

pid_t einit_exec_without_shell (char ** c) {
 struct einit_exec_data x;
 memset (&x, 0, sizeof(struct einit_exec_data));

 x.command_d = c;
 x.options |= einit_exec_no_shell;

 pid_t p = einit_exec (&x);

 return p;
}

void einit_exec_without_shell_sequence (char *** sequence) {
 while (*sequence) {
  einit_exec_wait (einit_exec_without_shell (*sequence));

  sequence++;
 }
}

pid_t einit_exec_with_shell (char * c) {
 struct einit_exec_data x;
 memset (&x, 0, sizeof(struct einit_exec_data));

 x.command = c;
 x.options |= einit_exec_shell;

 pid_t p = einit_exec (&x);

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

 emutex_lock (&einit_exec_running_mutex);
 if (einit_exec_running)
  for (; einit_exec_running[i]; i++) if (einit_exec_running[i]->readpipe) {
   if (FD_ISSET (einit_exec_running[i]->readpipe, rfds)) {
    int r = einit_exec_running[i]->handle_pipe_fragment (einit_exec_running[i]);

    if ((r == 0) || ((r < 0) && (errno != EAGAIN) && (errno != EINTR))) { /* pipe has died -> waitpid() */
     close (einit_exec_running[i]->readpipe);

     do {
      waitpid(einit_exec_running[i]->pid, &(einit_exec_running[i]->status), 0);
     } while (!WIFEXITED(einit_exec_running[i]->status) && !WIFSIGNALED(einit_exec_running[i]->status));

     if (einit_exec_running[i]->handle_dead_process)
	  einit_exec_running[i]->handle_dead_process (einit_exec_running[i]);
    }
   }
  }
 emutex_unlock (&einit_exec_running_mutex);
}
