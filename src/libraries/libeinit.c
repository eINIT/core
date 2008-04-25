/*
 *  libeinit.c
 *  einit
 *
 *  Created by Magnus Deininger on 24/07/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2007-2008, Magnus Deininger All rights reserved.
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

#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <einit/event.h>
#include <einit/ipc.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>

#define DEFAULT_EINIT_ADDRESS "/dev/einit"

#ifdef __FreeBSD__
#include <signal.h>
#endif

#ifdef estrdup
#undef estrdup
#endif
#ifdef emalloc
#undef emalloc
#endif
#ifdef ecalloc
#undef ecalloc
#endif

#ifdef __APPLE__
/*
 * dammit, what's wrong with macos!? 
 */

struct exported_function *cfg_addnode_fs = NULL;
struct exported_function *cfg_findnode_fs = NULL;
struct exported_function *cfg_getstring_fs = NULL;
struct exported_function *cfg_getnode_fs = NULL;
struct exported_function *cfg_getpath_fs = NULL;
struct exported_function *cfg_prefix_fs = NULL;
struct exported_function *cfg_callback_prefix_fs = NULL;

char einit_new_node = 0;
struct utsname osinfo = { };
#endif

char **einit_initial_environment = NULL;
char **einit_global_environment = NULL;
char **einit_argv = NULL;

struct stree *exported_functions = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
enum einit_mode coremode = einit_mode_init;

void einit_power_down()
{
    einit_switch_mode("power-down");
}

void einit_power_reset()
{
    einit_switch_mode("power-reset");
}

void einit_switch_mode(const char *mode)
{                               /* think "runlevel" */
    einit_ipc_request ("switch-mode!", se_symbol(mode));
}

/*
 * client
 */

pid_t einit_ipc_client_pid = 0;

char einit_connect(int *argc, char **argv)
{
    const char *einit_ipc_address = NULL;
    int einit_ipc_socket = -1;
    char *envvar;
    char priv = 0;
    if ((envvar = getenv("EINIT_ADDRESS")))
        einit_ipc_address = envvar;

    if (argc && argv) {
        int i = 0;
        for (i = 1; i < *argc; i++) {
            if (argv[i][0] == '-')
                switch (argv[i][1]) {
                case 'p':
                    priv = 1;
                    break;
                case 'a':
                    if ((++i) < (*argc))
                        einit_ipc_address = argv[i];
                    break;
                case '-':
                    if (strmatch (argv[i], "--socket") && ((++i) < (*argc)))
                        einit_ipc_socket = parse_integer (argv[i]);
                }
        }
    }

    if (!einit_ipc_address || !einit_ipc_address[0])
        einit_ipc_address = DEFAULT_EINIT_ADDRESS;

    // einit_ipc_9p_fd = ixp_dial (einit_ipc_address);
    if (einit_ipc_socket > 0) {
        return einit_ipc_connect_socket(einit_ipc_socket);
    } else if (priv) {
        return einit_connect_spawn(argc, argv);
    } else {
        return einit_ipc_connect(einit_ipc_address);
    }
}

char einit_connect_spawn(int *argc, char **argv)
{
    char sandbox = 0;

    if (argc && argv) {
        int i = 0;
        for (i = 1; i < *argc; i++) {
            if (argv[i][0] == '-')
                switch (argv[i][1]) {
                case 'p':
                    if (argv[i][2] == 's')
                        sandbox = 1;
                    break;
                }
        }
    }

    int ipcsocket[2];

    socketpair (AF_UNIX, SOCK_STREAM, 0, ipcsocket);

    char socketstring[32];

    snprintf(socketstring, BUFFERSIZE, "%i", ipcsocket[0]);

    // int fd = 0;

    einit_ipc_client_pid = fork();

    switch (einit_ipc_client_pid) {
    case -1:
        return 0;
    case 0:
        /*
         * fd = open ("/dev/null", O_RDWR); if (fd) { close (0); close
         * (1); close (2);
         * 
         * dup2 (fd, 0); dup2 (fd, 1); dup2 (fd, 2);
         * 
         * close (fd); }
         */
        close (ipcsocket[1]);

        execl(EINIT_LIB_BASE "/bin/einit-core", "einit-core", "--ipc-socket",
              socketstring, (sandbox ? "--sandbox" : NULL), NULL);

        exit(EXIT_FAILURE);
    default:
        close (ipcsocket[0]);

        return einit_ipc_connect_socket(ipcsocket[1]);
    }
}

