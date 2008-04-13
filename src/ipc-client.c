/*
 *  ipc.c
 *  einit
 *
 *  Created by Magnus Deininger on 07/04/2008.
 *  Copyright 2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

struct einit_ipc_callback {
    void (*handler)(struct einit_sexp *);
    struct einit_ipc_callback *next;
};

struct einit_ipc_callback *einit_ipc_callbacks = NULL;

struct einit_sexp_fd_reader *einit_ipc_client_rd = NULL;

/* (define-record einit:event
  type integer status task flag string stringset module)*/

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

void einit_ipc_handle_sexp_event (struct einit_sexp *sexp) {
    enum sexp_event_parsing_stage s = seps_type;
    struct einit_event *ev = NULL;

    while ((s != seps_done) && (sexp->type == es_cons)) {
        struct einit_sexp *p = sexp->primus;

        switch (s) {
            case seps_type:
                if (p->type == es_symbol) {
                    enum einit_event_code c = event_string_to_code (p->symbol);

                    if (c == einit_event_subsystem_custom) return;

                    ev = ecalloc (1, sizeof (struct einit_event));
                    ev->type = c;
                } else {
                    return;
                }
                break;
            case seps_integer:
                if (p->type == es_integer) {
                    ev->integer = p->integer;
                } else {
                    efree (ev);
                    return;
                }
                break;
            case seps_status:
                if (p->type == es_integer) {
                    ev->status = p->integer;
                } else {
                    efree (ev);
                    return;
                }
                break;
            case seps_task:
                if (p->type == es_integer) {
                    ev->task = p->integer;
                } else {
                    efree (ev);
                    return;
                }
                break;
            case seps_flag:
                if (p->type == es_integer) {
                    ev->flag = p->integer;
                } else {
                    efree (ev);
                    return;
                }
                break;
            case seps_string:
                if (p->type == es_string) {
                    ev->string = (char *)p->string;
                } else {
                    efree (ev);
                    return;
                }
                break;
            case seps_stringset:
                while (p->type == es_cons) {
                    struct einit_sexp *pp = p->primus;

                    if (pp->type == es_string) {
                        ev->stringset = set_str_add_stable (ev->stringset, (char *)pp->string);
                    } else {
                        efree (ev);
                        return;
                    }

                    p = p->secundus;
                }
                break;
            case seps_module:
                if (p->type == es_symbol) {
                    ev->rid = (char *)p->symbol;
                } else {
                    efree (ev);
                    return;
                }
                break;

            default:
                break;
        }

        s++;
        sexp = sexp->secundus;
    }

    if (ev) {
        event_emit (ev, 0);
        efree (ev);
    }
}

char einit_ipc_loop() {
    if (!einit_ipc_client_rd) return 0;

    struct einit_sexp *sexp;

    while ((sexp = einit_read_sexp_from_fd_reader (einit_ipc_client_rd)) != BAD_SEXP) {
        if (!sexp) {
            return 1;

            continue;
        }

        if (sexp->type == es_cons) {
            if ((sexp->secundus->type == es_cons) && (sexp->secundus->secundus->type == es_cons) && (sexp->secundus->secundus->secundus->type == es_list_end)) {
                /* 3-tuple: must be a reply, or a request */
                char *r = einit_sexp_to_string (sexp);
                fprintf (stderr, "REQUEST: %s\n", r);

                efree (r);

                if ((sexp->primus->type == es_symbol) && (strmatch (sexp->primus->symbol, "reply"))) {
                    if (einit_ipc_callbacks) {
                        void (*handler)(struct einit_sexp *) = einit_ipc_callbacks->handler;
                        struct einit_ipc_callback * t = einit_ipc_callbacks;

                        einit_ipc_callbacks = t->next;
                        efree (t);

                        if (sexp->secundus->secundus->primus->type == es_symbol) {
                            if (strmatch (sexp->secundus->secundus->primus->symbol, "bad-request")) {
                                char *r = einit_sexp_to_string (sexp);
                                fprintf (stderr, "BAD REQUEST: %s\n", r);

                                efree (r);
                            } else {
                                handler(sexp);
                            }
                        }
                    }
                }
            } else {
                if ((sexp->primus->type == es_symbol) && (strmatch (sexp->primus->symbol, "event"))) {
                    einit_ipc_handle_sexp_event (sexp->secundus);
                }
            }
        } else {
            char *r = einit_sexp_to_string (sexp);
            fprintf (stderr, "BAD MESSAGE: %s\n", r);

            efree (r);
        }

        einit_sexp_destroy (sexp);
    }

    return 0;
}

void einit_ipc_request_callback (const char *request, void (*handler)(struct einit_sexp *)) {
    if (!einit_ipc_client_rd) return;

    struct einit_ipc_callback *cb = emalloc (sizeof (struct einit_ipc_callback));
    cb->next = einit_ipc_callbacks;
    cb->handler = handler;

    einit_ipc_callbacks = cb;

    write (einit_ipc_client_rd->fd, request, strlen (request));
    fsync(einit_ipc_client_rd->fd);
}

char einit_ipc_connect (const char *address) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        perror ("socket()");
        return 0;
    }

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    snprintf (addr.sun_path, sizeof(addr.sun_path), "%s", address);

    if (connect(fd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror ("connect()");
        close (fd);
        return 0;
    }

    einit_ipc_client_rd = einit_create_sexp_fd_reader (fd);

    return (einit_ipc_client_rd != NULL);
}

int einit_ipc_get_fd () {
    if (!einit_ipc_client_rd) return -1;

    return einit_ipc_client_rd->fd;
}
