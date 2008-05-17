/*
 *  einit-monitor.c
 *  einit
 *
 *  Created by Magnus Deininger on 21/11/2007.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2007-2008, Magnus Deininger All rights reserved.
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
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>

#include <einit/configuration.h>
#include <einit/configuration-static.h>

#include <sys/prctl.h>

/*
 * (define-record einit:event type integer status task flag string
 * stringset module)
 */

#define PID_TERMINATED_EVENT "(event process/died %i 0 0 0 \"\" () einit-monitor)"
#define COREFILE EINIT_LIB_BASE "/bin/einit-core"

pid_t send_sigint_pid = 0;

void einit_sigint(int signal, siginfo_t * siginfo, void *context)
{
    if (send_sigint_pid)
        kill(send_sigint_pid, SIGINT);
}

int run_core(int argc, char **argv, int ipcsocket)
{
    char *narg[argc + 3];
    int i = 0;
    char tmp1[BUFFERSIZE];

    for (; i < argc; i++) {
        narg[i] = argv[i];
    }

    if (ipcsocket != -1) {
        narg[i] = "--socket";
        i++;

        snprintf(tmp1, BUFFERSIZE, "%i", ipcsocket);
        narg[i] = tmp1;
        i++;
    }

    narg[i] = 0;

    execv(COREFILE, narg);
    perror("couldn't execute eINIT");
    return -1;
}

void einit_monitor_loop(int argc, char **argv)
{
    int ipcsocket[2];
    pid_t core_pid;
    char dosocket = 1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ipcsocket) == -1) {
        dosocket = 0;
    }

    core_pid = fork();

    switch (core_pid) {
    case 0:
        if (dosocket) {
            close(ipcsocket[1]);
            run_core(argc, argv, ipcsocket[0]);
        } else {
            run_core(argc, argv, -1);
        }
        perror ("einit-monitor: programme execution error");
        _exit(EXIT_FAILURE);
    case -1:
        perror("einit-monitor: couldn't fork()");
        if (dosocket) {
            close(ipcsocket[0]);
            close(ipcsocket[1]);
        }

        sleep(1);
        return;
    default:
        if (dosocket) {
            close(ipcsocket[0]);
        }
        send_sigint_pid = core_pid;
    }

    while (1) {
        int rstatus;

        pid_t wpid = waitpid(-1, &rstatus, 0);  /* this ought to wait for
                                                 * ANY process */

        if (wpid == core_pid) {
            if (dosocket)
                close(ipcsocket[1]);

            if (WIFEXITED(rstatus)
                && (WEXITSTATUS(rstatus) != einit_exit_status_die_respawn)) {
                if (WEXITSTATUS(rstatus) == EXIT_SUCCESS)
                    fprintf(stderr, "eINIT has quit properly (%i).\n",
                            WEXITSTATUS(rstatus));
                else
                    fprintf(stderr,
                            "eINIT has quit, let's see if it left a message for us (%i)...\n",
                            WEXITSTATUS(rstatus));

                if (WEXITSTATUS(rstatus) ==
                    einit_exit_status_last_rites_halt) {
                    execl(EINIT_LIB_BASE "/bin/last-rites",
                          EINIT_LIB_BASE "/bin/last-rites", "h", NULL);
                } else if (WEXITSTATUS(rstatus) ==
                           einit_exit_status_last_rites_reboot) {
                    execl(EINIT_LIB_BASE "/bin/last-rites",
                          EINIT_LIB_BASE "/bin/last-rites", "r", NULL);
                } else if (WEXITSTATUS(rstatus) ==
                           einit_exit_status_last_rites_kexec) {
                    execl(EINIT_LIB_BASE "/bin/last-rites",
                          EINIT_LIB_BASE "/bin/last-rites", "k", NULL);
                }

                if (WEXITSTATUS(rstatus) == einit_exit_status_exit_respawn) {
                    fprintf(stderr,
                            "Respawning secondary eINIT process.\n");
                }

                exit(EXIT_SUCCESS);
            }

            int n = 5;
            fprintf(stderr,
                    "The secondary eINIT process has died, waiting a while before respawning.\n");

            while ((n = sleep(n)));
            fprintf(stderr, "Respawning secondary eINIT process.\n");
            return;
        } else if (dosocket) {
            char buffer[BUFFERSIZE];
            size_t len;

            snprintf(buffer, BUFFERSIZE, PID_TERMINATED_EVENT, wpid);
            len = strlen(buffer);

            if (write(ipcsocket[1], buffer, len) < len) {
                perror("couldn't write data");
            }
        }
    }
}

int main(int argc, char **argv)
{
    char *argv_mutable[argc + 1];
    int i = 0;

#if defined(PR_SET_NAME)
    prctl(PR_SET_NAME, "einit [monitor]", 0, 0, 0);
#endif

    for (; i < argc; i++) {
        argv_mutable[i] = argv[i];
    }
    argv_mutable[i] = 0;

    if (getpid() == 1) {
        struct sigaction action;

        /*
         * signal handlers 
         */
        action.sa_sigaction = einit_sigint;
        sigemptyset(&(action.sa_mask));
        action.sa_flags = SA_SIGINFO | SA_RESTART | SA_NODEFER;
        if (sigaction(SIGINT, &action, NULL))
            perror("calling sigaction() failed");

        do {
            einit_monitor_loop(argc, argv_mutable);
        } while(1);

        /* will not exit in this mode */
    }

    /*
     * non-ipc, non-core 
     */
    execv(EINIT_LIB_BASE "/bin/einit-helper", argv_mutable);
    perror("couldn't execute " EINIT_LIB_BASE "/bin/einit-helper");
    return -1;
}
