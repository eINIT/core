/*
 *  ipc-library.c
 *  einit
 *
 *  Created by Magnus Deininger on 13/04/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2008, Magnus Deininger All rights reserved.
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
#include <errno.h>
#include <string.h>

#include <fcntl.h>

#include <einit/ipc.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int einit_ipc_library_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule einit_ipc_library_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "IPC Library",
    .rid = "einit-ipc-library",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = einit_ipc_library_configure
};

module_register(einit_ipc_library_self);

#endif

void einit_ipc_reply_simple(int id, char *s, struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;
    char buffer[BUFFERSIZE];

    snprintf (buffer, BUFFERSIZE, "(%i %s)", id, s);

    int len = strlen(buffer);

    fcntl(reader->fd, F_SETFL, 0);
    int r = write (reader->fd, buffer, len);
    fcntl(reader->fd, F_SETFL, O_NONBLOCK);

    if ((r < 0) || (r == 0)) {
        fprintf (stderr, "COULDN'T WRITE REPLY!\n");
    }

    if (r < len) {
        fprintf (stderr, "SHORT WRITE!\n");
    }
}

void einit_ipc_library_stub(struct einit_sexp *sexp, int id,
                            struct einit_ipc_connection *cd)
{
    char *r = einit_sexp_to_string(sexp);
    fprintf(stderr, "IPC STUB: %s\n", r);

    efree(r);

    einit_ipc_reply_simple (id, "stub", cd);
}

/* TODO: allow the clients to tell us what events it should receive */
void einit_ipc_library_receive_events(struct einit_sexp *sexp, int id,
                                      struct einit_ipc_connection *cd)
{
    if (sexp->type == es_symbol) {
        if (strmatch (sexp->symbol, "replay-only")
            || strmatch (sexp->symbol, "backlog")) {
            cd->current_event = 0;

            einit_ipc_update_event_listeners ();

            if (strmatch (sexp->symbol, "replay-only")) {
                cd->current_event = -1;
            }
        } else if (strmatch (sexp->symbol, "no-backlog")) {
            for (cd->current_event = 0;
                 einit_event_backlog[cd->current_event];
                 cd->current_event++) ;
        } else {
            goto bad_request;
        }

        einit_ipc_reply_simple (id, "#t", cd);
    } else {
        bad_request:

        einit_ipc_reply_simple (id, "#f", cd);
    }
}

void einit_ipc_library_receive_specific_events(struct einit_sexp *sexp, int id,
                                               struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, id, cd);
}

void einit_ipc_library_mute_specific_events(struct einit_sexp *sexp, int id,
                                            struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, id, cd);
}

void einit_ipc_library_get_configuration(struct einit_sexp *sexp, int id,
                                         struct einit_ipc_connection *cd)
{
    struct einit_sexp *primus = se_car(sexp);
    struct einit_sexp *secundus = se_car(se_cdr(sexp));

    if ((primus->type == es_symbol) && (secundus->type == es_symbol)) {
        struct cfgnode *n = cfg_getnode (primus->symbol, NULL);
        char *value = NULL;

        if (n && n->arbattrs) {
            int i = 0;
            for (; n->arbattrs[i]; i+=2) {
                if (strmatch (n->arbattrs[i], secundus->symbol)) {
                    value = n->arbattrs[i+1];
                    break;
                }
            }
        }

        if (value) {
            struct einit_sexp_fd_reader *reader = cd->reader;

            struct einit_sexp *sp = 
                    se_cons(se_integer (id),
                    se_cons(se_string(value),
                            (struct einit_sexp *)sexp_end_of_list));

            char *r = einit_sexp_to_string(sp);

            einit_sexp_destroy(sp);

            fcntl(reader->fd, F_SETFL, 0);
            write (reader->fd, r, strlen(r));
            fcntl(reader->fd, F_SETFL, O_NONBLOCK);

            efree (r);

            return;
        }
    }

    einit_ipc_reply_simple (id, "#f", cd);
}

static struct einit_sexp *cfgnode2sexp (struct cfgnode *n)
{
    if (n && n->arbattrs) {
        int i = 0;
        struct einit_sexp *sp = (struct einit_sexp *)sexp_end_of_list;
        for (; n->arbattrs[i]; i+=2) {
            struct einit_sexp *v = se_cons (se_symbol (n->arbattrs[i]),
                                            se_cons (se_string (n->arbattrs[i+1]),
                                                    (struct einit_sexp *)sexp_end_of_list));

            sp = se_cons (v, sp);
        }

        if (sp == sexp_end_of_list)
            sp = (struct einit_sexp *)sexp_false;

        return sp;
    }

