/*
 *  config.h
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#ifndef _CONFIG_H
#define _CONFIG_H

#include <einit/module.h>
#include <einit/utility.h>
#include <sys/utsname.h>
#include <einit/tree.h>

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

#define BUFFERSIZE 1024
#define NODE_MODE 1

#define EINIT_VERSION 1
#define EINIT_VERSION_LITERAL_NUMBER "0.15.0"
#if ( ISSVN == 1 )
#define EINIT_VERSION_LITERAL EINIT_VERSION_LITERAL_NUMBER "-svn-" BUILDNUMBER
#else
#define EINIT_VERSION_LITERAL EINIT_VERSION_LITERAL_NUMBER
#endif

#define EI_NODETYPE_BASENODE 1
#define EI_NODETYPE_CONFIG 2
#define EI_NODETYPE_CONFIG_CUSTOM 4
#define EI_NODETYPE_MODE 8

#define EI_SIGNATURE 0xD1E0B666

#define EINIT_GMODE_INIT       0x00000001
#define EINIT_GMODE_METADAEMON 0x00000002
#define EINIT_GMODE_SANDBOX    0x00000003

struct cfgnode {
 unsigned int nodetype;
 char *id;
 uint32_t signature;
 struct cfgnode *mode;
 unsigned char flag;
 long int value;
 char *svalue;
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

uint32_t gmode;

char **einit_global_environment;

// free configuration
int cfg_free ();

// add a node to the main configuration
int cfg_addnode (struct cfgnode *);

// find a node (by id)
struct cfgnode *cfg_findnode (char *, unsigned int, struct cfgnode *);

/* these functions take the current mode into consideration */
char *cfg_getstring (char *, struct cfgnode *);          // get string (by id)
struct cfgnode *cfg_getnode (char *, struct cfgnode *);  // get node (by id)

/* those i-could've-sworn-there-were-library-functions-for-that functions */

char *cfg_getpath (char *);

#endif /* _CONFIG_H */
