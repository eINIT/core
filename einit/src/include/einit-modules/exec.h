/*
 *  exec.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 23/11/2006.
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

#ifndef _EINIT_MODULES_EXEC_H
#define _EINIT_MODULES_EXEC_H

#include <einit/module.h>
#include <einit/scheduler.h>
#include <einit/event.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* structures */
struct dexecinfo {
 char *command;
 char *prepare;
 char *cleanup;
 char **variables;
 uid_t uid;
 gid_t gid;
 char *user, *group;
 int restart;
 struct daemonst *cb;
 char **environment;
};

struct daemonst {
 pid_t pid;
 int status;
 time_t starttime;
 struct lmodule *module;
 struct dexecinfo *dx;
 struct daemonst *next;
 pthread_mutex_t mutex;
};

struct execst {
 pid_t pid;
 pthread_mutex_t mutex;
 int status;
 struct execst *next;
};

/* function types */
typedef int (*pexec_function)(char *, char **,  uid_t, gid_t, char *, char *, char **, struct einit_event *);
typedef int (*daemon_function)(struct dexecinfo *, struct einit_event *);

/* functions */
pexec_function __f_pxe;
daemon_function __f_start_daemon, __f_stop_daemon;

#define pexec(command, variables, uid, gid, user, group, local_environment, status) (__f_pxe? __f_pxe(command, variables, uid, gid, user, group, local_environment, status) : ((__f_pxe = function_find_one("einit-execute-command", 1, NULL)) ? __f_pxe(command, variables, uid, gid, user, group, local_environment, status) : STATUS_FAIL))
#define pexec_simple(command,variables,env,status) pexec (command, variables, 0, 0, NULL, NULL, env, status)
#define exec_configure(mod) __f_pxe = NULL; __f_start_daemon = NULL; __f_stop_daemon = NULL;
#define exec_cleanup(mod) __f_pxe = NULL; __f_start_daemon = NULL; __f_stop_daemon = NULL;
#define startdaemon(execheader, status) (__f_start_daemon? __f_start_daemon(execheader, status) : ((__f_start_daemon = function_find_one("einit-execute-daemon", 1, NULL)) ? __f_start_daemon(execheader, status) : STATUS_FAIL))
#define stopdaemon(execheader, status) (__f_stop_daemon? __f_stop_daemon(execheader, status) : ((__f_stop_daemon = function_find_one("einit-stop-daemon", 1, NULL)) ? __f_stop_daemon(execheader, status) : STATUS_FAIL))

#endif
