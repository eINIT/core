/*
 *  bitch.h
 *  einit
 *
 *  Created by Magnus Deininger on 14/02/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef __cplusplus
extern "C" {
#endif

/*!\file einit/bitch.h
 * \brief Error-reporting functions
 * \author Magnus Deininger
 *
 * Error reporting (a.k.a. "bitching") is fairly important...
*/

#ifndef EINIT_BITCH_H
#define EINIT_BITCH_H

#include <errno.h>

enum bitch_sauce {
 bitch_bad_sauce = 0x00,
 bitch_emalloc   = 0x01,
 bitch_stdio     = 0x02,
 bitch_regex     = 0x03,
 bitch_expat     = 0x04,
 bitch_dl        = 0x05,
 bitch_lookup    = 0x06,
 bitch_epthreads = 0x07
};

#define BITCH_SAUCES 0x08

/*!\brief Bitch about whatever happened just now
 * \param[in] opt bitwise OR of BTCH_ERRNO and BTCH_DL
 * \return (int)-1. Don't ask.
 *
 * Bitch about whatever happened just now, i.e. report the last error.
*/
#ifdef DEBUG
#define bitch(sauce, code, reason)\
 bitch_macro (sauce, __FILE__, __LINE__, __func__ , code, reason)

#else

#define bitch(sauce, code, reason)\
 bitch_macro (sauce, "", __LINE__, __func__ , code, reason)

#endif

int bitch_macro (enum bitch_sauce sauce, const char *file, const int line, const char *function, int error, const char *reason);

#ifdef DEBUG

#define eprintf(file, format, ...)\
 ((fprintf(file, format, __VA_ARGS__) < 0) ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , 0, "fprintf() failed."), errno) : 0)

#define esprintf(buffer, size, format, ...)\
 (snprintf(buffer, size, format, __VA_ARGS__) < 0 ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , 0, "snprintf() failed."), errno) : 0)

#define eputs(text, file)\
 (fputs(text, file) < 0 ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , 0, "fputs() failed."), errno) : 0)

#define efclose(stream)\
 ((fclose(stream) == EOF) ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , errno, "fclose() failed"), EOF): 0)

#define eclosedir(dir)\
 (closedir(dir) ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , errno, "closedir() failed"), -1): 0)

#define eclose(fd)\
 (close(fd) ? (bitch_macro (bitch_stdio, __FILE__, __LINE__, __func__ , errno, "close() failed"), -1): 0)

#else


#define eprintf(file, format, ...)\
 fprintf(file, format, __VA_ARGS__)

#define esprintf(buffer, size, format, ...)\
 snprintf(buffer, size, format, __VA_ARGS__)

#define eputs(text, file)\
 fputs(text, file)

#define efclose(stream)\
 fclose(stream)

#define eclosedir(dir)\
 closedir(dir)

#define eclose(fd)\
 close(fd)

#endif

#define eregcomp(target, pattern)\
 ((errno = eregcomp_cache(target, (pattern), REG_EXTENDED)) ? (bitch_macro (bitch_regex, __FILE__, __LINE__, __func__ , errno, "could not compile regular expression."), errno) : 0)

#define eregfree(x) eregfree_cache(x)

#else

#endif /* _BITCH_H */

#ifdef __cplusplus
}
#endif