    return (struct einit_sexp*)sexp_false;
}

void einit_ipc_library_get_configuration_multi(struct einit_sexp *sexp, int id,
                                               struct einit_ipc_connection *cd)
{
    if (sexp->type == es_symbol) {
        struct einit_sexp_fd_reader *reader = cd->reader;
        struct cfgnode *n = cfg_getnode (sexp->symbol, NULL);

        struct einit_sexp *sp = cfgnode2sexp(n);

        sp = se_cons(se_integer (id), se_cons (sp, (struct einit_sexp *)sexp_end_of_list));

        char *r = einit_sexp_to_string(sp);

        einit_sexp_destroy(sp);

        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, r, strlen(r));
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);

        efree (r);
        return;
    }

    einit_ipc_reply_simple (id, "#f", cd);
}

void einit_ipc_library_get_configuration_a(struct einit_sexp *sexp, int id,
                                           struct einit_ipc_connection *cd)
{
    if (sexp->type == es_symbol) {
        struct einit_sexp_fd_reader *reader = cd->reader;
        struct stree *st = cfg_prefix (sexp->symbol);

        if (st) {
            struct einit_sexp *sp = (struct einit_sexp *)sexp_end_of_list;

            st = streelinear_prepare (st);

            while (st) {
                sp = se_cons (
                       se_cons(se_symbol (st->key),
                         se_cons (cfgnode2sexp (st->value),
                         (struct einit_sexp *)sexp_end_of_list)),
                       sp);

                st = streenext(st);
            }

            sp = se_cons(se_integer (id), se_cons (sp, (struct einit_sexp *)sexp_end_of_list));

            char *r = einit_sexp_to_string(sp);

            einit_sexp_destroy(sp);

            fcntl(reader->fd, F_SETFL, 0);
            write (reader->fd, r, strlen(r));
            fcntl(reader->fd, F_SETFL, O_NONBLOCK);

            efree (r);

            return;
        }
    }

    einit_ipc_reply_simple (id, "#f", cd);
}

void einit_ipc_library_register_module(struct einit_sexp *sexp, int id,
                                       struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, id, cd);
}

void einit_ipc_library_register_module_actions(struct einit_sexp *sexp, int id,
                                               struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, id, cd);
}

void einit_ipc_library_list(struct einit_sexp *sexp, int id,
                            struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    if (sexp->type == es_symbol) {
        char **l = NULL;

        if (strmatch (sexp->symbol, "modules")) {
            l = mod_list_all_available_modules();
        }

        if (strmatch (sexp->symbol, "services")) {
            l = mod_list_all_available_services();
        }

        if (l) {
            char *m = set2str (' ', (const char **)l);

            if (m) {
                int size = strlen (m) + 38; /* 32 for the int, just in case */
                char buffer[size];

                snprintf (buffer, size, "(%i (%s))", id, m);

                fcntl(reader->fd, F_SETFL, 0);
                write (reader->fd, buffer, strlen(buffer));
                fcntl(reader->fd, F_SETFL, O_NONBLOCK);

                return;
            }
        }
    }

    einit_ipc_reply_simple (id, "#f", cd);
}

/*
 * (request get-module rid)
 * (reply get-module (rid "name" (provides) (requires) (before) (after) (uses)
 *  run-once deprecated (status) (actions)))
 */

void einit_ipc_library_get_module(struct einit_sexp *sexp, int id,
                                  struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    if (sexp->type == es_symbol) {
        struct lmodule *lm = mod_lookup_rid (sexp->symbol);

        if (!lm || !lm->module) goto fail;

        struct einit_sexp *sp = 
            se_cons(se_integer (id),
            se_cons(se_cons(se_symbol (lm->module->rid),
                se_cons(se_string (lm->module->name),
                se_cons(se_symbolset_to_list(lm->si ? lm->si->provides : NULL),
                se_cons(se_symbolset_to_list(lm->si ? lm->si->requires : NULL),
                se_cons(se_stringset_to_list(lm->si ? lm->si->before : NULL),
                se_cons(se_stringset_to_list(lm->si ? lm->si->after : NULL),
                se_cons(se_symbolset_to_list(lm->si ? lm->si->uses : NULL),
                se_cons((struct einit_sexp *)
                          ((lm->module->mode & einit_feedback_job) ?
                             sexp_true : sexp_false),
                se_cons((struct einit_sexp *)
                          ((lm->module->mode & einit_module_deprecated) ?
                             sexp_true : sexp_false),
                se_cons((struct einit_sexp *)sexp_end_of_list,
                se_cons((struct einit_sexp *)sexp_end_of_list,
                        (struct einit_sexp *)sexp_end_of_list))))))))))),
                    (struct einit_sexp *)sexp_end_of_list));

        char *r = einit_sexp_to_string(sp);

//        fprintf (stderr, "reply: %s\n", r);

        einit_sexp_destroy(sp);

        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, r, strlen(r));
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);

        efree (r);
    } else {
        fail:

        einit_ipc_reply_simple (id, "#f", cd);
    }
}

