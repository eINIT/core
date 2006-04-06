/***************************************************************************
 *            config.h
 *
 *  Mon Feb  6 15:41:57 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
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

#define BUFFERSIZE 1024
#define NODE_MODE 1

#define EINIT_VERSION 1
#define EINIT_VERSION_LITERAL "0.01"

#define EI_NODETYPE_BASENODE 1
#define EI_NODETYPE_CONFIG 2
#define EI_NODETYPE_CONFIG_CUSTOM 4
#define EI_NODETYPE_MODE 8

#ifdef __cplusplus
extern "C"
{
#endif

struct cfgnode {
 unsigned int nodetype;
 char *id;
 struct cfgnode *next;
 unsigned char flag;
 long int value;
 char *svalue;
 char **enable;
 char **disable;
 char **base;
 char **arbattrs;
 void *custom;
};

struct sconfiguration {
 int eiversion;
 int version;
 unsigned int options;
 char **arbattrs;
 struct cfgnode *node;
};

#ifndef _MODULE
char *configfile;
#endif

struct sconfiguration *sconfiguration;

// load configuration
int cfg_load ();

// free configuration
int cfg_free ();

// free a node and all its descendants
int cfg_freenode (struct cfgnode *);

// add a node to the main configuration
int cfg_addnode (struct cfgnode *);

// remove a node from the main configuration (by pointer)
int cfg_delnode (struct cfgnode *);

// find a node (by id)
struct cfgnode *cfg_findnode (char *, unsigned int);

// replace a node with a new one
int cfg_replacenode (struct cfgnode *, struct cfgnode *);

#ifdef __cplusplus
}
#endif

#endif /* _CONFIG_H */
