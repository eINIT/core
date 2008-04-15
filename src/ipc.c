/*
 *  ipc.c
 *  einit
 *
 *  Created by Magnus Deininger on 07/04/2008.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <einit/einit.h>
#include <einit/sexp.h>
#include <einit/ipc.h>

struct einit_ipc_connection *einit_ipc_connections = NULL;

int einit_ipc_sexp_fd = -1;

struct einit_ipc_handler {
    void (*handler) (struct einit_sexp *, struct einit_ipc_connection *);
};

struct stree *einit_ipc_handlers = NULL;

char ** einit_event_backlog = NULL;

void einit_ipc_add_connection (enum ipc_flag flags,
                               struct einit_sexp_fd_reader *reader)
{
    struct einit_ipc_connection *n =
        emalloc (sizeof (struct einit_ipc_connection));

    n->flags = flags;
    n->current_event = -1;
    n->reader = reader;

    reader->custom = n;

    n->next = einit_ipc_connections;
    einit_ipc_connections = n;
};

void einit_ipc_remove_connection (struct einit_ipc_connection *c)
{
    if (c == einit_ipc_connections) {
        einit_ipc_connections = einit_ipc_connections->next;

        efree (c);
    } else {
        struct einit_ipc_connection *o = einit_ipc_connections;

        while (o)
        {
            if (o->next == c) {
                o->next = c->next;
                efree (c);
                return;
            }

            o = o->next;
        }
    }
}

void einit_ipc_register_handler(const char *name,
                                void (*handler) (struct einit_sexp *,
                                                 struct einit_ipc_connection *))
{
    if (einit_ipc_handlers) {
        struct stree *st =
            streefind(einit_ipc_handlers, name, tree_find_first);
        if (st) {
            do {
                struct einit_ipc_handler *h = st->value;

                if (!h->handler) {
                    h->handler = handler;

                    return;
                }
            } while ((st =
                      streefind(einit_ipc_handlers, name,
                                tree_find_next)));
        }
    }

    struct einit_ipc_handler h = {.handler = handler };

    einit_ipc_handlers =
        streeadd(einit_ipc_handlers, name, &h,
                 sizeof(struct einit_ipc_handler), NULL);
}

void einit_ipc_unregister_handler(const char *name,
                                  void (*handler) (struct einit_sexp *,
                                         struct einit_ipc_connection *))
{
    if (!einit_ipc_handlers)
        return;

    struct stree *st =
        streefind(einit_ipc_handlers, name, tree_find_first);
    if (st) {
        do {
            struct einit_ipc_handler *h = st->value;

            if (h->handler == handler) {
                h->handler = NULL;
            }
        } while ((st =
                  streefind(einit_ipc_handlers, name, tree_find_next)));
    }
}

char einit_ipx_sexp_handle_fd(struct einit_sexp_fd_reader *rd)
{
    struct einit_sexp *sexp;

    // fprintf (stderr, "reading sexp: %i\n", rd->fd);

    while ((sexp = einit_read_sexp_from_fd_reader(rd))) {
        if (sexp == sexp_bad) {
            // fprintf (stderr, "BAD SEXP: %i\n", rd->fd);

            return 1;
        }

        char *s = einit_sexp_to_string(sexp);
        if (s) {
            fprintf(stderr, "read sexp: %s\n", s);
            efree(s);
        }

        if (sexp->type == es_cons) {
            if (sexp->primus->type == es_symbol) {
                if (strmatch(sexp->primus->symbol, "request")) {
                    if ((sexp->secundus->type == es_cons)
                        && (sexp->secundus->primus->type == es_symbol)
                        && (sexp->secundus->secundus->type == es_cons)) {
                        struct stree *st = streefind(einit_ipc_handlers,
                                                     sexp->secundus->
                                                     primus->symbol,
                                                     tree_find_first);
                        if (st) {
                            do {
                                struct einit_ipc_handler *h = st->value;

                                if (h->handler) {
                                    h->handler(sexp->secundus->secundus->
                                               primus, rd->custom);
                                }
                            } while ((st =
                                      streefind(einit_ipc_handlers,
                                                sexp->secundus->primus->
                                                symbol, tree_find_next)));
                        } else {
                            char buffer[BUFFERSIZE];

                            snprintf(buffer, BUFFERSIZE,
                                     "(reply %s bad-request)",
                                     sexp->secundus->primus->symbol);
                            write(rd->fd, buffer, strlen(buffer));
                        }
                    }
                } else if (strmatch(sexp->primus->symbol, "event")) {
                    einit_ipc_handle_sexp_event(sexp->secundus);
                }
            }
        }
    }

    // fprintf (stderr, "no sexp ready\n");

    return 0;
}

void einit_ipx_sexp_handle_connect()
{
    int fd = accept(einit_ipc_sexp_fd, NULL, 0);
    fprintf(stderr, "connected: %i\n", fd);

    if (fd < 0)
        return;

    struct einit_sexp_fd_reader *rd = einit_create_sexp_fd_reader(fd);

    einit_ipc_add_connection (0, rd);
}

int einit_ipc_sexp_prepare(fd_set * rfds)
{
    int r = 0;

    if (einit_ipc_sexp_fd < 0)
        return 0;

    FD_SET(einit_ipc_sexp_fd, rfds);
    if (r < einit_ipc_sexp_fd)
        r = einit_ipc_sexp_fd;

    struct einit_ipc_connection *c = einit_ipc_connections;

    while (c) {
        struct einit_ipc_connection *o = c;
        struct einit_sexp_fd_reader *reader = o->reader;

        c = c->next;

        FD_SET(reader->fd, rfds);
        if (r < reader->fd)
            r = reader->fd;
    }

    return r;
}

void einit_ipc_sexp_handle(fd_set * rfds)
{
    if (einit_ipc_sexp_fd < 0)
        return;

    if (FD_ISSET(einit_ipc_sexp_fd, rfds))
        einit_ipx_sexp_handle_connect();

    struct einit_ipc_connection *c = einit_ipc_connections;

    while (c) {
        struct einit_ipc_connection *o = c;
        struct einit_sexp_fd_reader *reader = o->reader;

        c = c->next;

        if (FD_ISSET(reader->fd, rfds)) {
            fprintf(stderr, "readable: %i\n", reader->fd);

            if (einit_ipx_sexp_handle_fd(reader)) {
                fprintf(stderr, "client disconnected\n");

                einit_ipc_remove_connection (o);
            }
        }
    }
}

char einit_ipx_sexp_prepare_fd()
{
    char *address = cfg_getstring("einit-subsystem-ipc/socket", NULL);
    if (!address)
        address = "/dev/einit";

    if (coremode & einit_mode_sandbox) {
        address = "dev/einit";
    }

    unlink(address);

    einit_ipc_sexp_fd = socket(PF_UNIX, SOCK_STREAM, 0);

    if (einit_ipc_sexp_fd < 0) {
        perror("socket()");
        return 0;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", address);

    if (bind
        (einit_ipc_sexp_fd, (const struct sockaddr *) &addr,
         sizeof(struct sockaddr_un)) < 0) {
        perror("bind()");
        close(einit_ipc_sexp_fd);
        einit_ipc_sexp_fd = -1;
        return 0;
    }

    if (listen(einit_ipc_sexp_fd, 10) < 0) {
        perror("listen()");
        close(einit_ipc_sexp_fd);
        einit_ipc_sexp_fd = -1;
        return 0;
    }

    fcntl(einit_ipc_sexp_fd, F_SETFL, O_NONBLOCK);
    fcntl(einit_ipc_sexp_fd, F_SETFD, FD_CLOEXEC);

    char *group = cfg_getstring("subsystem-ipc-9p/group", NULL);
    char *chmod_i = cfg_getstring("subsystem-ipc-9p/chmod", NULL);

    if (!group)
        group = "einit";
    if (!chmod_i)
        chmod_i = "0660";
    mode_t smode = parse_integer(chmod_i);

    gid_t g;
    lookupuidgid(NULL, &g, NULL, group);

    chown(address, 0, g);
    chmod(address, smode);

    einit_add_fd_prepare_function(einit_ipc_sexp_prepare);
    einit_add_fd_handler_function(einit_ipc_sexp_handle);

    return 1;
}

void einit_ipc_sexp_boot_event_handler_root_device_ok(struct einit_event
                                                      *ev)
{
    if (einit_ipc_sexp_fd < 0) {
        einit_ipx_sexp_prepare_fd();
    }
}

void einit_ipc_sexp_power_event_handler(struct einit_event *ev)
{
    if (einit_ipc_sexp_fd < 0)
        return;

    notice(4, "disabling IPC (sexp)");

    struct einit_event nev = evstaticinit(einit_ipc_disabling);
    event_emit(&nev, 0);
    evstaticdestroy(&nev);

    int fd = einit_ipc_sexp_fd;
    einit_ipc_sexp_fd = -1;

    close(fd);
}

void einit_ipc_update_event_listeners ()
{
    struct einit_ipc_connection *c = einit_ipc_connections;

    while (c) {
        struct einit_sexp_fd_reader *reader = c->reader;

        if (c->current_event > 0) {
            for (; einit_event_backlog[c->current_event]; c->current_event++) {
                int len = strlen (einit_event_backlog[c->current_event]), r;

                /*
                 * we're escaping the non-blocking mode for sheer ease-of-use
                 */

                fcntl(reader->fd, F_SETFL, 0);

                r = write (reader->fd,
                           einit_event_backlog[c->current_event],
                           len);

                fcntl(reader->fd, F_SETFL, O_NONBLOCK);

                /*
                 * in case something bad happens, just break out
                 */

                if ((r < 0) || (r == 0)) break;
                if (r < len) {
                    fprintf (stderr, "TOOT TOOT! tried to write %d bytes,"
                                     " but only wrote %d bytes\n", len, r);
                }
            }
        }

        c = c->next;
    }
}


