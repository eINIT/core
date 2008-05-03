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

#include <sys/select.h>

#include <errno.h>

struct itree *einit_ipc_replies = NULL;

struct einit_sexp_fd_reader *einit_ipc_client_rd = NULL;

/*
 * (define-record einit:event type integer status task flag string
 * stringset module)
 */

enum sexp_event_parsing_stage {
    seps_type,
    seps_integer,
    seps_status,
    seps_task,
    seps_flag,
    seps_string,
    seps_stringset,
    seps_module,
    seps_done
};

void einit_ipc_handle_sexp_event(struct einit_sexp *sexp)
{
    enum sexp_event_parsing_stage s = seps_type;
    struct einit_event *ev = NULL;

    while ((s != seps_done) && (sexp->type == es_cons)) {
        struct einit_sexp *p = sexp->data.cons.primus;

        switch (s) {
        case seps_type:
            if (p->type == es_symbol) {
                enum einit_event_code c = event_string_to_code(p->data.symbol);

                if (c == einit_event_subsystem_custom)
                    return;

                ev = ecalloc(1, sizeof(struct einit_event));
                ev->type = c;
            } else {
                return;
            }
            break;
        case seps_integer:
            if (p->type == es_integer) {
                ev->integer = p->data.integer;
            } else {
                efree(ev);
                return;
            }
            break;
        case seps_status:
            if (p->type == es_integer) {
                ev->status = p->data.integer;
            } else {
                efree(ev);
                return;
            }
            break;
        case seps_task:
            if (p->type == es_integer) {
                ev->task = p->data.integer;
            } else {
                efree(ev);
                return;
            }
            break;
        case seps_flag:
            if (p->type == es_integer) {
                ev->flag = p->data.integer;
            } else {
                efree(ev);
                return;
            }
            break;
        case seps_string:
            if (p->type == es_string) {
                if (p->data.string[0])
                    ev->string = (char *) p->data.string;
                else
                    ev->string = NULL;
            } else {
                efree(ev);
                return;
            }
            break;
        case seps_stringset:
            while (p->type == es_cons) {
                struct einit_sexp *pp = p->data.cons.primus;

                if (pp->type == es_string) {
                    ev->stringset =
                        set_str_add_stable(ev->stringset,
                                           (char *) pp->data.string);
                } else {
                    efree(ev);
                    return;
                }

                p = p->data.cons.secundus;
            }
            break;
        case seps_module:
            if (p->type == es_symbol) {
                ev->rid = (char *) p->data.symbol;
            } else {
                efree(ev);
                return;
            }
            break;

        default:
            break;
        }

        s++;
        sexp = sexp->data.cons.secundus;
    }

    if (ev) {
        /*
         * fprintf(stderr, "EVENT, decoded, emitting: %s\n",
         * einit_event_encode(ev));
         */

        event_emit(ev, 0);
        efree(ev);
    }
}

char einit_ipc_loop()
{
    if (!einit_ipc_client_rd)
        return 0;

    struct einit_sexp *sexp;

    while ((sexp =
            einit_read_sexp_from_fd_reader(einit_ipc_client_rd)) !=
           sexp_bad) {
        if (!sexp) {
            return 1;

            continue;
        }
        // char *r = einit_sexp_to_string(sexp);
        // fprintf(stderr, "READ SEXP: (%i) \"%s\"\n", sexp->type, r);

        // efree(r);

//        einit_sexp_display(sexp);

        if ((sexp->type == es_cons) && (sexp->data.cons.secundus->type == es_cons)) {
            if ((sexp->data.cons.primus->type == es_integer)
                 && (sexp->data.cons.secundus->data.cons.secundus->type == es_list_end)) {
                /*
                 * 2-tuple and starts with an integer: reply 
                 */
                // fprintf(stderr, "REPLY!\n");

                // fprintf (stderr, "storing reply with id=%i\n",
                // sexp->data.cons.primus->data.integer);

                einit_ipc_replies =
                        itreeadd(einit_ipc_replies, sexp->data.cons.primus->data.integer,
                                 sexp->data.cons.secundus->data.cons.primus, tree_value_noalloc);
                continue;
            }

            if ((sexp->data.cons.secundus->data.cons.secundus->type == es_cons)
                 && (sexp->data.cons.secundus->data.cons.secundus->data.cons.secundus->type == es_cons)
                 && (sexp->data.cons.secundus->data.cons.secundus->data.cons.secundus->data.cons.secundus->type ==
                 es_list_end) && (sexp->data.cons.primus->type == es_symbol)
                 && (strmatch(sexp->data.cons.primus->data.symbol, "request"))) {
                /*
                 * 4-tuple: must be a request
                 */
                // fprintf(stderr, "REQUEST!\n");

                /*
                 * TODO: still need to handle this somehow
                 */

                einit_sexp_destroy(sexp);

                continue;
            }

            if ((sexp->data.cons.primus->type == es_symbol)
                 && (strmatch(sexp->data.cons.primus->data.symbol, "event"))) {

                // fprintf(stderr, "EVENT, decoding\n");

                einit_ipc_handle_sexp_event(sexp->data.cons.secundus);
                einit_sexp_destroy(sexp);

                continue;
            }
        }
        // fprintf(stderr, "BAD MESSAGE!\n");

        einit_sexp_destroy(sexp);
    }

    return 0;
}