char einit_disconnect()
{
    if (einit_ipc_client_pid > 0) {
        /*
         * we really gotta do this in a cleaner way... 
         */
        kill(einit_ipc_client_pid, SIGKILL);

        waitpid(einit_ipc_client_pid, NULL, 0);
    }

    close (einit_ipc_get_fd());

    return 1;
}

void einit_service_call(const char *service, const char *action)
{
    einit_ipc_request ("service-do!", se_cons(se_symbol(service),
                       se_cons (se_symbol(action),
                       (struct einit_sexp *)sexp_end_of_list)));
}

void einit_module_call(const char *rid, const char *action)
{

    einit_ipc_request ("module-do!", se_cons(se_symbol(rid),
                       se_cons (se_symbol(action),
                       (struct einit_sexp *)sexp_end_of_list)));
}

struct lmodule *einit_get_core_module_descriptor (const char *rid)
{
    return einit_decode_lmodule_from_sexpr(einit_ipc_request ("get-module", se_symbol(rid)));
}

char *einit_module_get_name(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        char *rv = lm->module->name;
        einit_destroy_core_module_descriptor (lm);

        return rv;
    } else {
        return NULL;
    }
}

char **einit_module_get_provides(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        char **rv = lm->si ? lm->si->provides : NULL;
        einit_destroy_core_module_descriptor (lm);

        return rv;
    } else {
        return NULL;
    }
}

char **einit_module_get_requires(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        char **rv = lm->si ? lm->si->requires : NULL;
        einit_destroy_core_module_descriptor (lm);

        return rv;
    } else {
        return NULL;
    }
}

char **einit_module_get_after(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        char **rv = lm->si ? lm->si->after : NULL;
        einit_destroy_core_module_descriptor (lm);

        return rv;
    } else {
        return NULL;
    }
}

char **einit_module_get_before(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        char **rv = lm->si ? lm->si->before : NULL;
        einit_destroy_core_module_descriptor (lm);

        return rv;
    } else {
        return NULL;
    }
}

char **einit_module_get_status(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        einit_destroy_core_module_descriptor (lm);

        return NULL;
    } else {
        return NULL;
    }
}

char **einit_module_get_options(const char *rid)
{
    struct lmodule *lm = einit_get_core_module_descriptor (rid);

    if (lm) {
        einit_destroy_core_module_descriptor (lm);

        return NULL;
    } else {
        return NULL;
    }
}

void einit_event_loop()
{
    einit_ipc_request ("receive-events", se_symbol("backlog"));
    einit_ipc_loop_infinite();
}

void einit_event_loop_skip_old()
{
    einit_ipc_request ("receive-events", se_symbol("no-backlog"));
    einit_ipc_loop_infinite();
}

void einit_replay_events()
{
    einit_ipc_request ("receive-events", se_symbol("replay-only"));
}

/*
 * (define-record einit:event type integer status task flag string
 * stringset module)
 */

const char *einit_event_encode(struct einit_event *ev)
{
    struct einit_sexp *sp = 
        se_cons(se_symbol ("event"),
        se_cons(se_symbol (event_code_to_string(ev->type)),
        se_cons(se_integer(ev->integer),
        se_cons(se_integer(ev->status),
        se_cons(se_integer(ev->task),
        se_cons(se_integer(ev->flag),
        se_cons(se_string(ev->string),
        se_cons(se_stringset_to_list(ev->stringset),
        se_cons(se_symbol(ev->rid),
                (struct einit_sexp *)sexp_end_of_list)))))))));

    char *r = einit_sexp_to_string(sp);
    const char *rv = str_stabilise (r);
    efree (r);

    einit_sexp_destroy(sp);

    return rv;
}

/*
 * (request register-module (rid "name" (provides) (requires) (before) (after)
 *   (uses) run-once deprecated))
 * (reply register-module <#t/#f>)
 */

enum sexp_module_parsing_stage {
    smps_rid,
    smps_name,
    smps_provides,
    smps_requires,
    smps_before,
    smps_after,
    smps_uses,
    smps_run_once,
    smps_deprecated,
    smps_done
};

