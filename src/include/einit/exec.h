/*
 *  config.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

#ifndef EINIT_EXEC_H
#define EINIT_EXEC_H

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum einit_exec_options {
 einit_exec_default        = 0x0000,
 einit_exec_keep_stdin     = 0x0001,
 einit_exec_no_shell       = 0x0010,
 einit_exec_shell          = 0x0020,
 einit_exec_no_pipe        = 0x0100,
 einit_exec_create_session = 0x1000
};

struct einit_exec_data {
 enum einit_exec_options options;
 union {
  const char *command;
  char **command_d;
 };

 uid_t uid;
 gid_t gid;
 char *rid;

 char **environment;

 int readpipe;
 pid_t pid;

 int status;

 int (*handle_pipe_fragment)(struct einit_exec_data *);
 int (*handle_dead_process)(struct einit_exec_data *);
};

pid_t einit_exec (struct einit_exec_data *);
int einit_exec_wait(pid_t);

pid_t einit_exec_without_shell (char **);
void einit_exec_without_shell_sequence (char ***);

pid_t einit_exec_with_shell (char *);

pid_t einit_exec_auto (char *);

struct einit_exec_data *  einit_exec_create_exec_data_from_string (char *);


#endif

#ifdef __cplusplus
}
#endif
