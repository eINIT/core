/*
 *  exec.c
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
#include <einit-modules/exec.h>
#include <ctype.h>
#include <sys/stat.h>

#include <einit-modules/parse-sh.h>

#include <regex.h>
#include <time.h>

#include <einit/process.h>

#ifdef __linux__
#include <sys/syscall.h>
#include <linux/sched.h>
#endif

int einit_exec_configure(struct lmodule *);

const struct smodule einit_exec_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "pexec/dexec/eexec library module",
    .rid = "einit-exec",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = einit_exec_configure
};

module_register(einit_exec_self);

/*
 * variables 
 */
struct daemonst *running = NULL;

char **shell = NULL;
char *dshell[] = { "/bin/sh", "-c", NULL };

extern char shutting_down;

int spawn_timeout = 5;
char kill_timeout_primary = 20, kill_timeout_secondary = 20;

char **check_variables_f(const char *, const char **, FILE *);
char *apply_envfile_f(char *command, const char **environment);
int pexec_f(const char *command, const char **variables, uid_t uid,
            gid_t gid, const char *user, const char *group,
            char **local_environment, struct einit_event *status);

int eexec_f(const char *command, const char **variables, uid_t uid,
            gid_t gid, const char *user, const char *group,
            char **local_environment, struct einit_event *status);

int start_daemon_f(struct dexecinfo *shellcmd, struct einit_event *status);
int stop_daemon_f(struct dexecinfo *shellcmd, struct einit_event *status);
char **create_environment_f(char **environment, const char **variables);

char *apply_envfile_f(char *command, const char **environment)
{
    uint32_t i = 0;
    char **variables = NULL;

    if (environment) {
        for (; environment[i]; i++) {
            char *r = estrdup(environment[i]);
            char *n = strchr(r, '=');

            if (n) {
                *n = 0;
                n++;

                if (*n
                    && !inset((const void **) variables, r,
                              SET_TYPE_STRING)) {
                    variables = set_str_add(variables, r);
                    variables = set_str_add(variables, n);
                }
            }

            efree(r);
        }
    }

    if (variables) {
        command = apply_variables(command, (const char **) variables);

#ifdef DEBUG
        write(2, command, strlen(command));
        write(2, "\n", 1);
#endif

        efree(variables);
    }

    return command;
}

char **check_variables_f(const char *id, const char **variables,
                         FILE * output)
{
    uint32_t u = 0;
    if (!variables)
        return (char **) variables;
    for (u = 0; variables[u]; u++) {
        char *e = estrdup(variables[u]), *ep = strchr(e, '/');
        char *x[] = { e, NULL, NULL };
        char node_found = 1;
        uint32_t variable_matches = 0;

        if (ep) {
            *ep = 0;
            x[0] = (char *) str_stabilise(e);
            *ep = '/';

            ep++;
            x[1] = ep;
        }

        struct cfgnode *n;

        if (!(n = cfg_getnode(x[0]))) {
            node_found = 0;
        } else if (x[1] && n->arbattrs) {
            regex_t pattern;
            if (!eregcomp(&pattern, x[1])) {
                uint32_t v = 0;
                for (v = 0; n->arbattrs[v]; v += 2) {
                    if (!regexec(&pattern, n->arbattrs[v], 0, NULL, 0)) {
                        variable_matches++;
                    }
                }

                eregfree(&pattern);
            }
        } else if (cfg_getstring(x[0])) {
            variable_matches++;
        }

        if (!node_found) {
            eprintf(output, " * module: %s: undefined node: %s\n", id,
                    x[0]);
        } else if (!variable_matches) {
            eprintf(output, " * module: %s: undefined variable: %s\n", id,
                    e);
        }

        if (x[0] != e)
            efree(x[0]);
        efree(e);
    }

    return (char **) variables;
}

