/*
 *  bitch.h
 *  einit
 *
 *  Created by Magnus Deininger on 14/02/2006.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*!\file einit/bitch.h
 * \brief Error-reporting functions
 * \author Magnus Deininger
 *
 * Error reporting (a.k.a. "bitching") is fairly important...
*/

#ifndef _BITCH_H
#define _BITCH_H

#include <errno.h>

#define BITCH_BAD_SAUCE 0x00
#define BITCH_EMALLOC   0x01
#define BITCH_STDIO     0x02
#define BITCH_REGEX     0x03
#define BITCH_EXPAT     0x04
#define BITCH_DL        0x05
#define BITCH_LOOKUP    0x06
#define BITCH_EPTHREADS 0x07

#define BITCH_SAUCES (BITCH_EPTHREADS + 1)

unsigned char mortality[BITCH_SAUCES];

/*!\brief Bitch about whatever happened just now
 * \param[in] opt bitwise OR of BTCH_ERRNO and BTCH_DL
 * \return (int)-1. Don't ask.
 *
 * Bitch about whatever happened just now, i.e. report the last error.
*/
#define bitch(sauce, code, reason)\
 bitch_macro (sauce, __FILE__, __LINE__, __func__ , code, reason)

int bitch_macro (const unsigned char sauce, const char *file, const int line, const char *function, int error, const char *reason);

#ifdef DEBUG

/* debug messages... don't care if those can't be written */
#define debug(message)\
 fprintf(stderr, "DEBUG: %s:%i(%s): %s\n", __FILE__, __LINE__, __func__, message), fflush (stderr)

#if 0
#define emutex_lock(mutex)\
 ((debug("pthread_mutex_lock() called."), (errno = pthread_mutex_lock(mutex)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_lock() failed."), errno) : errno), debug("pthread_mutex_lock() done."), errno)

#define emutex_unlock(mutex)\
 ((debug("pthread_mutex_unlock() called."), (errno = pthread_mutex_unlock(mutex)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_lock() failed."), errno) : errno), debug("pthread_mutex_unlock() done."), errno)
#endif

#else

#define debug(message)\
 0

#endif

#define emutex_lock(mutex)\
 ((errno = pthread_mutex_lock(mutex)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_lock() failed."), errno) : errno)

#define emutex_unlock(mutex)\
 ((errno = pthread_mutex_unlock(mutex)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_unlock() failed."), errno) : errno)

#define emutex_init(mutex, mattr)\
 ((errno = pthread_mutex_init(mutex, mattr)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_init() failed."), errno) : errno)

#define emutex_destroy(mutex)\
 ((errno = pthread_mutex_destroy(mutex)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_mutex_destroy() failed."), errno) : errno)

#define ethread_create(th, tattr, function, fattr)\
 ((errno = pthread_create(th, tattr, function, fattr)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_create() failed."), errno) : errno)

#define ethread_cancel(th)\
 ((errno = pthread_cancel(th)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_cancel() failed."), errno) : errno)

#define ethread_join(th, ret)\
 ((errno = pthread_join(th, ret)) ? (bitch_macro (BITCH_EPTHREADS, __FILE__, __LINE__, __func__ , errno, "pthread_join() failed."), errno) : errno)

#define eprintf(file, format, ...)\
 ((fprintf(file, format, __VA_ARGS__) < 0) ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , 0, "fprintf() failed."), errno) : 0)

#define esprintf(buffer, size, format, ...)\
 (snprintf(buffer, size, format, __VA_ARGS__) < 0 ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , 0, "snprintf() failed."), errno) : 0)

#define eputs(text, file)\
 (fputs(text, file) < 0 ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , 0, "fputs() failed."), errno) : 0)

#ifdef POSIXREGEX
#define eregcomp(target, pattern)\
 ((errno = regcomp(target, (pattern), REG_EXTENDED)) ? (bitch_macro (BITCH_REGEX, __FILE__, __LINE__, __func__ , errno, "could not compile regular expression."), errno) : 0)
#endif

#define efclose(stream)\
 ((fclose(stream) == EOF) ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , errno, "fclose() failed"), EOF): 0)

#define eclosedir(dir)\
 (closedir(dir) ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , errno, "closedir() failed"), -1): 0)

#define eclose(fd)\
 (close(fd) ? (bitch_macro (BITCH_STDIO, __FILE__, __LINE__, __func__ , errno, "close() failed"), -1): 0)

#endif /* _BITCH_H */
