/*
 *  einit-monitor.c
 *  einit
 *
 *  Created by Magnus Deininger on 21/11/2007.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>

#include <einit/configuration.h>
#include <einit/configuration-static.h>

#if defined(LINUX)
#include <sys/prctl.h>
#endif

#if defined(BSD)
#include <sys/stat.h>
#endif

pid_t send_sigint_pid = 0;
char is_sandbox = 0;

#if defined(BSD)
char devfs = 0;
#endif

char *readfd (int fd) {
 int rn = 0;
 void *buf = NULL;
 char *data = NULL;
 ssize_t blen = 0;

 buf = malloc (BUFFERSIZE * 10);
 if (!buf) return NULL;

 do {
  fprintf (stderr, "reading.\n");
  buf = realloc (buf, blen + BUFFERSIZE * 10);
  if (buf == NULL) return NULL;
  fprintf (stderr, ".\n");

  rn = read (fd, (char *)(buf + blen), BUFFERSIZE * 10);
  blen = blen + rn;
 } while (rn > 0);

 fprintf (stderr, "done.\n");

 if (blen > -1) {
  data = realloc (buf, blen+1);
  if (buf == NULL) return NULL;

  data[blen] = 0;
  if (blen > 0) {
   *(data+blen-1) = 0;
  } else {
   free (data);
   data = NULL;
  }
 }

 return data;
}

void einit_sigint (int signal, siginfo_t *siginfo, void *context) {
 if (send_sigint_pid)
  kill (send_sigint_pid, SIGINT);
}

#ifdef BSD
void do_devfs () {
/* this code is from freebsd's init.c, this is probably why things failed earlier */
/*
 * Additional check if devfs needs to be mounted:
 * If "/" and "/dev" have the same device number,
 * then it hasn't been mounted yet.
*/

 close(0);
 close(1);
 close(2);

 if (!devfs) {
  struct stat stst;
  dev_t root_devno;

  stat("/", &stst);
  root_devno = stst.st_dev;
  if (stat("/dev", &stst) != 0)
   warning("Can't stat /dev: %m");
  else if (stst.st_dev == root_devno)
   devfs++;
 }

 if (devfs) {
  struct iovec iov[4];
  char *s;
  int i;

  char _fstype[]  = "fstype";
  char _devfs[]   = "devfs";
  char _fspath[]  = "fspath";
  char _path_dev[]= _PATH_DEV;

  iov[0].iov_base = _fstype;
  iov[0].iov_len = sizeof(_fstype);
  iov[1].iov_base = _devfs;
  iov[1].iov_len = sizeof(_devfs);
  iov[2].iov_base = _fspath;
  iov[2].iov_len = sizeof(_fspath);
                /*
  * Try to avoid the trailing slash in _PATH_DEV.
  * Be *very* defensive.
                */
  s = strdup(_PATH_DEV);
  if (s != NULL) {
   i = strlen(s);
   if (i > 0 && s[i - 1] == '/')
    s[i - 1] = '\0';
   iov[3].iov_base = s;
   iov[3].iov_len = strlen(s) + 1;
  } else {
   iov[3].iov_base = _path_dev;
   iov[3].iov_len = sizeof(_path_dev);
  }
  nmount(iov, 4, 0);
  if (s != NULL)
   free(s);
 }
}
#endif

int run_core (int argc, char **argv, char **env, char *einit_crash_data, int command_pipe, int crash_pipe, char need_recovery) {
#if defined(BSD)
 do_devfs();
#endif

 char *narg[argc + 8];
 int i = 0;
 char tmp1[BUFFERSIZE], tmp2[BUFFERSIZE];

 for (; i < argc; i++) {
  narg[i] = argv[i];
 }

 if (command_pipe) {
  narg[i] = "--command-pipe"; i++;

  snprintf (tmp1, BUFFERSIZE, "%i", command_pipe);
  narg[i] = tmp1; i++;
 }

 if (crash_pipe) {
  narg[i] = "--crash-pipe"; i++;

  snprintf (tmp2, BUFFERSIZE, "%i", crash_pipe);
  narg[i] = tmp2; i++;
 }

 if (einit_crash_data) {
  narg[i] = "--crash-data"; i++;
  narg[i] = einit_crash_data; i++;
 }

 if (need_recovery) {
  narg[i] = "--recover"; i++;
 }

 narg[i] = 0;

 execve (EINIT_LIB_BASE "/bin/einit-core", narg, env);
 perror ("couldn't execute eINIT");
 return -1;
}