void einit_ipc_generic_event_handler(struct einit_event *ev)
{
    if (ev->type != einit_core_forked_subprocess) {
        const char *s = einit_event_encode (ev);

        if (coremode & einit_mode_sandbox) {
            fprintf (stderr, "%s\n", s);
        }

        einit_event_backlog = set_str_add_stable (einit_event_backlog, (char *)s);

        einit_ipc_update_event_listeners ();
    } else {
        /*
         * we need to make a check for this in the generic handler anyway,
         * might as well do the handling here
         */

        event_ignore(einit_boot_dev_writable,
                     einit_ipc_sexp_boot_event_handler_root_device_ok);
        event_ignore(einit_boot_root_device_ok,
                     einit_ipc_sexp_boot_event_handler_root_device_ok);

        event_ignore(einit_event_subsystem_any, einit_ipc_generic_event_handler);

        if (einit_ipc_sexp_fd > 0) {
            close(einit_ipc_sexp_fd);
            einit_ipc_sexp_fd = -1;
        }
    }
}

void einit_ipc_setup()
{
    event_listen(einit_boot_dev_writable,
                 einit_ipc_sexp_boot_event_handler_root_device_ok);
    event_listen(einit_boot_root_device_ok,
                 einit_ipc_sexp_boot_event_handler_root_device_ok);

    event_listen(einit_power_down_imminent,
                 einit_ipc_sexp_power_event_handler);
    event_listen(einit_power_reset_imminent,
                 einit_ipc_sexp_power_event_handler);
    event_listen(einit_ipc_disable, einit_ipc_sexp_power_event_handler);

    event_listen(einit_event_subsystem_any, einit_ipc_generic_event_handler);
}
