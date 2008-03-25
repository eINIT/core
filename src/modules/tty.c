/*
 *  tty.c
 *  einit
 *
 *  Created by Magnus Deininger on 20/04/2006.
 *  Renamed from tty.c on 11/10/2006.
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
#include <stdint.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit/event.h>
#include <string.h>
#include <einit-modules/process.h>
#include <einit-modules/exec.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#include <ctype.h>

#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#ifdef __linux__
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
/* okay, i think i found the proper file now */
#include <asm/ioctls.h>
#include <linux/vt.h>

#include <sys/syscall.h>
#endif

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

struct ttyst {
 pid_t pid;
 int restart;
 struct ttyst *next;
 struct cfgnode *node;
};

int einit_tty_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_tty_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "TTY-Configuration",
 .rid       = "einit-tty",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_tty_configure
};

module_register(einit_tty_self);

#endif

struct ttyst *ttys = NULL;
pthread_mutex_t ttys_mutex = PTHREAD_MUTEX_INITIALIZER;
char einit_tty_feedback_blocked = 0;

int einit_tty_texec (struct cfgnode *);

void einit_tty_process_event_handler (struct einit_event *);

void *einit_tty_watcher (intptr_t pid_i) {
 pid_t pid = (pid_t)pid_i;

 emutex_lock (&ttys_mutex);
 struct ttyst *cur = ttys;
 struct ttyst *prev = NULL;
 struct cfgnode *node = NULL;
 while (cur) {
  if (cur->pid == pid) {
   killpg (pid, SIGHUP); // send a SIGHUP to the getty's process group

   if (cur->restart)
    node = cur->node;
   if (prev != NULL) {
    prev->next = cur->next;
   } else {
    ttys = cur->next;
   }
   efree (cur);
   break;
  }
  prev = cur;
  cur = cur->next;
 }
 emutex_unlock (&ttys_mutex);

 if (node) {
  if (node->id) {
   char tmp[BUFFERSIZE];
   esprintf (tmp, BUFFERSIZE, "einit-tty: restarting: %s\n", node->id);
   notice (6, tmp);
  }
  emutex_lock (&ttys_mutex);
  einit_tty_texec (node);
  emutex_unlock (&ttys_mutex);
 }

 return 0;
}

void einit_tty_process_event_handler (struct einit_event *ev) {
 intptr_t p = ev->integer;

 ethread_spawn_detached ((void *(*)(void *))einit_tty_watcher, (void *)p);
}