struct smodule *einit_decode_module_from_sexpr(struct einit_sexp *sexp)
{
    struct smodule *sm = NULL;

    enum sexp_module_parsing_stage s = smps_rid;

    while ((s != smps_done) && (sexp->type == es_cons)) {
        struct einit_sexp *p = sexp->primus;

        switch (s) {
            case smps_rid:
                if (p->type == es_symbol) {
                    sm = emalloc(sizeof (struct smodule));
                    memset (sm, 0, sizeof (struct smodule));

                    sm->rid = (char*)(p->symbol);
                } else {
                    return NULL;
                }
                break;

            case smps_name:
                if (p->type == es_string) {
                    sm->name = (char*)(p->string);
                } else {
                    efree (sm);
                    return NULL;
                }
                break;

            case smps_provides:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_symbol) {
                        sm->si.provides =
                                set_str_add_stable(sm->si.provides,
                                (char *) pp->symbol);
                    } else {
                        efree (sm);
                        return NULL;
                    }

                    p = p->secundus;
                }
                break;

            case smps_requires:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_symbol) {
                        sm->si.requires =
                                set_str_add_stable(sm->si.requires,
                                (char *) pp->symbol);
                    } else {
                        efree (sm);
                        return NULL;
                    }

                    p = p->secundus;
                }
                break;

            case smps_before:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_string) {
                        sm->si.before =
                                set_str_add_stable(sm->si.before,
                                (char *) pp->string);
                    } else {
                        efree (sm);
                        return NULL;
                    }

                    p = p->secundus;
                }
                break;

            case smps_after:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_string) {
                        sm->si.after =
                                set_str_add_stable(sm->si.after,
                                (char *) pp->string);
                    } else {
                        efree (sm);

                        return NULL;
                    }

                    p = p->secundus;
                }
                break;

            case smps_uses:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_symbol) {
                        sm->si.uses =
                                set_str_add_stable(sm->si.uses,
                                (char *) pp->symbol);
                    } else {
                        efree (sm);

                        return NULL;
                    }

                    p = p->secundus;
                }
                break;

            case smps_run_once:
                if (p == sexp_true)
                    sm->mode |= einit_feedback_job;
                break;

            case smps_deprecated:
                if (p == sexp_true)
                    sm->mode |= einit_module_deprecated;
                break;

            case smps_done:
            default:
                break;
        }

        s++;
        sexp = sexp->secundus;
    }

    if (sm) {
        sm->mode |= einit_module | einit_module_event_actions;
    }

    return sm;
}

struct lmodule *einit_decode_lmodule_from_sexpr(struct einit_sexp *sexp)
{
    struct smodule *sm = einit_decode_module_from_sexpr (sexp);

    if (sm) {
        struct lmodule *lm = ecalloc (1, sizeof (struct lmodule));
        lm->module = sm;

        struct einit_sexp *p =
                se_cdr (se_cdr (se_cdr (se_cdr (se_cdr (se_cdr (se_cdr (se_cdr (se_cdr (sexp)))))))));

        struct einit_sexp *status = se_car(p);
        struct einit_sexp *actions = se_car(se_cdr(p));

        /* TODO : parse status and actions */

        if (sm->si.provides || sm->si.requires || sm->si.after || sm->si.before || sm->si.uses) {
            lm->si = emalloc (sizeof (*(lm->si)));

            lm->si->provides = sm->si.provides ? sm->si.provides : NULL;
            lm->si->requires = sm->si.requires ? sm->si.requires : NULL;
            lm->si->after = sm->si.after ? sm->si.after : NULL;
            lm->si->before = sm->si.before ? sm->si.before : NULL;
            lm->si->uses = sm->si.uses ? sm->si.uses : NULL;
        }

        return lm;
    } else {
        fprintf (stderr, "failed to parse module: %s\n", einit_sexp_to_string(sexp));

        return NULL;
    }
}

void einit_destroy_core_module_descriptor (struct lmodule *lm)
{
    if (lm->si) efree (lm->si);
    if (lm->module) efree ((void *)(lm->module));
    efree (lm);
}


