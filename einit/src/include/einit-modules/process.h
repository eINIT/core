/*
 *  process.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 18/11/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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

#ifndef _EINIT_MODULES_PROCESS_H
#define _EINIT_MODULES_PROCESS_H

#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>

#define PC_CONDITION_OPTIONAL  0x0001
#define PC_COLLECT_ADDITIVE    0x0010
#define PC_COLLECT_SUBTRACTIVE 0x0020

struct pc_conditional {
 char *match;
 void *para;
 uint16_t match_options;
};

struct process_status {
 time_t last_check;
 pid_t pid;
 char *command;
 char *cwd;

 char **files;
 char state;
 pid_t ppid;
 pid_t pgrp;
 signed int sessionid;
 uint32_t tty;
 pid_t tty_pgrp;
 uint32_t flags;
 uint32_t minflt;
 uint32_t cminflt;
 uint32_t majflt;
 uint32_t cmajflt;
 uint32_t utime;
 uint32_t stime;
 uint32_t cutime;
 uint32_t cstime;
 unsigned char priority;
 signed char nice;
 uint32_t na;
 uint32_t itrealvalue;
 uint32_t starttime;
 uint32_t vsize;
 uint32_t rss;
 uint32_t rlim;
 uint32_t starcode;
 uint32_t endcode;
 uint32_t startstack;
 uint32_t kstkesp;
 uint32_t kstkeip;
 uint32_t signal;
 uint32_t blocked;
 uint32_t sigignore;
 uint32_t sigcatch;
 uint32_t wchan;
 uint32_t nswap;
 uint32_t cnswap;
 int exit_signal;
 uint32_t processor;
 uint32_t rt_priority;
 uint32_t policy;
};

typedef pid_t **(*process_collector)(struct pc_conditional **);
typedef pid_t **(*process_filter)(struct pc_conditional *, pid_t **, struct process_status **);
typedef struct process_status **(*process_status_updater)(struct process_status **);

process_collector pcf;

#define pcollect(x) (pcf? pcf(x) : ((pcf = function_find_one("einit-process-collect", 1, NULL)) ? pcf(x) : NULL))

#define process_configure(mod) pcf = NULL;
#define process_cleanup(mod) pcf = NULL;

#endif