char **create_environment_f(char **environment, const char **variables)
{
    int i = 0;
    char *variablevalue = NULL;
    if (variables)
        for (i = 0; variables[i]; i++) {
            if ((variablevalue = strchr(variables[i], '/'))) {
                /*
                 * special treatment if we have an attribue specifier in
                 * the variable name 
                 */
                char *name = NULL, *filter = variablevalue + 1;
                struct cfgnode *node;
                *variablevalue = 0;
                name = (char *) str_stabilise(variables[i]);
                *variablevalue = '/';

                if ((node = cfg_getnode(name)) && node->arbattrs) {
                    size_t bkeylen = strlen(name) + 2, pvlen = 1;
                    char *key = emalloc(bkeylen);
                    char *pvalue = NULL;
                    regex_t pattern;

                    if (!eregcomp(&pattern, filter)) {
                        int y = 0;
                        *key = 0;
                        strcat(key, name);
                        *(key + bkeylen - 2) = '/';
                        *(key + bkeylen - 1) = 0;

                        for (y = 0; node->arbattrs[y]; y += 2)
                            if (!regexec
                                (&pattern, node->arbattrs[y], 0, NULL,
                                 0)) {
                                size_t attrlen =
                                    strlen(node->arbattrs[y]) + 1;
                                char *subkey = emalloc(bkeylen + attrlen);
                                *subkey = 0;
                                strcat(subkey, key);
                                strcat(subkey, node->arbattrs[y]);
                                environment =
                                    straddtoenviron(environment, subkey,
                                                    node->arbattrs[y + 1]);
                                efree(subkey);

                                if (pvalue) {
                                    pvalue =
                                        erealloc(pvalue, pvlen + attrlen);
                                    *(pvalue + pvlen - 2) = ' ';
                                    *(pvalue + pvlen - 1) = 0;
                                    strcat(pvalue, node->arbattrs[y]);
                                    pvlen += attrlen;
                                } else {
                                    pvalue = emalloc(pvlen + attrlen);
                                    *pvalue = 0;
                                    strcat(pvalue, node->arbattrs[y]);
                                    pvlen += attrlen;
                                }
                            }

                        eregfree(&pattern);
                    }

                    if (pvalue) {
                        uint32_t txi = 0;
                        for (; pvalue[txi]; txi++) {
                            if (!isalnum(pvalue[txi])
                                && (pvalue[txi] != ' '))
                                pvalue[txi] = '_';
                        }
                        *(key + bkeylen - 2) = 0;
                        environment =
                            straddtoenviron(environment, key, pvalue);

                        efree(pvalue);
                    }
                    efree(key);
                }
            } else {
                /*
                 * else: just add it 
                 */
                char *variablevalue = cfg_getstring(variables[i]);
                if (variablevalue)
                    environment =
                        straddtoenviron(environment, variables[i],
                                        variablevalue);
            }
        }

    /*
     * if (variables) { int i = 0; for (; variables [i]; i++) { char
     * *variablevalue = cfg_getstring (variables [i]); if
     * (variablevalue) { exec_environment = straddtoenviron
     * (exec_environment, variables [i], variablevalue); } } }
     */

    return environment;
}

struct exec_parser_data {
    int commands;
    char **command;
    char forkflag;
};

void exec_callback(char **data, enum einit_sh_parser_pa status,
                   struct exec_parser_data *pd)
{
    switch (status) {
    case pa_end_of_file:
        break;

    case pa_new_context:
    case pa_new_context_fork:
        if (pd->command) {
            efree(pd->command);
        }

        pd->command = set_str_dup_stable(data);
        pd->commands++;
        pd->forkflag = (status == pa_new_context_fork);
        break;

    default:
        break;
    }
}

char **exec_run_sh(char *command, enum pexec_options options,
                   char **exec_environment)
{
    struct exec_parser_data pd;
    char *ocmd = (char *) str_stabilise(command);

    memset(&pd, 0, sizeof(pd));

    command = strip_empty_variables(command);

    parse_sh_ud(command,
                (void (*)(const char **, enum einit_sh_parser_pa, void *))
                exec_callback, &pd);

    if ((pd.commands == 1) && pd.command && !pd.forkflag) {
        char **r = which(pd.command[0]);

        if (r && r[0]) {
            pd.command[0] = r[0];
        }

        char *cmdtx = set2str(',', (const char **) pd.command);
        if (cmdtx) {
            efree(cmdtx);
        }

        return pd.command;
    } else {
        char **cmd;

        if (pd.command)
            efree(pd.command);

        cmd = set_str_dup_stable(shell);
        cmd = set_str_add_stable(cmd, ocmd);

        return cmd;
    }

    return NULL;
}

