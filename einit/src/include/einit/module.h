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

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _MODULE_H
#define _MODULE_H

#define EINIT_OPT_WAIT 8
#define EINIT_OPT_ONCE 16
#define EINIT_OPT_KEEPALIVE 32

#define EINIT_MOD_LOADER 1
#define EINIT_MOD_FEEDBACK 2
#define EINIT_MOD_COMMAND 4

#ifdef __cplusplus
extern "C"
{
#endif

struct smodule {
 int eiversion;
 int version;
 int mode;
 unsigned int options;
 char *name;
 char *rid;
 char **provides;
 char **requires;
};

struct lmodule {
 void *sohandle;
 int (*func)(void *);
 void *param;
 struct smodule *module;
 struct lmodule *next;
};

#ifndef _MODULE
struct lmodule *mlist;
#endif

int mod_scanmodules ();
int mod_freemodules ();
void mod_lsmod ();

#ifdef __cplusplus
}
#endif

#endif /* _MODULE_H */