int einit_tty_texec (struct cfgnode *node) {
 int i = 0, restart = 0;
 char *device = NULL, *command = NULL;
 char **environment = set_str_dup_stable (einit_global_environment);
 char **variables = NULL;

 for (; node->arbattrs[i]; i+=2) {
  if (strmatch("dev", node->arbattrs[i]))
   device = node->arbattrs[i+1];
  else if (strmatch("command", node->arbattrs[i]))
   command = node->arbattrs[i+1];
  else if (strmatch("restart", node->arbattrs[i]))
   restart = strmatch(node->arbattrs[i+1], "yes");
  else if (strmatch("variables", node->arbattrs[i])) {
   variables = str2set (':', node->arbattrs[i+1]);
  } else {
   environment = straddtoenviron (environment, node->arbattrs[i], node->arbattrs[i+1]);
  }
 }

 environment = create_environment(environment, (const char **)variables);
 if (variables) efree (variables);

 if (command) {
  char **cmds = str2set (' ', command);
  pid_t cpid;
  if (cmds && cmds[0]) {
   struct stat statbuf;
   if (lstat (cmds[0], &statbuf)) {
    char cret [BUFFERSIZE];
    esprintf (cret, BUFFERSIZE, "%s: not forking, %s: %s", ( node->id ? node->id : "unknown node" ), cmds[0], strerror (errno));
    notice (2, cret);
   } else {
    int cpipes[2];
    if (pipe (cpipes)) {
     notice (1, "tty.c: couldn't create an I/O pipe");
     return status_failed;
    }
    fcntl (cpipes[0], F_SETFD, FD_CLOEXEC);
    fcntl (cpipes[1], F_SETFD, FD_CLOEXEC);

#ifdef __linux__
   if ((cpid = syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL)) == 0)
#else
   retry_fork:
   if ((cpid = fork()) < 0) {
    goto retry_fork;
   } else if (cpid == 0)
#endif
   {
    close (cpipes[0]);

    /* this 'ere is the code that gets executed in the child process */
    /* let's fork /again/, so that the main einit monitor process can pick these processes up */

#ifdef __linux__
    pid_t cfork = syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL); /* i was wrong about using the real fork */
#else
    pid_t cfork = fork();                                                                                                  
#endif

    switch (cfork) {
     case -1:
      close (cpipes[1]);
      _exit (-1);
      break;

     case 0:
     {
      close (cpipes[1]);

      setsid();

      disable_core_dumps ();

      if (device) {
       int newfd = eopen(device, O_RDWR);
       if (newfd > 0) {
        close(0);
        close(1);
        close(2);
        dup2 (newfd, 0);
        dup2 (newfd, 1);
        dup2 (newfd, 2);
       }

#ifdef __linux__
       int fdc = open ("/dev/console", O_WRONLY | O_NOCTTY);
       if (fdc > 0) {
        ioctl(fdc, TIOCSCTTY, 1);
        close (fdc);
       }
#endif
      }
      execve (cmds[0], cmds, environment);
      bitch (bitch_stdio, 0, "execve() failed.");
      exit(-1);
      break;
     }
     default:
      /* exit and return the new child's PID */
      write (cpipes[1], &cfork, sizeof(pid_t));
      close (cpipes[1]);
      _exit (0);
      break;
    }

    _exit (-1); /* never reached */
   } else if (cpid != -1) {
    int rstatus;

    close (cpipes[1]);

    do {
     waitpid(cpid, &rstatus, 0);
    } while (!WIFEXITED(rstatus) && !WIFSIGNALED(rstatus));

    if (WIFSIGNALED(rstatus)) {
     notice (1, "tty.c: intermediate child process died");
     close (cpipes[0]);
     return status_failed;
    }

    pid_t realpid = WEXITSTATUS(rstatus);

    if (realpid < 0) {
     notice (1, "tty.c: couldn't fork() to associate the new terminal process with einit's monitor.");
     close (cpipes[0]);
     return status_failed;
    }

    while (read (cpipes[0], &realpid, sizeof(pid_t)) < 0);

    int ctty = -1;
    pid_t curpgrp;

    setpgid (realpid, realpid);  // create a new process group for the new process
    if (((curpgrp = tcgetpgrp(ctty = 2)) < 0) ||
        ((curpgrp = tcgetpgrp(ctty = 0)) < 0) ||
        ((curpgrp = tcgetpgrp(ctty = 1)) < 0)) tcsetpgrp(ctty, realpid); // set foreground group

    struct ttyst *new = ecalloc (1, sizeof (struct ttyst));
    new->pid = realpid;
    new->node = node;
    new->restart = restart;

    new->next = ttys;
    ttys = new;
   } else {
    close (cpipes[1]);
   }

   close (cpipes[0]);

   efree (cmds);
  }
  }
 }

 if (environment) {
  efree (environment);
  environment = NULL;
 }
 if (variables) {
  efree (variables);
  variables = NULL;
 }

 return 0;
}

void einit_tty_disable_unused (char **enab_ttys) {
 emutex_lock (&ttys_mutex);
 struct ttyst *cur = ttys;

 while (cur) {
  if (cur->node && (!enab_ttys || !inset ((const void **)enab_ttys, cur->node->id + 18, SET_TYPE_STRING))) {
   notice (4, "disabling tty %s (not used in new mode)", cur->node->id + 18);
   cur->restart = 0;
   killpg (cur->pid, SIGHUP); // send a SIGHUP to the getty's process group
   kill (cur->pid, SIGTERM);
  }
  cur = cur->next;
 }
 emutex_unlock (&ttys_mutex);
}

char einit_tty_is_present (char *ttyname) {
 char present = 0;

// emutex_lock (&ttys_mutex);
 struct ttyst *cur = ttys;

 while (cur) {
  if (cur->node && strmatch (ttyname, cur->node->id + 18)) {
   present = 1;
   break;
  }
  cur = cur->next;
 }
// emutex_unlock (&ttys_mutex);

 return present;
}

int einit_tty_in_switch = 0;