int pexec_f(const char *command, const char **variables, uid_t uid,
            gid_t gid, const char *user, const char *group,
            char **local_environment, struct einit_event *status)
{
    int pipefderr[2];
    pid_t child;
    int pidstatus = 0;
    enum pexec_options options = (status ? 0 : pexec_option_nopipe);
    uint32_t cs = status_ok;
    char have_waited = 0;

    lookupuidgid(&uid, &gid, user, group);

    if (!command)
        return status_failed;
    // if the first command is pexec-options, then set some special
    // options
    if (strprefix(command, "pexec-options")) {
        char *ocmds = (char *) str_stabilise(command), *rcmds =
            strchr(ocmds, ';'), **optx = NULL;
        if (!rcmds) {
            return status_failed;
        }

        *rcmds = 0;
        optx = str2set(' ', ocmds);
        *rcmds = ';';

        command = rcmds + 1;

        if (optx) {
            unsigned int x = 0;
            for (; optx[x]; x++) {
                if (strmatch(optx[x], "no-pipe")) {
                    options |= pexec_option_nopipe;
                } else if (strmatch(optx[x], "dont-close-stdin")) {
                    options |= pexec_option_dont_close_stdin;
                }
            }

            efree(optx);
        }
    }
    if (!command || !command[0]) {
        return status_failed;
    }

    if (strmatch(command, "true")) {
        return status_ok;
    }

    if (!(options & pexec_option_nopipe)) {
        if (pipe(pipefderr)) {
            fbprintf(status, "failed to create pipe: %s", strerror(errno));
            return status_failed;
        }
        /*
         * make sure the read end won't survive an exec*() 
         */
        fcntl(pipefderr[0], F_SETFD, FD_CLOEXEC);
        if (fcntl(pipefderr[0], F_SETFL, O_NONBLOCK) == -1) {
            bitch(bitch_stdio, errno,
                  "can't set pipe (read end) to non-blocking mode!");
        }
        /*
         * make sure we unset this flag after fork()-ing 
         */
        fcntl(pipefderr[1], F_SETFD, FD_CLOEXEC);
        if (fcntl(pipefderr[1], F_SETFL, O_NONBLOCK) == -1) {
            bitch(bitch_stdio, errno,
                  "can't set pipe (write end) to non-blocking mode!");
        }
    }

    char **exec_environment;

    exec_environment =
        (char **) setcombine((const void **) einit_global_environment,
                             (const void **) local_environment,
                             SET_TYPE_STRING);
    exec_environment = create_environment_f(exec_environment, variables);

    command =
        apply_envfile_f((char *) command,
                        (const char **) exec_environment);

    char **exvec =
        exec_run_sh((char *) command, options, exec_environment);

    /*
     * efree ((void *)command); command = NULL;
     */

#ifdef __linux__
    // void *stack = emalloc (4096);
    // if ((child = syscall(__NR_clone, CLONE_PTRACE | CLONE_STOPPED,
    // stack+4096)) < 0) {

    if ((child =
         syscall(__NR_clone, CLONE_STOPPED | SIGCHLD, 0, NULL, NULL,
                 NULL)) < 0) {
        if (status)
            status->string = strerror(errno);
        return status_failed;
    }
#else
  retry_fork:

    if ((child = fork()) < 0) {
        if (status)
            status->string = strerror(errno);

        goto retry_fork;

        return status_failed;
    }
#endif
    else if (child == 0) {
        /*
         * make sure einit's thread is in a proper state 
         */
#ifndef __linux__
        sched_yield();
#endif

        disable_core_dumps();

        /*
         * cause segfault 
         */
        /*
         * sleep (1); *((char *)0) = 1;
         */

        if (gid && (setgid(gid) == -1))
            perror("setting gid");
        if (uid && (setuid(uid) == -1))
            perror("setting uid");

        if (!(options & pexec_option_dont_close_stdin))
            close(0);

        close(1);
        if (!(options & pexec_option_nopipe)) {
            /*
             * unset this flag after fork()-ing 
             */
            fcntl(pipefderr[1], F_SETFD, 0);
            close(2);
            close(pipefderr[0]);
            dup2(pipefderr[1], 1);
            dup2(pipefderr[1], 2);
            close(pipefderr[1]);
        } else {
            dup2(2, 1);
        }

        execve(exvec[0], exvec, exec_environment);
    } else {
        FILE *fx;

        if (exec_environment)
            efree(exec_environment);
        if (exvec)
            efree(exvec);

        if (!(options & pexec_option_nopipe) && status) {
            /*
             * tag the fd as close-on-exec, just in case 
             */
            fcntl(pipefderr[1], F_SETFD, FD_CLOEXEC);
            close(pipefderr[1]);
            errno = 0;

            if ((fx = fdopen(pipefderr[0], "r"))) {
                char rxbuffer[BUFFERSIZE];
                setvbuf(fx, NULL, _IONBF, 0);

#ifdef __linux__
                kill(child, SIGCONT);
#endif

                if ((waitpid(child, &pidstatus, WNOHANG) == child)
                    && (WIFEXITED(pidstatus) || WIFSIGNALED(pidstatus))) {
                    have_waited = 1;
                } else
                    while (!feof(fx)) {
                        if (!fgets(rxbuffer, BUFFERSIZE, fx)) {
                            if (errno == EAGAIN) {
                                usleep(100);
                                goto skip_read;
                            }
                            break;
                        }

                        char **fbc = str2set('|', rxbuffer), orest = 1;
                        strtrim(rxbuffer);

                        if (fbc) {
                            if (strmatch(fbc[0], "feedback")) {
                                // suppose things are going fine until
                                // proven otherwise
                                cs = status_ok;

                                if (strmatch(fbc[1], "notice")) {
                                    orest = 0;
                                    fbprintf(status, "%s", fbc[2]);
                                } else if (strmatch(fbc[1], "success")) {
                                    orest = 0;
                                    cs = status_ok;
                                    fbprintf(status, "%s", fbc[2]);
                                } else if (strmatch(fbc[1], "failure")) {
                                    orest = 0;
                                    cs = status_failed;
                                    fbprintf(status, "%s", fbc[2]);
                                }
                            }

                            efree(fbc);
                        }

                        if (orest) {
                            fbprintf(status, "%s", rxbuffer);
                        }

                        continue;

                      skip_read:

                        if (waitpid(child, &pidstatus, WNOHANG) == child) {
                            if (WIFEXITED(pidstatus)
                                || WIFSIGNALED(pidstatus)) {
                                have_waited = 1;
                                break;
                            }
                        }
                    }

                efclose(fx);
            } else {
                perror("pexec(): open pipe");
            }
        }
#ifdef __linux__
        else
            kill(child, SIGCONT);
#endif

        if (!have_waited) {
            do {
                waitpid(child, &pidstatus, 0);
            } while (!WIFEXITED(pidstatus) && !WIFSIGNALED(pidstatus));
        }
    }

    if (cs == status_failed)
        return status_failed;
    if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS))
        return status_ok;
    return status_failed;
}


