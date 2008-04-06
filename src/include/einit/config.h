/*
 *  config.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
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

enum einit_cfg_node_options {
 einit_node_regular = 0x02,
 einit_node_mode = 0x08
};

enum einit_mode {
 einit_mode_init       = 0x0001,
 einit_mode_sandbox    = 0x0010,
 einit_mode_ipconly    = 0x0020,
 einit_core_exiting    = 0x1000
};

struct cfgnode {
 enum einit_cfg_node_options type;
 char *id;
 struct cfgnode *mode;

/* data */
 unsigned char flag;
 long int value;
 char *svalue;

/* arbitrary attributes + shortcuts */
 char **arbattrs;
 char *idattr;
};

struct utsname osinfo;

struct cfgnode *cmode, *amode;

char einit_new_node;

extern enum einit_mode coremode;

char **einit_global_environment,
     **einit_initial_environment;

extern char **einit_argv;

/* use this to define functions that take a tree of configuration nodes and turn it into a string (for saving) */
typedef char *(*cfg_string_converter) (const struct stree *);

#if (! defined(einit_modules_00_configuration_stree)) || (einit_modules_00_configuration_stree == 'm') || (einit_modules_00_configuration_stree == 'n')

struct exported_function *cfg_addnode_fs;
struct exported_function *cfg_findnode_fs;
struct exported_function *cfg_getstring_fs;
struct exported_function *cfg_getnode_fs;
struct exported_function *cfg_getpath_fs;
struct exported_function *cfg_prefix_fs;
struct exported_function *cfg_callback_prefix_fs;

#define config_configure() cfg_addnode_fs = NULL; cfg_findnode_fs = NULL; cfg_getstring_fs = NULL; cfg_getnode_fs = NULL; cfg_getpath_fs = NULL; cfg_prefix_fs = NULL;

#define cfg_addnode(node) function_call_by_name_use_data (int, "einit-configuration-node-add", 1, cfg_addnode_fs, -1, node)
#define cfg_findnode(name, mode, node) function_call_by_name_use_data (struct cfgnode *, "einit-configuration-node-get-find", 1, cfg_findnode_fs, NULL, name, mode, node)
#define cfg_getstring(id, base) function_call_by_name_use_data (char *, "einit-configuration-node-get-string", 1, cfg_getstring_fs, NULL, id, base)
#define cfg_getnode(id, base) function_call_by_name_use_data (struct cfgnode *, "einit-configuration-node-get", 1, cfg_getnode_fs, NULL, id, base)
#define cfg_getpath(id) function_call_by_name_use_data (char *, "einit-configuration-node-get-path", 1, cfg_getpath_fs, NULL, id)
#define cfg_prefix(prefix) function_call_by_name_use_data (struct stree *, "einit-configuration-node-get-prefix", 1, cfg_prefix_fs, NULL, prefix)

#define cfg_callback_prefix(prefix,callback) function_call_by_name_use_data (int, "einit-configuration-callback-prefix", 1, cfg_callback_prefix_fs, 0, prefix, callback)

#else

int cfg_addnode_f (struct cfgnode *);
struct cfgnode *cfg_findnode_f (const char *, enum einit_cfg_node_options, const struct cfgnode *);
char *cfg_getstring_f (const char *, const struct cfgnode *);
struct cfgnode *cfg_getnode_f (const char *, const struct cfgnode *);
char *cfg_getpath_f (const char *);
char *cfg_prefix_f (const char *);
int cfg_callback_prefix_f (char *prefix, void (*callback)(struct cfgnode *));

#define config_configure() ;

#define cfg_addnode(node) cfg_addnode_f (node)
#define cfg_findnodenode(name, mode, node) cfg_addnode_f (name, mode, node)
#define cfg_getstring(id, base) cfg_getstring_f(id, base)
#define cfg_getnode(id, base) cfg_getnode_f(id, base)
#define cfg_getpath(id) cfg_getpath_f(id)
#define cfg_prefix(filter) cfg_prefix_f(filter)

#define cfg_callback_prefix(prefix,callback) cfg_callback_prefix_f(prefix, callback)

#endif

#endif /* _CONFIG_H */

#ifdef __cplusplus
}
#endif