void einit_tty_enable_vector (char **enab_ttys) {
 int i = 0;
 struct cfgnode *node = NULL;

 emutex_lock (&ttys_mutex);

 if (!enab_ttys || strmatch (enab_ttys[0], "none")) {
  notice (4, "no ttys to bring up");
 } else for (i = 0; enab_ttys[i]; i++) if (einit_tty_is_present (enab_ttys[i])) {
  notice (4, "not enabling tty %s (already enabled)", enab_ttys[i]);
 } else {
  char *tmpnodeid = emalloc (strlen(enab_ttys[i])+20);
  notice (4, "enabling tty %s (new)", enab_ttys[i]);

  memcpy (tmpnodeid, "configuration-tty-", 19);
  strcat (tmpnodeid, enab_ttys[i]);

  node = cfg_getnode (tmpnodeid, NULL);
  if (node && node->arbattrs) {
   einit_tty_texec (node);
  } else {
   notice (4, "einit-tty: node %s not found", tmpnodeid);
  }

  efree (tmpnodeid);
 }

 emutex_unlock (&ttys_mutex);
}

void einit_tty_update() {
 char **enab_ttys = NULL;
 int i = 0;
 char sysv_semantics = parse_boolean(cfg_getstring("ttys/sysv-style", NULL));

 if (!einit_tty_feedback_blocked) enab_ttys = str2set (':', cfg_getstring("feedback-ttys", NULL));

 if (!(sysv_semantics && einit_tty_in_switch)) {
  char **tmp_ttys = NULL;
  tmp_ttys = str2set (':', cfg_getstring("ttys", NULL));

  if (tmp_ttys && !strmatch (tmp_ttys[0], "none")) {
   int i = 0;
   for (; tmp_ttys[i]; i++) {
    enab_ttys = set_str_add (enab_ttys, tmp_ttys[i]);
   }

   efree(tmp_ttys);
  }
 }

 notice (4, "reconfiguring ttys");

 einit_tty_disable_unused (enab_ttys);
 einit_tty_enable_vector (enab_ttys);

 char **enabled_ttys = NULL;

 emutex_lock (&ttys_mutex);
 struct ttyst *cur = ttys;
 while (cur) {
  struct cfgnode *node = cur->node;
  char *device = NULL;

  if (node && node->arbattrs) {
   for (i = 0; node->arbattrs[i]; i+=2) {
    if (strmatch("dev", node->arbattrs[i]))
     device = node->arbattrs[i+1];
   }
  }

  if (device) {
   ssize_t len = strlen (device);
   i = len-1;

   while (isdigit (device[i]) && (i > 0)) i--;
   if ((i > 0) && (i++, isdigit(device[i])) && (!enabled_ttys || !inset ((const void **)enabled_ttys, (device+i), SET_TYPE_STRING)))
    enabled_ttys = set_str_add (enabled_ttys, device+i);
  }

  cur = cur->next;
 }
 emutex_unlock (&ttys_mutex);

 if (enabled_ttys) {
  char *s = set2str (':', (const char **)enabled_ttys);
  struct cfgnode newnode;

  memset (&newnode, 0, sizeof(struct cfgnode));

  newnode.id = (char *)str_stabilise ("enabled-ttys");
  newnode.type = einit_node_regular;

  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)"ids");
  newnode.arbattrs = set_str_add_stable (newnode.arbattrs, (void *)s);

  newnode.svalue = newnode.arbattrs[3];

  cfg_addnode (&newnode);

  efree (s);
  efree (enabled_ttys);
 }

 efree (enab_ttys);
}

void einit_tty_update_switching () {
 einit_tty_in_switch = 1;

 einit_tty_update();
}

void einit_tty_update_switch_done () {
 einit_tty_in_switch = 0;

 einit_tty_update();
}

void einit_tty_disable_feedback () {
 einit_tty_feedback_blocked = 1;
 einit_tty_update ();
}

int einit_tty_configure (struct lmodule *this) {
 module_init (this);

 if (coremode & einit_mode_sandbox) {
  return 0;
 }

 exec_configure(this);

 event_listen (einit_process_died, einit_tty_process_event_handler);
 event_listen (einit_core_switching, einit_tty_update_switching);
 event_listen (einit_core_mode_switching, einit_tty_update_switching);
 event_listen (einit_core_done_switching, einit_tty_update_switch_done);
 event_listen (einit_boot_devices_available, einit_tty_update);

 event_listen (einit_ipc_disabling, einit_tty_disable_feedback);

 return 0;
}
