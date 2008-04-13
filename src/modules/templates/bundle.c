/*
 *  bundle.c
 *  einit
 *
 *  Created on 14/02/2008.
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_bundle_configure(struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_bundle_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = einit_module,
    .name = module_bundle_name,
    .rid = module_bundle_rid,
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = module_bundle_configure
};

module_register(module_bundle_self);

#endif

char module_bundle_firstrun = 1;

const struct smodule **module_bundle_contents[MAXMODULES];

void module_bundle_scanmodules(struct einit_event *ev)
{
    mod_update_source(module_bundle_rid);

    if (module_bundle_firstrun) {
        module_bundle_firstrun = 0;

        if (module_bundle_contents) {
            uint32_t cp = 0;

            for (; module_bundle_contents[cp]; cp++) {
                struct lmodule *lmm =
                    mod_add(NULL, (*module_bundle_contents[cp]));

                if (lmm) {
                    lmm->source =
                        (char *) str_stabilise(module_bundle_rid);
                }
            }
        }
    }

    return;
}

int module_bundle_configure(struct lmodule *pa)
{
    module_init(pa);

    event_listen(einit_core_update_modules, module_bundle_scanmodules);

    module_bundle_scanmodules(NULL);

    return 0;
}