int eexec_f(const char *command, const char **variables, uid_t uid,
            gid_t gid, const char *user, const char *group,
            char **local_environment, struct einit_event *status)
{
    int pipefderr[2];
    pid_t child;
    int pidstatus = 0;
    enum pexec_options options = (status ? 0 : pexec_option_nopipe);
    uint32_t cs = status_ok;
    char have_waited = 0;

    lookupuidgid(&uid, &gid, user, group);

    if (!command)
        return status_failed;
    // if the first command is pexec-options, then set some special
    // options
    if (strprefix(command, "pexec-options")) {
        char *ocmds = (char *) str_stabilise(command), *rcmds =
            strchr(ocmds, ';'), **optx = NULL;
        if (!rcmds) {
            return status_failed;
        }

        *rcmds = 0;
        optx = str2set(' ', ocmds);
        *rcmds = ';';

        command = rcmds + 1;

        if (optx) {
            unsigned int x = 0;
            for (; optx[x]; x++) {
                if (strmatch(optx[x], "no-pipe")) {
                    options |= pexec_option_nopipe;
                } else if (strmatch(optx[x], "dont-close-stdin")) {
                    options |= pexec_option_dont_close_stdin;
                }
            }

            efree(optx);
        }
    }
    if (!command || !command[0]) {
        return status_failed;
    }

    if (strmatch(command, "true")) {
        return status_ok;
    }

    if (!(options & pexec_option_nopipe)) {
        if (pipe(pipefderr)) {
            fbprintf(status, "failed to create pipe: %s", strerror(errno));
            return status_failed;
        }
        /*
         * make sure the read end won't survive an exec*() 
         */
        fcntl(pipefderr[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipefderr[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefderr[1], F_SETFD, FD_CLOEXEC);
        fcntl(pipefderr[1], F_SETFL, O_NONBLOCK);
    }

    char **exec_environment;

    exec_environment =
        (char **) setcombine((const void **) einit_global_environment,
                             (const void **) local_environment,
                             SET_TYPE_STRING);
    exec_environment = create_environment_f(exec_environment, variables);

    command =
        apply_envfile_f((char *) command,
                        (const char **) exec_environment);

    char **exvec =
        exec_run_sh((char *) command, options, exec_environment);

    /*
     * efree ((void *)command); command = NULL;
     */

    if ((child = efork()) < 0) {
        if (status)
            status->string = strerror(errno);
        return status_failed;
    } else if (child == 0) {
        disable_core_dumps();

        if (gid && (setgid(gid) == -1))
            perror("setting gid");
        if (uid && (setuid(uid) == -1))
            perror("setting uid");

        if (!(options & pexec_option_dont_close_stdin))
            close(0);

        close(1);
        if (!(options & pexec_option_nopipe)) {
            /*
             * unset this flag after fork()-ing 
             */
            fcntl(pipefderr[1], F_SETFD, 0);
            close(2);
            close(pipefderr[0]);
            dup2(pipefderr[1], 1);
            dup2(pipefderr[1], 2);
            close(pipefderr[1]);
        } else {
            dup2(2, 1);
        }

        execve(exvec[0], exvec, exec_environment);
    } else {
        FILE *fx;

        if (exec_environment)
            efree(exec_environment);
        if (exvec)
            efree(exvec);

        if (!(options & pexec_option_nopipe) && status) {
            /*
             * tag the fd as close-on-exec, just in case 
             */
            fcntl(pipefderr[1], F_SETFD, FD_CLOEXEC);
            close(pipefderr[1]);
            errno = 0;

            if ((fx = fdopen(pipefderr[0], "r"))) {
                char rxbuffer[BUFFERSIZE];
                setvbuf(fx, NULL, _IONBF, 0);

                if ((waitpid(child, &pidstatus, WNOHANG) == child)
                    && (WIFEXITED(pidstatus) || WIFSIGNALED(pidstatus))) {
                    have_waited = 1;
                } else
                    while (!feof(fx)) {
                        if (!fgets(rxbuffer, BUFFERSIZE, fx)) {
                            if (errno == EAGAIN) {
                                usleep(100);
                                goto skip_read;
                            }
                            break;
                        }

                        char **fbc = str2set('|', rxbuffer), orest = 1;
                        strtrim(rxbuffer);

                        if (fbc) {
                            if (strmatch(fbc[0], "feedback")) {
                                // suppose things are going fine until
                                // proven otherwise
                                cs = status_ok;

                                if (strmatch(fbc[1], "notice")) {
                                    orest = 0;
                                    fbprintf(status, "%s", fbc[2]);
                                } else if (strmatch(fbc[1], "success")) {
                                    orest = 0;
                                    cs = status_ok;
                                    fbprintf(status, "%s", fbc[2]);
                                } else if (strmatch(fbc[1], "failure")) {
                                    orest = 0;
                                    cs = status_failed;
                                    fbprintf(status, "%s", fbc[2]);
                                }
                            }

                            efree(fbc);
                        }

                        if (orest) {
                            fbprintf(status, "%s", rxbuffer);
                        }

                        continue;

                      skip_read:

                        if (waitpid(child, &pidstatus, WNOHANG) == child) {
                            if (WIFEXITED(pidstatus)
                                || WIFSIGNALED(pidstatus)) {
                                have_waited = 1;
                                break;
                            }
                        }
                    }

                efclose(fx);
            } else {
                perror("pexec(): open pipe");
            }
        }

        if (!have_waited) {
            do {
                waitpid(child, &pidstatus, 0);
            } while (!WIFEXITED(pidstatus) && !WIFSIGNALED(pidstatus));
        }
    }

    if (cs == status_failed)
        return status_failed;
    if (WIFEXITED(pidstatus) && (WEXITSTATUS(pidstatus) == EXIT_SUCCESS))
        return status_ok;
    return status_failed;
}

int start_daemon_f(struct dexecinfo *shellcmd, struct einit_event *status)
{
    pid_t child;
    uid_t uid;
    gid_t gid;

    if (!shellcmd)
        return status_failed;

    char *pidfile = NULL;
    if ((shellcmd->options & daemon_did_recovery) && shellcmd->pidfile
        && (pidfile = readfile(shellcmd->pidfile))) {
        pid_t pid = parse_integer(pidfile);

        efree(pidfile);
        pidfile = NULL;

        if (process_alive_p(pid)) {
            fbprintf(status,
                     "Module's PID-file already exists and is valid.");

            struct daemonst *new = ecalloc(1, sizeof(struct daemonst));
            new->starttime = time(NULL);
            new->dx = shellcmd;
            if (status)
                new->daemon_rid = status->rid;
            else
                new->daemon_rid = NULL;
            new->next = running;
            running = new;

            shellcmd->cb = new;
            new->pid = pid;

            return status_ok;
        }
    }

    /*
     * check if needed files are available 
     */
    if (shellcmd->need_files) {
        uint32_t r = 0;
        struct stat st;

        for (; shellcmd->need_files[r]; r++) {
            if (shellcmd->need_files[r][0] == '/') {
                if (stat(shellcmd->need_files[r], &st)) {
                    notice(4,
                           "can't bring up daemon \"%s\", because file \"%s\" does not exist.",
                           shellcmd->id ? shellcmd->id : "unknown",
                           shellcmd->need_files[r]);

                    return status_failed;
                }
            } else {
                char **w = which(shellcmd->need_files[r]);
                if (!w) {
                    notice(4,
                           "can't bring up daemon \"%s\", because executable \"%s\" does not exist.",
                           shellcmd->id ? shellcmd->id : "unknown",
                           shellcmd->need_files[r]);

                    return status_failed;
                } else {
                    efree(w);
                }
            }
        }
    }

    if (shellcmd->pidfile) {
        unlink(shellcmd->pidfile);
        errno = 0;
    }

    if (shellcmd->prepare) {
        // if (pexec (shellcmd->prepare, shellcmd->variables,
        // shellcmd->uid, shellcmd->gid, shellcmd->user, shellcmd->group,
        // shellcmd->environment, status) == status_failed) return
        // status_failed;
        if (pexec
            (shellcmd->prepare, (const char **) shellcmd->variables, 0, 0,
             NULL, NULL, shellcmd->environment, status) == status_failed)
            return status_failed;
    }
    // if ((status->task & einit_module_enable) && (!shellcmd ||
    // !shellcmd->command)) return status_failed;

    // if (status->task & einit_module_enable)
    // else return status_ok;


    uid = shellcmd->uid;
    gid = shellcmd->gid;

    lookupuidgid(&uid, &gid, shellcmd->user, shellcmd->group);

    if (shellcmd->options & daemon_model_forking) {
        int retval;

        retval =
            pexec_f(shellcmd->command, (const char **) shellcmd->variables,
                    uid, gid, shellcmd->user, shellcmd->group,
                    shellcmd->environment, status);

        if (retval == status_ok) {
            struct daemonst *new = ecalloc(1, sizeof(struct daemonst));
            new->starttime = time(NULL);
            new->dx = shellcmd;
            if (status)
                new->daemon_rid = status->rid;
            else
                new->daemon_rid = NULL;
            new->next = running;
            running = new;

            shellcmd->cb = new;

            shellcmd->pidfiles_last_update = 0;

            return status_ok;
        } else
            return status_failed;
    } else {
        struct daemonst *new = ecalloc(1, sizeof(struct daemonst));
        new->starttime = time(NULL);
        new->dx = shellcmd;
        if (status)
            new->daemon_rid = status->rid;
        else
            new->daemon_rid = NULL;

        shellcmd->cb = new;

        char **daemon_environment;

        daemon_environment =
            (char **) setcombine((const void **) einit_global_environment,
                                 (const void **) shellcmd->environment,
                                 SET_TYPE_STRING);
        daemon_environment =
            create_environment_f(daemon_environment,
                                 (const char **) shellcmd->variables);

        char *command = apply_envfile_f(shellcmd->command,
                                        (const char **)
                                        daemon_environment);

        char **exvec = exec_run_sh(command, 0, daemon_environment);

        /*
         * efree (command); command = NULL;
         */

        int cpipes[2];
        if (pipe(cpipes)) {
            notice(1, "tty.c: couldn't create an I/O pipe");
            return status_failed;
        }
        fcntl(cpipes[0], F_SETFD, FD_CLOEXEC);
        fcntl(cpipes[1], F_SETFD, FD_CLOEXEC);

#ifdef __linux__
        if ((child =
             syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL)) < 0) {
            if (status) {
                status->string = strerror(errno);
            }
            return status_failed;
        }
#else
      retry_fork:

        if ((child = fork()) < 0) {
            if (status) {
                status->string = strerror(errno);
            }

            goto retry_fork;
            return status_failed;
        }
#endif
        else if (child == 0) {
            /*
             * this 'ere is the code that gets executed in the child
             * process 
             */
            /*
             * let's fork /again/, so that the main einit monitor process
             * can pick these processes up 
             */
            close(cpipes[0]);

#ifdef __linux__
            pid_t cfork = syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL);    /* i 
                                                                                 * was 
                                                                                 * wrong 
                                                                                 * about 
                                                                                 * using 
                                                                                 * the 
                                                                                 * real 
                                                                                 * fork 
                                                                                 */
#else
            pid_t cfork = fork();
#endif

            switch (cfork) {
            case -1:
                close(cpipes[1]);
                _exit(-1);
                break;

            case 0:
                {
                    close(cpipes[1]);

                    disable_core_dumps();

                    if (gid && (setgid(gid) == -1))
                        perror("setting gid");
                    if (uid && (setuid(uid) == -1))
                        perror("setting uid");

                    close(1);
                    dup2(2, 1);

                    execve(exvec[0], exvec, daemon_environment);
                    exit(-1);
                    break;
                }
            default:
                /*
                 * exit and return the new child's PID 
                 */
                write(cpipes[1], &cfork, sizeof(pid_t));
                close(cpipes[1]);
                _exit(0);
                break;
            }
        } else {
            int rstatus;

            close(cpipes[1]);

            do {
                waitpid(child, &rstatus, 0);
            } while (!WIFEXITED(rstatus) && !WIFSIGNALED(rstatus));

            if (WIFSIGNALED(rstatus)) {
                fbprintf(status, "intermediate child process died");
                close(cpipes[0]);
                return status_failed;
            }

            pid_t realpid = WEXITSTATUS(rstatus);

            if (realpid < 0) {
                fbprintf(status,
                         "couldn't fork() to associate the child process with einit's monitor.");
                close(cpipes[0]);
                return status_failed;
            }

            while (read(cpipes[0], &realpid, sizeof(pid_t)) < 0);

            if (daemon_environment)
                efree(daemon_environment);
            if (exvec)
                efree(exvec);

            new->pid = realpid;

            new->next = running;
            running = new;
        }

        close(cpipes[0]);

        if (shellcmd->is_up) {
            return pexec(shellcmd->is_up,
                         (const char **) shellcmd->variables, 0, 0, NULL,
                         NULL, shellcmd->environment, status);
        }

        return status_ok;
    }
}

