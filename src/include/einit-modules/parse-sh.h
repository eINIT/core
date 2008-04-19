/*
 *  parse-sh.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 08/01/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2006, 2007, Magnus Deininger All rights reserved.
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

#ifndef EINIT_MODULES_IPC_H
#define EINIT_MODULES_IPC_H

#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>

    enum einit_sh_parser_status {
        sh_parser_status_lw = 0,
        sh_parser_status_read = 1,
        sh_parser_status_ignore_till_eol = 2
    };

    enum einit_sh_parser_pa {
        pa_end_of_file = 0x1,
        pa_new_context = 0x2,
        pa_new_context_fork = 0x4
    };

    int parse_sh(const char *data,
                 void (*callback) (const char **, uint8_t, void *), void *ud);

#define parse_sh_ud(data, callback, user) parse_sh(data, callback, user)

#endif

#ifdef __cplusplus
}
#endif
