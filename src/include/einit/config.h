/*
 *  config.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2006-2008, Magnus Deininger All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EINIT_CONFIG_H
#define EINIT_CONFIG_H

    struct cfgnode;

#include <einit/module.h>
#include <einit/utility.h>
#include <sys/utsname.h>
#include <einit/tree.h>
#include <einit/configuration-static.h>

#include <einit/configuration.h>

    enum einit_mode {
        einit_mode_init = 0x0001,
        einit_mode_sandbox = 0x0010,
        einit_mode_ipconly = 0x0020,
        einit_core_exiting = 0x1000
    };

    struct cfgnode {
        char *id;
        char *modename;

        /*
         * data 
         */
        unsigned char flag;
        long int value;
        char *svalue;

        /*
         * arbitrary attributes + shortcuts 
         */
        char **arbattrs;
        char *idattr;
    };

    struct utsname osinfo;

    char einit_new_node;

    extern enum einit_mode coremode;

    char **einit_global_environment, **einit_initial_environment;

    extern char **einit_argv;

    /*
     * use this to define functions that take a tree of configuration
     * nodes and turn it into a string (for saving) 
     */
    typedef char *(*cfg_string_converter) (const struct stree *);

    int cfg_addnode(struct cfgnode *);
    char *cfg_getstring(const char *);
    char cfg_getboolean(const char *);
    int cfg_getinteger(const char *);
    struct cfgnode *cfg_getnode(const char *);
    char *cfg_getpath(const char *);
    struct cfgnode **cfg_prefix(const char *);
    struct cfgnode **cfg_match(const char *);
    int cfg_callback_prefix(char *prefix,
                            void (*callback) (struct cfgnode *));

    void cfg_set_current_mode(char *modename);

#endif                          /* _CONFIG_H */

#ifdef __cplusplus
}
#endif
