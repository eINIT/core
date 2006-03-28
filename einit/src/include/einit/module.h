/***************************************************************************
 *            module.h
 *
 *  Mon Feb  6 15:27:11 2006
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

#ifndef _MODULE_H
#define _MODULE_H

#define EINIT_OPT_WAIT 8
#define EINIT_OPT_ONCE 16
#define EINIT_OPT_KEEPALIVE 32

#define EINIT_MOD_LOADER 1
#define EINIT_MOD_FEEDBACK 2
#define EINIT_MOD_EXEC 4

#define LOAD_OK 1
#define LOAD_FAIL -1
#define LOAD_FAIL_REQ -2

#define EI_VIS_TASK_LOAD 1
#define EI_VIS_TASK_UNLOAD 2
#define EI_VIS_TASK_RELOAD 4
#define EI_VIS_TASK_RESET 8

#define EINIT_

#define STATUS_IDLE 0
#define STATUS_OK 1
#define STATUS_ABORTED 2
#define STATUS_WORKING 4
#define STATUS_LOADING 12
#define STATUS_UNLOADING 20
#define STATUS_RELOADING 28
#define STATUS_LOADED 33

#ifdef __cplusplus
extern "C"
{
#endif

struct mfeedback {
 unsigned volatile int status;
 unsigned volatile int progress;
 unsigned volatile int task;
 unsigned volatile char changed;
 struct lmodule * module;
};

struct smodule {
 int eiversion;
 int version;
 int mode;
 unsigned int options;
 char *name;
 char *rid;
 char **provides;
 char **requires;
 char **notwith;
};

struct lmodule {
 void *sohandle;
 int (*enable)  (void *, struct mfeedback *);
 int (*disable) (void *, struct mfeedback *);
 int (*comment) (struct mfeedback *);
 int (*cleanup) (struct lmodule *);
 unsigned int status;
 void *param;
 struct smodule *module;
 struct lmodule *next;
};

struct mdeptree {
 struct lmodule *mod;
 struct mdeptree *alternative;
 struct mdeptree *next;
 struct mdeptree *down;
};

// scans for modules (i.e. load their .so and add it to the list)
int mod_scanmodules ();

// frees the chain of loaded modules and unloads the .so-files
int mod_freemodules ();

// lists all known modules to stdout
void mod_ls ();

// adds a module to the main chain of modules
int mod_add (void *, int (*)(void *, struct mfeedback *), int (*)(void *, struct mfeedback *), void *, struct smodule *);

// find a module
struct lmodule *mod_find (char *rid, unsigned int options);

// enable a module
int mod_enable (struct lmodule *);

// disable a module
int mod_disable (struct lmodule *);

// create the dependency tree for a module or a series of them
struct mdeptree *mod_create_deptree (char **);

// free a dependency tree
void mod_free_deptree (struct mdeptree *);

// make things ready to be run
int mod_configure ();

#ifdef __cplusplus
}
#endif

/* what your module would usually have to define:
//
// _ALL_ modules:
// struct smodule self;
//
// modules that want to configure themselves:
// int configure (struct lmodule *);
//
// feedback-modules:
// int comment (struct mfeedback *status);
//
// modules that load something:
// int load (void *pa, struct mfeedback *status);
// int unload (void *pa, struct mfeedback *status);
//
// modules for loading different types of modules:
// int scanmodules (struct lmodule *);
//
// ------------------------------------------------ */

#endif /* _MODULE_H */
