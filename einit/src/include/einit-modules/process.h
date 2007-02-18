/*
 *  process.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 18/11/2006.
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
 time_t update;
 pid_t pid;
 char *cmd;
 char *cwd;

/* char **files;
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
 uint32_t policy;*/
};

typedef pid_t *(*process_collector)(struct pc_conditional **);
typedef int (*process_signal_function)(struct pc_conditional **, int);
typedef pid_t *(*process_filter)(struct pc_conditional *, pid_t *, struct process_status **);
typedef struct process_status **(*process_status_updater)(struct process_status **);

process_collector __f_process_collector, __f_p_jktdb;
process_signal_function __f_e_kill;

#define pcollect(x) ((__f_process_collector || (__f_process_collector = function_find_one("einit-process-collect", 1, NULL))) ? __f_process_collector(x) : NULL)

#define ekill(x,signal) ((__f_e_kill || (__f_e_kill = function_find_one("einit-process-ekill", 1, NULL))) ? __f_e_kill(x,signal) : -1)
#define pekill(x) ((__f_p_jktdb || (__f_p_jktdb = function_find_one("einit-process-killing-spree", 1, NULL))) ? __f_p_jktdb(x) : NULL)

#define process_configure(mod) __f_process_collector = NULL; __f_e_kill = NULL; __f_p_jktdb = NULL;
#define process_cleanup(mod) __f_process_collector = NULL; __f_e_kill = NULL; __f_p_jktdb = NULL;

#endif
