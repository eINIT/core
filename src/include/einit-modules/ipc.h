/*
 *  ipc.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 26/11/2006.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EINIT_MODULES_IPC_H
#define EINIT_MODULES_IPC_H

#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>

struct ipc_fs_node {
 char *name;
 char is_file;
};

#if (! defined(einit_modules_ipc)) || (einit_modules_ipc == 'm') || (einit_modules_ipc == 'n')

typedef int (*ipc_processor) (const char *, FILE *);

ipc_processor ipc_string_process_fp;

#define ipc_process(string, output) ((ipc_string_process_fp || (ipc_string_process_fp = (ipc_processor)function_find_one("einit-ipc-process-string", 1, NULL))) ? ipc_string_process_fp(string, output) : -1)

#define ipc_configure(mod) ipc_string_process_fp = NULL;
#define ipc_cleanup(mod) ipc_string_process_fp = NULL;

#else

#define ipc_configure(mod) ;
#define ipc_cleanup(mod) ;

int ipc_process_f (const char *cmd, FILE *f);

#define ipc_process(string, output) ipc_process_f(string, output)

#endif

#endif

#ifdef __cplusplus
}
#endif
