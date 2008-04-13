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

#define BAD_REQUEST "(reply unknown bad-request)"
#define BAD_REQUEST_SIZE sizeof(BAD_REQUEST)

void einit_ipc_library_stub(struct einit_sexp *sexp, int fd)
{
    char *r = einit_sexp_to_string(sexp);
    fprintf(stderr, "IPC STUB: %s\n", r);

    efree(r);

    write(fd, BAD_REQUEST, BAD_REQUEST_SIZE);
}

void einit_ipc_library_receive_events(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_receive_specific_events(struct einit_sexp *sexp,
                                               int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_mute_specific_events(struct einit_sexp *sexp,
                                            int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_get_configuration(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_get_configuration_a(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_register_module(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_register_module_actions(struct einit_sexp *sexp,
                                               int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_list(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_get_module(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_get_service(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_module_do_bang(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_service_do_bang(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
}

void einit_ipc_library_service_switch_mode(struct einit_sexp *sexp, int fd)
{
    einit_ipc_library_stub(sexp, fd);
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
    einit_ipc_register_handler("switch-mode",
                               einit_ipc_library_service_switch_mode);

    return 0;
}