void einit_register_module(struct smodule *s)
{
    struct einit_sexp *sp = 
                se_cons(se_symbol (s->rid),
                se_cons(se_string (s->name),
                se_cons(se_symbolset_to_list(s->si.provides),
                se_cons(se_symbolset_to_list(s->si.requires),
                se_cons(se_stringset_to_list(s->si.before),
                se_cons(se_stringset_to_list(s->si.after),
                se_cons(se_symbolset_to_list(s->si.uses),
                se_cons((struct einit_sexp *)
                          ((s->mode & einit_feedback_job) ?
                             sexp_true : sexp_false),
                se_cons((struct einit_sexp *)
                          ((s->mode & einit_module_deprecated) ?
                             sexp_true : sexp_false),
                        (struct einit_sexp *)sexp_end_of_list)))))))));

    einit_ipc_request ("register-module", sp);
}

char *einit_get_configuration_string(const char *key,
                                     const char *attribute)
{
    char *rv = NULL;
    if (!attribute) attribute = "s";

    struct einit_sexp *s =
            einit_ipc_request("get-configuration", se_cons(se_symbol (key),
                              se_cons (se_symbol (attribute),
                              (struct einit_sexp *)sexp_end_of_list)));

    if (s) {
        if (s->type == es_string)
        {
            rv = (char *)s->string;
        }

        einit_sexp_destroy (s);
    }

    return rv;
}

signed int einit_get_configuration_integer(const char *key,
                                           const char *attribute)
{
    char *s =
        einit_get_configuration_string(key, attribute ? attribute : "i");

    if (s)
        return parse_integer(s);

    return 0;
}

char einit_get_configuration_boolean(const char *key,
                                     const char *attribute)
{
    char *s =
        einit_get_configuration_string(key, attribute ? attribute : "b");

    if (s)
        return parse_boolean(s);

    return 0;
}

static char **sexp2arbattrs(char **rv, struct einit_sexp *s)
{
    if (s) {
        struct einit_sexp *p = s;
        while (p->type == es_cons) {
            struct einit_sexp *primus = se_car(se_car(p));
            struct einit_sexp *secundus = se_car(se_cdr(se_car(p)));

            if ((primus->type == es_symbol) && (secundus->type == es_string)) {
                rv = set_str_add_stable (rv, (char*)primus->symbol);
                rv = set_str_add_stable (rv, (char*)secundus->string);
            }

            p = p->secundus;
        }
    }

    return rv;
}

char **einit_get_configuration_attributes(const char *key)
{
    struct einit_sexp *s = einit_ipc_request ("get-configuration-multi",
                                              se_symbol(key));

    char **rv = NULL;

    rv = sexp2arbattrs(rv, s);
    if (s) einit_sexp_destroy (s);

    return rv;
}

char ***einit_get_configuration_prefix(const char *prefix)
{
    struct einit_sexp *s = einit_ipc_request ("get-configuration*",
                                              se_symbol(prefix));
    char ***rv = NULL;

    if (s) {
        struct einit_sexp *p = s;
        while (p->type == es_cons) {
            struct einit_sexp *primus = se_car(se_car(p));
            struct einit_sexp *secundus = se_car(se_cdr(se_car(p)));

            if (primus->type == es_symbol) {
                char **e = set_str_add_stable (NULL, (char *)primus->symbol);
                e = sexp2arbattrs(e, secundus);

                rv = (char***)set_noa_add((void**)rv, e);
            }

            p = p->secundus;
        }

        einit_sexp_destroy (s);
    }

    return rv;
}

char **einit_list (char *r)
{
    struct einit_sexp *s = einit_ipc_request ("list", se_symbol(r));
    struct einit_sexp *p = s;
    char ** rv = NULL;

    if (!s) return NULL;

    while (p->type == es_cons) {
        struct einit_sexp *pp = p->primus;

        if (pp->type == es_symbol) {
            rv = set_str_add_stable(rv, (char *) pp->symbol);
        } else {
            efree (rv);
            einit_sexp_destroy (s);

            return NULL;
        }

        p = p->secundus;
    }

    einit_sexp_destroy (s);

    return rv;
}

char **einit_list_modules ()
{
    return einit_list("modules");
}

char **einit_list_services ()
{
    return einit_list("services");
}
