/*
 *  einit-wtf.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/04/2008.
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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int have_issues = 0;

enum test_result {
    test_passed,
    test_failed,
    test_skipped
};

enum test_result test_rnv ()
{
    return test_skipped;
}

enum test_result test_mode_consistency ()
{
    return test_skipped;
}

void run_test (char *message, enum test_result (*test_function)())
{
    fprintf (stdout, "%s: ", message);
    fflush (stdout);

    enum test_result result = test_function();

    switch (result)
    {
        case test_passed:
            fputs ("passed\n", stdout);
            break;
        case test_failed:
            fputs ("failed\n", stdout);
            have_issues++;
            break;
        case test_skipped:
            fputs ("skipped\n", stdout);
            break;
    }
}

int main(int argc, char **argv)
{
    if (!einit_connect_spawn(&argc, argv)) {
        return 0;
    }

    run_test ("checking config files against schema", test_rnv);
    run_test ("checking services in modes", test_mode_consistency);

    return have_issues;
}