void einit_ipc_library_get_service(struct einit_sexp *sexp, int id,
                                   struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, id, cd);
}

void einit_ipc_library_module_do_bang(struct einit_sexp *sexp, int id,
                                      struct einit_ipc_connection *cd)
{
    struct einit_sexp *primus = se_car(sexp),
    *secundus = se_car(se_cdr(sexp));

    if ((primus->type == es_symbol) && (secundus->type == es_symbol)) {
        einit_ipc_reply_simple (id, "#t", cd);

        struct lmodule *lm = mod_lookup_rid (primus->symbol);

        if (strmatch (secundus->symbol, "enable")) {
            mod (einit_module_enable, lm, NULL);
        } else if (strmatch (secundus->symbol, "disable")) {
            mod (einit_module_disable, lm, NULL);
        } else {
            mod (einit_module_custom, lm, (char*)(secundus->symbol));
        }
    } else {
        einit_ipc_reply_simple (id, "#f", cd);
    }
}

void einit_ipc_library_service_do_bang(struct einit_sexp *sexp, int id,
                                       struct einit_ipc_connection *cd)
{
    struct einit_sexp *primus = se_car(sexp),
    *secundus = se_car(se_cdr(sexp));

    if ((primus->type == es_symbol) && (secundus->type == es_symbol)) {
        einit_ipc_reply_simple (id, "#t", cd);

        struct einit_event ev = evstaticinit (einit_core_change_service_status);
        ev.rid = (char*)(primus->symbol);
        ev.string = (char*)(secundus->symbol);
        event_emit (&ev, 0);
    } else {
        einit_ipc_reply_simple (id, "#f", cd);
    }
}

void einit_ipc_library_service_switch_mode(struct einit_sexp *sexp, int id,
                                           struct einit_ipc_connection *cd)
{
    if (sexp->type == es_symbol) {
        einit_ipc_reply_simple (id, "#t", cd);

        struct einit_event ev = evstaticinit (einit_core_switch_mode);
        ev.string = (char*)(sexp->symbol);
        event_emit (&ev, 0);
    } else {
        einit_ipc_reply_simple (id, "#t", cd);
    }
}

int einit_ipc_library_configure(struct lmodule *irr)
{
    module_init(irr);

    einit_ipc_register_handler("receive-events",
                               einit_ipc_library_receive_events);
    einit_ipc_register_handler("receive-specific-events",
                               einit_ipc_library_receive_specific_events);
    einit_ipc_register_handler("mute-specific-events",
                               einit_ipc_library_mute_specific_events);
    einit_ipc_register_handler("get-configuration",
                               einit_ipc_library_get_configuration);
    einit_ipc_register_handler("get-configuration-multi",
                               einit_ipc_library_get_configuration_multi);
    einit_ipc_register_handler("get-configuration*",
                               einit_ipc_library_get_configuration_a);
    einit_ipc_register_handler("register-module",
                               einit_ipc_library_register_module);
    einit_ipc_register_handler("register-module-actions",
                               einit_ipc_library_register_module_actions);
    einit_ipc_register_handler("list", einit_ipc_library_list);
    einit_ipc_register_handler("get-module", einit_ipc_library_get_module);
    einit_ipc_register_handler("get-service",
                               einit_ipc_library_get_service);
    einit_ipc_register_handler("module-do!",
                               einit_ipc_library_module_do_bang);
    einit_ipc_register_handler("service-do!",
                               einit_ipc_library_service_do_bang);
    einit_ipc_register_handler("switch-mode!",
                               einit_ipc_library_service_switch_mode);

    return 0;
}
