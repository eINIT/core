/*
 *  einit-sysvd.c
 *  einit
 *
 *  Created by Magnus Deininger on 23/02/2008.
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

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <einit/einit.h>
#include <syslog.h>

#define INITCTL_MAGIC 0x03091969
#define INITCTL_CMD_START        0x00000000
#define INITCTL_CMD_RUNLVL       0x00000001
#define INITCTL_CMD_POWERFAIL    0x00000002
#define INITCTL_CMD_POWERFAILNOW 0x00000003
#define INITCTL_CMD_POWEROK      0x00000004

#define INITCTL_CMD_SETENV       0x00000006
#define INITCTL_CMD_UNSETENV     0x00000007

struct init_command {
    uint32_t signature;         // signature, must be INITCTL_MAGIC
    uint32_t command;           // the request ID
    uint32_t runlevel;          // the runlevel argument
    uint32_t timeout;           // time between TERM and KILL
    char padding[368];          // padding, legacy applications expect the 
                                // 
    // 
    // 
    // 
    // 
    // 
    // 
    // struct to be 384 bytes long
};

#define PIDFILE "/var/run/einit-sysvd.pid"

#define INITCTL_FIFO "/dev/initctl"
#define INITCTL_FIFO_MODE 0600

#define RUNLEVEL_0 "power-down"
#define RUNLEVEL_1 "single"
#define RUNLEVEL_2 "boot"
#define RUNLEVEL_3 "default"
#define RUNLEVEL_4 "default"
#define RUNLEVEL_5 "default"
#define RUNLEVEL_6 "power-reset"
#define RUNLEVEL_S "single"

char **sysvd_argv = NULL;
int sysvd_argc = 0;

void connect_or_terminate()
{
    if (!einit_connect(&sysvd_argc, sysvd_argv)) {
        syslog(LOG_CRIT, "could not connect to einit: %m");
    }
}

int initctl_wait(char *fifo)
{
    int nfd;

    while ((nfd = open(fifo, O_RDONLY))) {
        struct init_command ic;

        if (nfd == -1) {        /* open returning -1 is very bad,
                                 * terminate */
            return EXIT_FAILURE;
        }

        memset(&ic, 0, sizeof(struct init_command));    // clear this
        // struct, just in 
        // case

        if (read(nfd, &ic, sizeof(struct init_command)) > 12) { // enough
            // bytes
            // to
            // process 
            // were
            // read,
            // we dont 
            // care
            // about
            // the
            // rest
            // anyway
            if (ic.signature == INITCTL_MAGIC) {
                // INITCTL_CMD_START: what's that do?
                // INITCTL_CMD_UNSETENV is deliberately ignored
                if (ic.command == INITCTL_CMD_RUNLVL) { // switch
                    // runlevels
                    // (modes...)
                    char *nmode = NULL;

                    switch (ic.runlevel) {
                    case '0':
                        nmode = RUNLEVEL_0;
                        break;
                    case '1':
                        nmode = RUNLEVEL_1;
                        break;
                    case '2':
                        nmode = RUNLEVEL_2;
                        break;
                    case '3':
                        nmode = RUNLEVEL_3;
                        break;
                    case '4':
                        nmode = RUNLEVEL_4;
                        break;
                    case '5':
                        nmode = RUNLEVEL_5;
                        break;
                    case '6':
                        nmode = RUNLEVEL_6;
                        break;
                    case 's':
                    case 'S':
                        nmode = RUNLEVEL_S;
                        break;
                    default:
                        syslog(LOG_WARNING, "Invalid Runlevel: '%c'",
                               ic.runlevel);
                        break;
                    }

                    if (nmode) {
                        syslog(LOG_NOTICE,
                               "initctl: switching to mode %s (runlevel %c)",
                               nmode, ic.runlevel);

                        connect_or_terminate();
                        einit_switch_mode(nmode);
                        einit_disconnect();
                    }
                } else if (ic.command == INITCTL_CMD_SETENV) {  // padding 
                                                                // 
                    // 
                    // 
                    // 
                    // 
                    // 
                    // 
                    // contains 
                    // the new 
                    // environment 
                    // string
                    char **cx = str2set(':', ic.padding);
                    if (cx) {
                        if (cx[0] && cx[1]) {
                            if (strmatch(cx[0], "INIT_HALT")) {
                                if (strmatch(cx[1], "HALT")
                                    || strmatch(cx[1], "POWERDOWN")) {
                                    connect_or_terminate();
                                    einit_power_down();
                                    einit_disconnect();
                                }
                            }
                        }

                        efree(cx);
                    }
                } else
                    syslog(LOG_WARNING,
                           "invalid initctl received: unknown command");
            } else {
                syslog(LOG_WARNING,
                       "invalid initctl received: invalid signature");
            }
        }

        close(nfd);
    }

    return EXIT_SUCCESS;        /* not reached... usually */
}

int main(int argc, char **argv)
{
    FILE *pidfile;

    sysvd_argv = argv;
    sysvd_argc = argc;

    unlink(INITCTL_FIFO);       /* unlink fifo, ignore if it didn't work */
    if (mkfifo(INITCTL_FIFO, INITCTL_FIFO_MODE) && (errno != EEXIST)) {
        perror("could not create initctl fifo");
        return EXIT_FAILURE;
    }

    if (daemon(0, 0)) {
        perror("could not daemonise");
        return EXIT_FAILURE;
    }

    pidfile = fopen(PIDFILE, "w");
    if (pidfile) {
        fprintf(pidfile, "%d\n", getpid());
        fclose(pidfile);
    }

    return initctl_wait(INITCTL_FIFO);
}