void einit_ipc_loop_infinite()
{
    int fd = einit_ipc_get_fd();

    while (1) {
        int selectres;

        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        selectres = select((fd + 1), &rfds, NULL, NULL, 0);

        if (selectres > 0) {
            einit_ipc_loop();
        }
    }
}

char einit_ipc_connect_socket(int fd)
{
    einit_ipc_client_rd = einit_create_sexp_fd_reader(fd);

    return (einit_ipc_client_rd != NULL);
}

char einit_ipc_connect(const char *address)
{
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket()");
        return 0;
    }

    struct sockaddr_un addr = {.sun_family = AF_UNIX };
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", address);

    if (connect
        (fd, (const struct sockaddr *) &addr,
         sizeof(struct sockaddr_un)) < 0) {
        perror("connect()");
        close(fd);
        return 0;
    }

    return einit_ipc_connect_socket(fd);
}

int einit_ipc_get_fd()
{
    if (!einit_ipc_client_rd)
        return -1;

    return einit_ipc_client_rd->fd;
}

int einit_ipc_current_id = 0;

struct einit_sexp *einit_ipc_request_sexp_raw(struct einit_sexp *request)
{
    if (!einit_ipc_client_rd)
        return NULL;
    int id = (se_car(se_cdr(se_cdr(request))))->data.integer;

    int fd = einit_ipc_client_rd->fd;

    char *r = einit_sexp_to_string(request);
    einit_ipc_write(r, einit_ipc_client_rd);
    efree(r);

    einit_sexp_destroy(request);

    while (1) {
        int selectres;
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        selectres = select((fd + 1), &rfds, NULL, NULL, 0);

        if (selectres > 0) {
            einit_ipc_loop();
        }

        if (einit_ipc_replies) {
            struct itree *t =
                itreefind(einit_ipc_replies, id, tree_find_first);

            if (t) {
                struct einit_sexp *rv = (struct einit_sexp *) (t->value);
                // fprintf (stderr, "REPLY FOR REQUEST: %s, REPLY=%s\n",
                // r, einit_sexp_to_string(rv));
                // efree (r);

                return rv;
            }                   // else {
            // fprintf (stderr, "NO REPLY FOR REQUEST: %s [id=%i]\n", r,
            // id);
            // }
        }
    }

    return (struct einit_sexp *) sexp_bad;
}

struct einit_sexp *einit_ipc_request(const char *rq,
                                     struct einit_sexp *payload)
{
    struct einit_sexp *req = se_cons(se_symbol("request"),
                                     se_cons(se_symbol(rq),
                                             se_cons(se_integer
                                                     (einit_ipc_current_id++),
                                                     se_cons(payload,
                                                             (struct
                                                              einit_sexp *)
                                                             sexp_end_of_list))));

    return einit_ipc_request_sexp_raw(req);
}

void einit_ipc_write(char *s, struct einit_sexp_fd_reader *rd)
{
    int len = strlen(s);

    fcntl(rd->fd, F_SETFL, 0);
    int r = write(rd->fd, s, len);
    fcntl(rd->fd, F_SETFL, O_NONBLOCK);

    if ((r < 0) || (r == 0)) {
        fprintf(stderr, "COULDN'T WRITE: %s\n", s);
        perror("error");
        if ((r == 0) || (errno == EAGAIN) || (errno == EINTR)) {
            einit_ipc_write(s, rd);
        }
    } else if (r < len) {
        fprintf(stderr, "SHORT WRITE!\n");
        einit_ipc_write(s + len, rd);
    }

    fsync(rd->fd);
}
