/*
 *  libeinit.c
 *  einit
 *
 *  Created by Magnus Deininger on 24/07/2007.
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

#include <einit/einit.h>
#include <einit/utility.h>
#include <einit/bitch.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <einit/event.h>
#include <einit/ipc.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <fcntl.h>

#define DEFAULT_EINIT_ADDRESS "/dev/einit"

#ifdef __FreeBSD__
#include <signal.h>
#endif

#ifdef estrdup
#undef estrdup
#endif
#ifdef emalloc
#undef emalloc
#endif
#ifdef ecalloc
#undef ecalloc
#endif

#ifdef __APPLE__
/*
 * dammit, what's wrong with macos!? 
 */

struct exported_function *cfg_addnode_fs = NULL;
struct exported_function *cfg_findnode_fs = NULL;
struct exported_function *cfg_getstring_fs = NULL;
struct exported_function *cfg_getnode_fs = NULL;
struct exported_function *cfg_getpath_fs = NULL;
struct exported_function *cfg_prefix_fs = NULL;
struct exported_function *cfg_callback_prefix_fs = NULL;

char einit_new_node = 0;
struct utsname osinfo = { };
#endif

char **einit_initial_environment = NULL;
char **einit_global_environment = NULL;
char **einit_argv = NULL;

struct stree *exported_functions = NULL;

struct cfgnode *cmode = NULL, *amode = NULL;
enum einit_mode coremode = einit_mode_init;

void einit_power_down()
{
    einit_switch_mode("power-down");
}

void einit_power_reset()
{
    einit_switch_mode("power-reset");
}

void einit_switch_mode(const char *mode)
{                               // think "runlevel"
}

/*
 * client 
 */

const char *einit_ipc_address = DEFAULT_EINIT_ADDRESS;
pid_t einit_ipc_client_pid = 0;

char einit_connect(int *argc, char **argv)
{
    char *envvar = getenv("EINIT_ADDRESS");
    char priv = 0;
    if (envvar)
        einit_ipc_address = envvar;

    if (argc && argv) {
        int i = 0;
        for (i = 1; i < *argc; i++) {
            if (argv[i][0] == '-')
                switch (argv[i][1]) {
                case 'p':
                    priv = 1;
                    break;
                case 'a':
                    if ((++i) < (*argc))
                        einit_ipc_address = argv[i];
                    break;
                }
        }
    }

    if (!einit_ipc_address || !einit_ipc_address[0])
        einit_ipc_address = DEFAULT_EINIT_ADDRESS;

    // einit_ipc_9p_fd = ixp_dial (einit_ipc_address);
    if (priv) {
        return einit_connect_spawn(argc, argv);
    } else {
        return einit_ipc_connect(einit_ipc_address);
    }
}

char einit_connect_spawn(int *argc, char **argv)
{
    char sandbox = 0;

    if (argc && argv) {
        int i = 0;
        for (i = 1; i < *argc; i++) {
            if (argv[i][0] == '-')
                switch (argv[i][1]) {
                case 'p':
                    if (argv[i][2] == 's')
                        sandbox = 1;
                    break;
                }
        }
    }

    char address[BUFFERSIZE];
    struct stat st;

    snprintf(address, BUFFERSIZE, "/tmp/einit.%i", getpid());

    // int fd = 0;

    einit_ipc_client_pid = fork();

    switch (einit_ipc_client_pid) {
    case -1:
        return 0;
        break;
    case 0:
        /*
         * fd = open ("/dev/null", O_RDWR); if (fd) { close (0); close
         * (1); close (2);
         * 
         * dup2 (fd, 0); dup2 (fd, 1); dup2 (fd, 2);
         * 
         * close (fd); }
         */

        execl(EINIT_LIB_BASE "/bin/einit-core", "einit-core", "--ipc",
              address, "--do-wait", (sandbox ? "--sandbox" : NULL), NULL);

        exit(EXIT_FAILURE);
        break;
    default:
        while (stat(address, &st))
            usleep(100);

        char rv = einit_ipc_connect(address);

        unlink(address);
        return rv;
        break;
    }
}

char einit_disconnect()
{
    if (einit_ipc_client_pid > 0) {
        /*
         * we really gotta do this in a cleaner way... 
         */
        kill(einit_ipc_client_pid, SIGKILL);

        waitpid(einit_ipc_client_pid, NULL, 0);
    }

    /*
     * FIXME 
     */
    return 1;
}

void einit_service_call(const char *service, const char *action)
{
}

void einit_module_call(const char *rid, const char *action)
{
}

char *einit_module_get_attribute(const char *rid, const char *attribute)
{
}

char *einit_module_get_name(const char *rid)
{
}

char **einit_module_get_provides(const char *rid)
{
}

char **einit_module_get_requires(const char *rid)
{
}

char **einit_module_get_after(const char *rid)
{
}

char **einit_module_get_before(const char *rid)
{
}

char **einit_module_get_status(const char *rid)
{
}

char **einit_module_get_options(const char *rid)
{
}

void einit_event_loop()
{
    einit_ipc_request ("(request receive-events backlog)");
}

void einit_event_loop_skip_old()
{
    einit_ipc_request ("(request receive-events no-backlog)");
}

void einit_replay_events()
{
    einit_ipc_request ("(request receive-events replay-log)");
}

/*
 * (define-record einit:event type integer status task flag string
 * stringset module)
 */

const char *einit_event_encode(struct einit_event *ev)
{
    struct einit_sexp *sp = 
        se_cons(se_symbol ("event"),
        se_cons(se_symbol (event_code_to_string(ev->type)),
        se_cons(se_integer(ev->integer),
        se_cons(se_integer(ev->status),
        se_cons(se_integer(ev->task),
        se_cons(se_integer(ev->flag),
        se_cons(se_string(ev->string),
        se_cons(se_stringset_to_list(ev->stringset),
        se_cons(se_symbol(ev->rid),
                (struct einit_sexp *)sexp_end_of_list)))))))));

    char *r = einit_sexp_to_string(sp);
    const char *rv = str_stabilise (r);
    efree (r);

    einit_sexp_destroy(sp);

    return rv;
}

struct smodule *einit_decode_module_from_string(const char *s)
{
}

void einit_register_module(struct smodule *s)
{
}

char *einit_get_configuration_string(const char *key,
                                     const char *attribute)
{
}

signed int einit_get_configuration_integer(const char *key,
                                           const char *attribute)
{
    char *s =
        einit_get_configuration_string(key, attribute ? attribute : "i");

    if (s)
        return parse_integer(s);

    return 0;
}

char einit_get_configuration_boolean(const char *key,
                                     const char *attribute)
{
    char *s =
        einit_get_configuration_string(key, attribute ? attribute : "b");

    if (s)
        return parse_boolean(s);

    return 0;
}

char **einit_get_configuration_attributes(const char *key)
{
}

char ***einit_get_configuration_prefix(const char *prefix)
{
}
