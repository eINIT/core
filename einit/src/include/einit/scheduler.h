/*
 *  scheduler.h
 *  einit
 *
 *  Created by Magnus Deininger on 02/05/2006.
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


#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <pthread.h>
#include <sys/types.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <signal.h>

#define SCHEDULER_SWITCH_MODE 0x0001
#define SCHEDULER_POWER_OFF 0x0002
#define SCHEDULER_POWER_RESET 0x0003
#define SCHEDULER_PID_NOTIFY 0x0004
#define SCHEDULER_MOD_ACTION 0x0005

#define EINIT_NOMINAL 0x00000000
#define EINIT_EXITING 0x00000001

struct sschedule {
 unsigned int task;
 void *param;
};

struct spidcb {
 pid_t pid;
 int status;
 char dead;
 void *(*cfunc)(struct spidcb *);
 struct spidcb *next;
};

struct sschedule **schedule;
struct spidcb *cpids;
struct spidcb *sched_deadorphans;
uint32_t gstatus;

int scheduler_cleanup ();

int sched_switchmode (char *);
int sched_modaction (char **);

void sched_init ();
void sched_reset_event_handlers ();
int sched_queue (unsigned int, void *);
int sched_watch_pid (pid_t, void *(*)(struct spidcb *));
void *sched_run_sigchild (void *);

/* this should be the best place for signal handlers... */

void sched_signal_sigchld (int, siginfo_t *, void *);
void sched_signal_sigint (int, siginfo_t *, void *);

void sched_event_handler(struct einit_event *);

#endif
