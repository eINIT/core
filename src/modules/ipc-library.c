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

#define BAD_REQUEST "(reply unknown stub)"
#define BAD_REQUEST_SIZE sizeof(BAD_REQUEST)

void einit_ipc_library_stub(struct einit_sexp *sexp,
                            struct einit_ipc_connection *cd)
{
    char *r = einit_sexp_to_string(sexp);
    fprintf(stderr, "IPC STUB: %s\n", r);

    efree(r);

    write(cd->reader->fd, BAD_REQUEST, BAD_REQUEST_SIZE);
}

/* TODO: allow the clients to tell us what events it should receive */
void einit_ipc_library_receive_events(struct einit_sexp *sexp,
                                      struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    if (sexp->type == es_symbol) {
        if (strmatch (sexp->symbol, "replay-only")
            || strmatch (sexp->symbol, "backlog")) {
            for (cd->current_event = 0;
                 einit_event_backlog[cd->current_event];
                 cd->current_event++) {

                int len = strlen (einit_event_backlog[cd->current_event]), r;

                fcntl(reader->fd, F_SETFL, 0);

                r = write (reader->fd,
                           einit_event_backlog[cd->current_event],
                           len);

                fcntl(reader->fd, F_SETFL, O_NONBLOCK);

                if ((r < 0) || (r == 0)) break;
                if (r < len) {
                    fprintf (stderr, "TOOT TOOT!\n");
                }
            }

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

        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply receive-events #t)", 25);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);
    } else {
        bad_request:

        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply receive-events #f)", 25);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);
    }
}

void einit_ipc_library_receive_specific_events(struct einit_sexp *sexp,
                                               struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_mute_specific_events(struct einit_sexp *sexp,
                                            struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_get_configuration(struct einit_sexp *sexp,
                                         struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_get_configuration_a(struct einit_sexp *sexp,
                                           struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_register_module(struct einit_sexp *sexp,
                                       struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_register_module_actions(struct einit_sexp *sexp,
                                               struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_list(struct einit_sexp *sexp,
                            struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_get_module(struct einit_sexp *sexp,
                                  struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_get_service(struct einit_sexp *sexp,
                                   struct einit_ipc_connection *cd)
{
    einit_ipc_library_stub(sexp, cd);
}

void einit_ipc_library_module_do_bang(struct einit_sexp *sexp,
                                      struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    struct einit_sexp *primus = se_car(sexp),
    *secundus = se_car(se_cdr(sexp));

    if ((primus->type == es_symbol) && (secundus->type == es_symbol)) {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply module-do! #t)", 21);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);

        struct lmodule *lm = mod_lookup_rid (primus->symbol);

        if (strmatch (secundus->symbol, "enable")) {
            mod (einit_module_enable, lm, NULL);
        } else if (strmatch (secundus->symbol, "disable")) {
            mod (einit_module_disable, lm, NULL);
        } else {
            mod (einit_module_custom, lm, (char*)(secundus->symbol));
        }
    } else {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply module-do! #f)", 21);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);
    }
}

void einit_ipc_library_service_do_bang(struct einit_sexp *sexp,
                                       struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    struct einit_sexp *primus = se_car(sexp),
    *secundus = se_car(se_cdr(sexp));

    if ((primus->type == es_symbol) && (secundus->type == es_symbol)) {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply service-do! #t)", 22);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);

        struct einit_event ev = evstaticinit (einit_core_change_service_status);
        ev.rid = (char*)(primus->symbol);
        ev.string = (char*)(secundus->symbol);
        event_emit (&ev, 0);
    } else {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply service-do! #f)", 22);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);
    }
}

void einit_ipc_library_service_switch_mode(struct einit_sexp *sexp,
                                           struct einit_ipc_connection *cd)
{
    struct einit_sexp_fd_reader *reader = cd->reader;

    if (sexp->type == es_symbol) {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply switch-mode! #t)", 23);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);

        struct einit_event ev = evstaticinit (einit_core_switch_mode);
        ev.string = (char*)(sexp->symbol);
        event_emit (&ev, 0);
    } else {
        fcntl(reader->fd, F_SETFL, 0);
        write (reader->fd, "(reply switch-mode! #f)", 23);
        fcntl(reader->fd, F_SETFL, O_NONBLOCK);
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
