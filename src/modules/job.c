/*
 *  job.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/09/2006.
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/sexp.h>
#include <einit/exec.h>
#include <einit/ipc.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_job_configure(struct lmodule *);

const struct smodule einit_job_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "eINIT Job and Server Support",
    .rid = "einit-job",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
           .configure = einit_job_configure
};

module_register(einit_job_self);

struct einit_job {
    const char *name;
    char **run_on;
    struct lmodule *module;
};

struct einit_server {
    const char *name;
    char **need_files;
    char **environment;
    struct lmodule *module;
};

struct stree *einit_jobs = NULL;
struct stree *einit_servers = NULL;

int einit_jobs_running = 0;

char **einit_servers_running = NULL;

void einit_servers_synchronise();

void einit_job_add_or_update (struct einit_sexp *s)
{
    einit_sexp_display (s);

    struct einit_sexp *primus = se_car (s);
    struct einit_sexp *secundus = se_car (se_cdr(s));
    struct einit_sexp *tertius = se_car (se_cdr(se_cdr(s)));
    struct einit_sexp *rest = se_cdr(se_cdr(se_cdr(s)));

    if ((primus->type == es_symbol) && strmatch (primus->symbol, "job") &&
        (secundus->type == es_symbol) && (tertius->type == es_string)) {
        struct einit_job j = { tertius->string, NULL, NULL };
        char buffer[BUFFERSIZE];
        const char *binary;

        snprintf (buffer, BUFFERSIZE, "job-%s", secundus->symbol);
        binary = str_stabilise(buffer);

        while (rest->type == es_cons) {
            primus = se_car(se_car(rest));
            secundus = se_cdr(se_car(rest));

            if (primus->type == es_symbol) {
                if (strmatch(primus->symbol, "run-on")) {
                    while (secundus->type == es_cons) {
                        primus = se_car (secundus);

                        if (primus->type == es_symbol) {
                            j.run_on = (char **)set_noa_add
                                    ((void **)j.run_on, (void *)primus->symbol);
                        }

                        secundus = se_cdr (secundus);
                    }
                }
            }

            rest = se_cdr (rest);
        }

        struct lmodule *lm;
        snprintf (buffer, BUFFERSIZE, "j-%s", binary);

        if (!(lm = mod_lookup_rid (buffer))) {
            struct smodule *sm = ecalloc (1, sizeof (struct smodule));

            sm->name = (char *)j.name;
            sm->rid = (char *)str_stabilise (buffer);

            lm = mod_add_or_update(NULL, sm, substitue_and_prune);
        }

        if (lm) {
            j.module = lm;
        }

        if (einit_jobs) {
            struct stree *st = streefind (einit_jobs, binary, tree_find_first);
            if (st) {
                efree (st->luggage);
                st->luggage = j.run_on;
                memcpy (st->value, &j, sizeof (struct einit_job));
                return;
            }
        }

        einit_jobs = streeadd(einit_jobs, binary, &j, sizeof (struct einit_job),
                              j.run_on);
    } else if ((primus->type == es_symbol) &&
                strmatch (primus->symbol, "server") &&
                (secundus->type == es_symbol) && (tertius->type == es_string)) {
        struct einit_server s = { tertius->string, NULL, NULL };
        char buffer[BUFFERSIZE];
        const char *binary;

        snprintf (buffer, BUFFERSIZE, "server-%s", secundus->symbol);
        binary = str_stabilise(buffer);

        while (rest->type == es_cons) {
            primus = se_car(se_car(rest));
            secundus = se_cdr(se_car(rest));

            if (primus->type == es_symbol) {
                if (strmatch(primus->symbol, "need-files")) {
                    while (secundus->type == es_cons) {
                        primus = se_car (secundus);

                        if (primus->type == es_string) {
                            s.need_files = (char **)set_noa_add
                                    ((void **)s.need_files, (void *)primus->string);
                        }

                        secundus = se_cdr (secundus);
                    }
                } else if (strmatch(primus->symbol, "environment")) {
                    while (secundus->type == es_cons) {
                        primus = se_car (secundus);

                        if (primus->type == es_string) {
                            s.environment = (char **)set_noa_add
                                    ((void **)s.environment, (void *)primus->string);
                        }

                        secundus = se_cdr (secundus);
                    }
                }

            }

            rest = se_cdr (rest);
        }

        struct lmodule *lm;

        if (!(lm = mod_lookup_rid (binary))) {
            struct smodule *sm = ecalloc (1, sizeof (struct smodule));

            sm->name = (char *)s.name;
            sm->rid = (char *)binary;

            lm = mod_add_or_update(NULL, sm, substitue_and_prune);
        }

        if (lm) {
            s.module = lm;
        }

        if (einit_servers) {
            struct stree *st = streefind (einit_servers, binary, tree_find_first);
            if (st) {
                struct einit_server *os = st->value;
                if (os->need_files) efree (os->need_files);
                if (os->environment) efree (os->environment);

                memcpy (st->value, &s, sizeof (struct einit_server));
                return;
            }
        }

        einit_servers = streeadd(einit_servers, binary, &s,
                                 sizeof (struct einit_server), NULL);
    }
}

void einit_jobs_update()
{
    char **files =
            readdirfilter(NULL, EINIT_LIB_BASE "/jobs", "\\.sexpr$", NULL, 0);

    if (files) {
        int i = 0;
        for (; files[i]; i++) {
            int fd = open (files[i], O_RDONLY);
            if (fd < 0) continue;

            struct einit_sexp_fd_reader *r = einit_create_sexp_fd_reader(fd);
            struct einit_sexp *s;

            while ((s = einit_read_sexp_from_fd_reader(r)) != sexp_bad) {
                if (s) {
                    einit_job_add_or_update(s);
                }
            }
        }
    }

    einit_servers_synchronise();
}

void einit_job_run_dead_process_callback (struct einit_exec_data *d)
{
    einit_jobs_running--;

    if (!einit_jobs_running) {
        struct einit_event ev = evstaticinit(einit_job_all_done);
        event_emit (&ev, 0);
        evstaticdestroy (ev);
    }
}

void einit_job_run(struct einit_job *j, const char *binary, const char *event)
{
    notice (1, "running job \"%s\" for event \"%s\"", j->name, event);

    char *path = joinpath (EINIT_LIB_BASE "/bin/", (char *)binary);
    if (path) {
        char *rpath = path;
        struct stat st;
        int ipcsocket[2];
        char sstring[33];

        if (coremode & einit_mode_sandbox) rpath++;

        if (stat (rpath, &st)) {
            notice (1,
                    "failed to run job \"%s\" for event \"%s\": missing "
                    "binary (%s)", j->name, event, binary);
            return;
        }

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipcsocket) == -1) {
            return;
        }

        fcntl(ipcsocket[1], F_SETFD, FD_CLOEXEC);

        snprintf (sstring, 33, "%i", ipcsocket[0]);

        char *cmd[] = { rpath, (char *)event, "--socket", sstring, NULL, NULL };

        if (coremode & einit_mode_sandbox) {
            cmd[4] = "--sandbox";
        }

        einit_exec_without_shell_with_function_on_process_death(cmd,
                einit_job_run_dead_process_callback, j->module);

        einit_ipc_connect_client (ipcsocket[1]);
        einit_jobs_running++;

        efree (path);
    }
}

void einit_server_run_dead_process_callback (struct einit_exec_data *d)
{
    einit_servers_running =
            strsetdel (einit_servers_running, d->module->module->rid);
}

void einit_server_run(struct einit_server *s, const char *binary)
{
    notice (1, "running server \"%s\"", s->name);

    char *path = joinpath (EINIT_LIB_BASE "/bin/", (char *)binary);
    if (path) {
        char *rpath = path;
        struct stat st;
        int ipcsocket[2];
        char sstring[33];

        if (coremode & einit_mode_sandbox) rpath++;

        if (stat (rpath, &st)) {
            notice (1,
                    "failed to run server \"%s\": missing "
                            "binary (%s)", s->name, binary);
            return;
        }

        if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipcsocket) == -1) {
            return;
        }

        fcntl(ipcsocket[1], F_SETFD, FD_CLOEXEC);

        snprintf (sstring, 33, "%i", ipcsocket[0]);

        char *cmd[] = { rpath, "--socket", sstring, NULL, NULL };

        if (coremode & einit_mode_sandbox) {
            cmd[3] = "--sandbox";
        }

        struct einit_exec_data *x = ecalloc(1, sizeof(struct einit_exec_data));

        x->command_d = cmd;
        x->options = einit_exec_no_shell;
        x->module = s->module;
        x->handle_dead_process = einit_server_run_dead_process_callback;
        x->environment = s->environment;

        einit_exec(x);

        einit_ipc_connect_client (ipcsocket[1]);

        einit_servers_running =
                set_str_add_stable (einit_servers_running, (char *)binary);

        efree (path);
    }
}

void einit_jobs_run(const char *event)
{
    struct stree *st = streelinear_prepare (einit_jobs);
    while (st) {
        struct einit_job *j = st->value;

        if (j->run_on &&
            inset ((const void **)j->run_on, event, SET_TYPE_STRING)) {
            einit_job_run (j, st->key, event);
        }

        st = streenext (st);
    }
}

void einit_servers_synchronise()
{
    struct stree *st = streelinear_prepare (einit_servers);
    while (st) {
        struct einit_server *s = st->value;

        if (!inset ((const void **)einit_servers_running, st->key,
                    SET_TYPE_STRING) && 
            (!s->need_files || check_files(s->need_files))) {
            einit_server_run (s, st->key);
        }

        st = streenext (st);
    }
}

int einit_job_configure(struct lmodule *irr)
{
    module_init(irr);

    event_listen(einit_core_update_modules, einit_jobs_update);
    event_listen(einit_core_update_modules, einit_jobs_update);

    einit_jobs_update();
    einit_jobs_run("core-initialisation");

    return 1;
}
