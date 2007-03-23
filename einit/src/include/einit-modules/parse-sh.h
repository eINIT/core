/*
 *  parse-sh.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 08/01/2007.
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

#ifndef _EINIT_MODULES_IPC_H
#define _EINIT_MODULES_IPC_H

#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>

#define SH_PARSER_STATUS_LW              0
#define SH_PARSER_STATUS_READ            1
#define SH_PARSER_STATUS_IGNORE_TILL_EOL 2

#define PA_END_OF_FILE                   0x01
#define PA_NEW_CONTEXT                   0x02

#if (! defined(einit_modules_parse_sh)) || (einit_modules_parse_sh == 'm') || (einit_modules_parse_sh == 'n')

typedef int (*sh_parser) (const char *, void (*)(const char **, uint8_t));

sh_parser __f_parse_sh;

#define parse_sh(data, callback) ((__f_parse_sh || (__f_parse_sh = function_find_one("einit-parse-sh", 1, NULL))) ? __f_parse_sh(data, callback) : -1)

#define parse_sh_configure(mod) __f_parse_sh = NULL;
#define parse_sh_cleanup(mod) __f_parse_sh = NULL;

#else

int __parse_sh (const char *, void (*)(const char **, uint8_t));

#define parse_sh_configure(mod) ;
#define parse_sh_cleanup(mod) ;

#define parse_sh(data, callback) __parse_sh(data, callback)

#endif

#endif

#ifdef __cplusplus
}
#endif