int einit_monitor_loop (int argc, char **argv, char **env, char *einit_crash_data, char need_recovery) {
 int commandpipe[2];
 int debugsocket[2];
 FILE *commandpipe_out = NULL;
 pid_t core_pid;

 pipe (commandpipe);

 socketpair (AF_UNIX, SOCK_STREAM, 0, debugsocket);
 fcntl (debugsocket[0], F_SETFD, FD_CLOEXEC | O_NONBLOCK);

 fcntl (commandpipe[1], F_SETFD, FD_CLOEXEC);

 core_pid = fork();

 switch (core_pid) {
  case 0:
   run_core (argc, argv, env, einit_crash_data, commandpipe[0], debugsocket[1], need_recovery);
  case -1:
   perror ("einit-monitor: couldn't fork()");
   sleep (1);
   return einit_monitor_loop(argc, argv, env, NULL, need_recovery);
  default:
   send_sigint_pid = core_pid;
   break;
 }

 commandpipe_out = fdopen (commandpipe[1], "w");
 close (debugsocket[1]);

 if (einit_crash_data) {
  free (einit_crash_data);
  einit_crash_data = NULL;
 }

 while (1) {
  int rstatus;
  pid_t wpid = waitpid(-1, &rstatus, 0); /* this ought to wait for ANY process */

  if (wpid == core_pid) {
   //    goto respawn; /* try to recover by re-booting */
//   if (!debug) if (commandpipe_in) fclose (commandpipe_in);
   if (commandpipe_out) fclose (commandpipe_out);

   if (WIFEXITED(rstatus) && (WEXITSTATUS(rstatus) != einit_exit_status_die_respawn)) {
    fprintf (stderr, "eINIT has quit properly.\n");

    if (!is_sandbox) {
     if (WEXITSTATUS(rstatus) == einit_exit_status_last_rites_halt) {
      execl (EINIT_LIB_BASE "/bin/last-rites", EINIT_LIB_BASE "/bin/last-rites", "h", NULL);
     } else if (WEXITSTATUS(rstatus) == einit_exit_status_last_rites_reboot) {
      execl (EINIT_LIB_BASE "/bin/last-rites", EINIT_LIB_BASE "/bin/last-rites", "r", NULL);
     } else if (WEXITSTATUS(rstatus) == einit_exit_status_last_rites_kexec) {
      execl (EINIT_LIB_BASE "/bin/last-rites", EINIT_LIB_BASE "/bin/last-rites", "k", NULL);
     }
    }

    exit (EXIT_SUCCESS);
   }

   int n = 5;
   fprintf (stderr, "The secondary eINIT process has died, waiting a while before respawning.\n");
   if ((einit_crash_data = readfd (debugsocket[0]))) {
    fprintf (stderr, " > neat, received crash data\n");
   } else {
    fprintf (stderr, " > no crash data...\n");
   }

   while ((n = sleep (n)));
   fprintf (stderr, "Respawning secondary eINIT process.\n");

/*   if (crash_threshold) crash_threshold--;
   else debug = 1;*/
//   need_recovery = 1;
//   initoverride = 0;

   close (debugsocket[0]);

   return einit_monitor_loop(argc, argv, env, einit_crash_data, 1);
  } else {
   if (commandpipe_out) {
    if (WIFEXITED(rstatus)) {
     fprintf (commandpipe_out, "pid %i terminated\n\n", wpid);
    } else {
     fprintf (commandpipe_out, "pid %i died\n\n", wpid);
    }
    fflush (commandpipe_out);
   }
  }
 }
}

int main(int argc, char **argv, char **env) {
 char *argv_mutable[argc+1];
 int i = 0, it = 0;
 char force_init = (getpid() == 1);
 char need_recovery = 0;
 char is_ipc = 0;

#if defined(LINUX) && defined(PR_SET_NAME)
 prctl (PR_SET_NAME, "einit [monitor]", 0, 0, 0);
#endif

 for (; i < argc; i++) {
  if (!strcmp(argv[i], "--force-init")) {
   force_init = 1;
//   continue;
  } else if (!strcmp(argv[i], "--sandbox")) {
   need_recovery = 1;
   is_sandbox = 1;
  } else if (!strcmp(argv[i], "--ipc")) {
   is_ipc = 1;
  }

  argv_mutable[it] = argv[i];
  argv_mutable[it+1] = 0;
  it++;
 }

 if (force_init) {
  struct sigaction action;

  /* signal handlers */
  action.sa_sigaction = einit_sigint;
  sigemptyset(&(action.sa_mask));
  action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
  if ( sigaction (SIGINT, &action, NULL) ) perror ("calling sigaction() failed");

  /* ignore sigpipe */
  action.sa_sigaction = (void (*)(int, siginfo_t *, void *))SIG_IGN;

  if ( sigaction (SIGPIPE, &action, NULL) ) perror ("calling sigaction() failed");

  return einit_monitor_loop (it, argv_mutable, env, NULL, need_recovery);
 }

 if (is_ipc) {
  execve (EINIT_LIB_BASE "/bin/einit-core", argv, env);
  perror ("couldn't execute eINIT");
  return -1;
 }

/* non-ipc, non-core */
 argv_mutable[0] = EINIT_LIB_BASE "/bin/einit-helper";
 execve (EINIT_LIB_BASE "/bin/einit-helper", argv_mutable, env);
 perror ("couldn't execute " EINIT_LIB_BASE "/bin/einit-helper");
 return -1;
}
