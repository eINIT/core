/*
 *  einit-test.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/04/2008.
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

#include <einit/einit.h>
#include <einit/ipc.h>
#include <einit/sexp.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

void callback(struct einit_sexp *sexp)
{
    char *r = einit_sexp_to_string(sexp);
    fprintf(stderr, "|%s|\n", r);
    efree(r);

    einit_sexp_destroy(sexp);
}

int main(int argc, char **argv)
{
#if 0
    int fd = open("test.sexp", O_RDONLY);

    if (fd > 0) {
        struct einit_sexp_fd_reader *rd = einit_create_sexp_fd_reader(fd);
        struct einit_sexp *sexp;

        while ((sexp = einit_read_sexp_from_fd_reader(rd)) != sexp_bad) {
            if (!sexp)
                continue;

            char *r = einit_sexp_to_string(sexp);
            fprintf(stderr, "|%s|\n", r);

            efree(r);
            einit_sexp_destroy(sexp);
        }

        close(fd);
    } else {
        perror("open(test.sexp)");
    }
#else
    if (!einit_connect(&argc, argv)) {
        return 0;
    }

#if 0
    char ***p = einit_get_configuration_prefix("c");
    if (p) {
        int i = 0;
        for (; p[i]; i++) {
            fprintf (stderr, "key=%s, arbattrs=(%s)\n", p[i][0], set2str(' ', p[i]+1));
        }
    }
#endif

    /*
     * einit_ipc_request_callback(REQUEST, callback);
     * 
     * einit_ipc_loop_infinite();
     */
#endif

    return 0;
}
