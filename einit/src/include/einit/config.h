/*
 *  config.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#include <einit/configuration.h>

#define BSDLICENSE "All rights reserved.\n"\
 "\n"\
 "Redistribution and use in source and binary forms, with or without modification,\n"\
 "are permitted provided that the following conditions are met:\n"\
 "\n"\
 "    * Redistributions of source code must retain the above copyright notice,\n"\
 "      this list of conditions and the following disclaimer.\n"\
 "    * Redistributions in binary form must reproduce the above copyright notice,\n"\
 "      this list of conditions and the following disclaimer in the documentation\n"\
 "      and/or other materials provided with the distribution.\n"\
 "    * Neither the name of the project nor the names of its contributors may be\n"\
 "      used to endorse or promote products derived from this software without\n"\
 "      specific prior written permission.\n"\
 "\n"\
 "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n"\
 "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"\
 "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"\
 "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR\n"\
 "ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n"\
 "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n"\
 "LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON\n"\
 "ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"\
 "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n"\
 "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"

#define NODE_MODE 1

#define EINIT_VERSION 1

#if defined (ISSVN) && (ISSVN > 0)
#define EINIT_VERSION_LITERAL_NUMBER "live"
#else
#define EINIT_VERSION_LITERAL_NUMBER "0.23.5"
#endif

#define EINIT_VERSION_LITERAL EINIT_VERSION_LITERAL_NUMBER EINIT_VERSION_LITERAL_SUFFIX

enum einit_cfg_node_options {
 einit_node_regular = 0x02,
 einit_node_mode = 0x08,
 einit_node_modified = 0x10
};

enum einit_mode {
 einit_mode_init       = 0x0001,
 einit_mode_metadaemon = 0x0002,
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
 char *path;
 char *source;
 char *source_file;
};

struct stree *hconfiguration;
struct utsname osinfo;
pthread_attr_t thread_attribute_detached;

struct cfgnode *cmode, *amode;

time_t boottime;
char einit_new_node;

enum einit_mode coremode;
unsigned char *gdebug;

char **einit_global_environment,
     **einit_initial_environment;

int einit_have_feedback;

/* use this to define functions that take a tree of configuration nodes and turn it into a string (for saving) */
typedef char *(*cfg_string_converter) (const struct stree *);

#if (! defined(einit_modules_bootstrap_configuration_stree)) || (einit_modules_bootstrap_configuration_stree == 'm') || (einit_modules_bootstrap_configuration_stree == 'n')

typedef int (*cfg_addnode_t) (struct cfgnode *);
typedef struct cfgnode *(*cfg_findnode_t) (const char *, enum einit_cfg_node_options, const struct cfgnode *);
typedef char *(*cfg_getstring_t) (const char *, const struct cfgnode *);
typedef struct cfgnode *(*cfg_getnode_t) (const char *, const struct cfgnode *);
typedef struct stree *(*cfg_filter_t) (const char *, enum einit_cfg_node_options);
typedef char *(*cfg_getpath_t) (const char *);
typedef struct stree *(*cfg_prefix_t) (const char *);

cfg_addnode_t cfg_addnode_fp;
cfg_findnode_t cfg_findnode_fp;
cfg_getstring_t cfg_getstring_fp;
cfg_getnode_t cfg_getnode_fp;
cfg_filter_t cfg_filter_fp;
cfg_getpath_t cfg_getpath_fp;
cfg_prefix_t cfg_prefix_fp;

#define config_configure() cfg_addnode_fp = NULL; cfg_findnode_fp = NULL; cfg_getstring_fp = NULL; cfg_getnode_fp = NULL; cfg_filter_fp = NULL; cfg_getpath_fp = NULL; cfg_prefix_fp = NULL;
#define config_cleanup() cfg_addnode_fp = NULL; cfg_findnode_fp = NULL; cfg_getstring_fp = NULL; cfg_getnode_fp = NULL; cfg_filter_fp = NULL; cfg_getpath_fp = NULL; cfg_prefix_fp = NULL;


#define cfg_addnode(node) ((cfg_addnode_fp || (cfg_addnode_fp = function_find_one("einit-configuration-node-add", 1, NULL))) ? cfg_addnode_fp(node) : -1)

#define cfg_findnode(name, mode, node) ((cfg_findnode_fp || (cfg_findnode_fp = function_find_one("einit-configuration-node-get-find", 1, NULL))) ? cfg_findnode_fp(name, mode, node) : NULL)

#define cfg_getstring(id, base) ((cfg_getstring_fp || (cfg_getstring_fp = function_find_one("einit-configuration-node-get-string", 1, NULL))) ? cfg_getstring_fp(id, base) : NULL)

#define cfg_getnode(id, base) ((cfg_getnode_fp || (cfg_getnode_fp = function_find_one("einit-configuration-node-get", 1, NULL))) ? cfg_getnode_fp(id, base) : NULL)

#define cfg_getpath(id) ((cfg_getpath_fp || (cfg_getpath_fp = function_find_one("einit-configuration-node-get-path", 1, NULL))) ? cfg_getpath_fp(id) : NULL)

#define cfg_filter(filter, i) ((cfg_filter_fp || (cfg_filter_fp = function_find_one("einit-configuration-node-get-filter", 1, NULL))) ? cfg_filter_fp(filter, i) : NULL)

#define cfg_prefix(prefix) ((cfg_prefix_fp || (cfg_prefix_fp = function_find_one("einit-configuration-node-get-prefix", 1, NULL))) ? cfg_prefix_fp(prefix) : NULL)

#else

int cfg_addnode_f (struct cfgnode *);
struct cfgnode *cfg_findnode_f (const char *, enum einit_cfg_node_options, const struct cfgnode *);
char *cfg_getstring_f (const char *, const struct cfgnode *);
struct cfgnode *cfg_getnode_f (const char *, const struct cfgnode *);
struct stree *cfg_filter_f (const char *, enum einit_cfg_node_options);
char *cfg_getpath_f (const char *);
char *cfg_prefix_f (const char *);

#define config_configure() ;
#define config_cleanup() ;

#define cfg_addnode(node) cfg_addnode_f (node)
#define cfg_findnodenode(name, mode, node) cfg_addnode_f (name, mode, node)
#define cfg_getstring(id, base) cfg_getstring_f(id, base)
#define cfg_getnode(id, base) cfg_getnode_f(id, base)
#define cfg_getpath(id) cfg_getpath_f(id)
#define cfg_filter(filter, i) cfg_filter_f(filter, i)
#define cfg_prefix(filter, i) cfg_prefix_f(filter, i)

#endif

#endif /* _CONFIG_H */

#ifdef __cplusplus
}
#endif
