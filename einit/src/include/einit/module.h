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

#define MOD_ENABLE 0x0001
#define MOD_DISABLE 0x0002
#define MOD_RELOAD 0x0004
#define MOD_RESET 0x0008
#define MOD_DISABLE_UNSPEC 0x1000
#define MOD_DISABLE_UNSPEC_FEEDBACK 0x2000
#define MOD_FEEDBACK_SHOW 0x0100

#define STATUS_IDLE 0x0000
#define STATUS_OK 0x8003
#define STATUS_ABORTED 0x8002
#define STATUS_FAIL 0x0100
#define STATUS_FAIL_REQ 0x0300
#define STATUS_DONE 0x8000
#define STATUS_WORKING 0x0010
#define STATUS_ENABLING 0x0011
#define STATUS_DISABLING 0x0012
#define STATUS_RELOADING 0x0013
#define STATUS_ENABLED 0x0401
#define STATUS_DISABLED 0x0802

#ifdef __cplusplus
extern "C"
{
#endif

struct mfeedback {
 unsigned volatile int status;
 unsigned volatile int progress;
 unsigned volatile int task;
 unsigned volatile int errorc;
 char volatile *verbose;
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

struct mloadplan {
 unsigned int task;
 struct lmodule *mod;
 struct mloadplan **left;  /* elements of the tree to load on failure */
 struct mloadplan **right; /* elements of the tree to load on success */
 struct mloadplan **orphaned; /* orphaned elements */
 char **unsatisfied;
 char **unavailable;
};

extern struct lmodule mdefault;

// scans for modules (i.e. load their .so and add it to the list)
int mod_scanmodules ();

// frees the chain of loaded modules and unloads the .so-files
int mod_freemodules ();

// adds a module to the main chain of modules
int mod_add (void *, int (*)(void *, struct mfeedback *), int (*)(void *, struct mfeedback *), void *, struct smodule *);

// find a module
struct lmodule *mod_find (char *rid, unsigned int options);

// do something to a module
int mod (unsigned int, struct lmodule *);

#define mod_enable(lname) mod (MOD_ENABLE, lname)
#define mod_disable(lname) mod (MOD_DISABLE, lname)

// create a plan for loading a set of atoms
struct mloadplan *mod_plan (struct mloadplan *, char **, unsigned int);

// actually do what the plan says
unsigned int mod_plan_commit (struct mloadplan *);

// free all of the resources of the plan
int mod_plan_free (struct mloadplan *);

#ifdef DEBUG
// lists all known modules to stdout
void mod_ls ();

// write out a plan-struct to stdout
void mod_plan_ls (struct mloadplan *);
#endif

// use this to tell einit that there is new feedback-information
// don't rely on this to be a macro!
#define status_update(a) if (mdefault.comment) mdefault.comment(a)

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
