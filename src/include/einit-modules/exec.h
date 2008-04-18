/*
 *  exec.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 23/11/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2006-2008, Magnus Deininger All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. *
 * Neither the name of the project nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EINIT_MODULES_EXEC_H
#define EINIT_MODULES_EXEC_H

#include <einit/module.h>
#include <einit/event.h>
#include <einit-modules/configuration.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

    enum pexec_options {
        pexec_option_nopipe = 0x1,
        pexec_option_dont_close_stdin = 0x4
    };

    enum daemon_options {
        daemon_model_forking = 0x0001,
        daemon_did_recovery = 0x0002
    };

    /*
     * structures 
     */
    struct dexecinfo {
        char *id;
        char *command;
        char *prepare;
        char *cleanup;
        char *is_up;
        char *is_down;
        char **variables;
        uid_t uid;
        gid_t gid;
        char *user, *group;
        int restart;
        struct daemonst *cb;
        char **environment;
        char *pidfile;
        char **need_files;
        char **oattrs;

        enum daemon_options options;

        time_t pidfiles_last_update;
    };

    struct daemonst {
        pid_t pid;
        int status;
        time_t starttime;
        time_t timer;
        char *daemon_rid;
        struct dexecinfo *dx;
        struct daemonst *next;
    };

#ifdef BUGGY_PTHREAD_CHILD_WAIT_HANDLING
    struct execst {
        pid_t pid;
        int status;
        struct execst *next;
    };
#endif

#if (! defined(einit_modules_exec)) || (einit_modules_exec == 'm') || (einit_modules_exec == 'n')

    /*
     * function types 
     */
    typedef int (*pexec_function) (const char *, const char **, uid_t,
                                   gid_t, const char *, const char *,
                                   char **, struct einit_event *);
    typedef int (*daemon_function) (struct dexecinfo *,
                                    struct einit_event *);
    typedef char **(*environment_function) (char **, const char **);

    typedef void (*variable_checkup_function) (const char *, const char **,
                                               FILE *);

    /*
     * functions 
     */
    pexec_function f_pxe;
    pexec_function f_exe;
    daemon_function f_start_daemon, f_stop_daemon;
    environment_function f_create_environment;
    variable_checkup_function f_check_variables;

#define exec_configure(mod) f_pxe = NULL; f_start_daemon = NULL; f_stop_daemon = NULL; f_create_environment = NULL; f_check_variables = NULL;

#define pexec(command, variables, uid, gid, user, group, local_environment, status) ((f_pxe || (f_pxe = function_find_one("einit-execute-command", 1, NULL)))? f_pxe(command, variables, uid, gid, user, group, local_environment, status) : status_failed)

#define pexec_v1(command,variables,env,status) pexec (command, variables, 0, 0, NULL, NULL, env, status)
#define pexec_simple(command, status) pexec (command, NULL, 0, 0, NULL, NULL, NULL, status);

#define eexec(command, variables, uid, gid, user, group, local_environment, status) ((f_exe || (f_exe = function_find_one("einit-execute-command-in-main-loop", 1, NULL)))? f_exe(command, variables, uid, gid, user, group, local_environment, status) : status_failed)

#define startdaemon(execheader, status) ((f_start_daemon || (f_start_daemon = function_find_one("einit-execute-daemon", 1, NULL)))? f_start_daemon(execheader, status) : status_failed)
#define stopdaemon(execheader, status) ((f_stop_daemon || (f_stop_daemon = function_find_one("einit-stop-daemon", 1, NULL)))? f_stop_daemon(execheader, status) : status_failed)

#define create_environment(environment, variables) ((f_create_environment || (f_create_environment = function_find_one("einit-create-environment", 1, NULL)))? f_create_environment(environment, variables) : environment)

#define check_variables(output_id, variables, target) ((f_check_variables || (f_check_variables = function_find_one("einit-check-variables", 1, NULL)))? f_check_variables(output_id, variables, target) : NULL)

#else

    char **check_variables_f(const char *, const char **, FILE *);
    int pexec_f(const char *command, const char **variables, uid_t uid,
                gid_t gid, const char *user, const char *group,
                char **local_environment, struct einit_event *status);

    int eexec_f(const char *command, const char **variables, uid_t uid,
                gid_t gid, const char *user, const char *group,
                char **local_environment, struct einit_event *status);

    int start_daemon_f(struct dexecinfo *shellcmd,
                       struct einit_event *status);
    int stop_daemon_f(struct dexecinfo *shellcmd,
                      struct einit_event *status);
    char **create_environment_f(char **environment,
                                const char **variables);

#define exec_configure(mod) ;

#define pexec(command, variables, uid, gid, user, group, local_environment, status) pexec_f(command, variables, uid, gid, user, group, local_environment, status)
#define pexec_v1(command,variables,env,status) pexec (command, variables, 0, 0, NULL, NULL, env, status)
#define pexec_simple(command, status) pexec (command, NULL, 0, 0, NULL, NULL, NULL, status);

#define eexec(command, variables, uid, gid, user, group, local_environment, status) eexec_f(command, variables, uid, gid, user, group, local_environment, status)

#define startdaemon(execheader, status) start_daemon_f(execheader, status)
#define stopdaemon(execheader, status) stop_daemon_f(execheader, status)

#define create_environment(environment, variables) create_environment_f(environment, variables)

#define check_variables(output_id, variables, target) check_variables_f(output_id, variables, target)

#endif

#endif

#ifdef __cplusplus
}
#endif