/*
 * TODO: port everything using startdaemon/stopdaemon to the new exec.h
 * code 
 */
int stop_daemon_f(struct dexecinfo *shellcmd, struct einit_event *status)
{
    return status_ok;
}

int einit_exec_configure(struct lmodule *irr)
{
    module_init(irr);

    if (!
        (shell =
         (char **) str2set(' ',
                           cfg_getstring("configuration-system-shell"))))
        shell = dshell;
    exec_configure(irr);

    int i = 0;

    if ((i = cfg_getinteger("configuration-system-daemon-spawn-timeout")))
        spawn_timeout = i;

    if ((i =
         cfg_getinteger
         ("configuration-system-daemon-term-timeout-primary")))
        kill_timeout_primary = i;

    if ((i =
         cfg_getinteger
         ("configuration-system-daemon-term-timeout-secondary")))
        kill_timeout_secondary = i;

    function_register("einit-execute-command", 1, pexec_f);
    function_register("einit-execute-command-in-main-loop", 1, eexec_f);
    function_register("einit-execute-daemon", 1, start_daemon_f);
    function_register("einit-stop-daemon", 1, stop_daemon_f);
    function_register("einit-create-environment", 1, create_environment_f);
    function_register("einit-check-variables", 1, check_variables_f);
    function_register("einit-apply-envfile", 1, apply_envfile_f);

    return 0;
}
