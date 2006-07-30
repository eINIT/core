/*
 *  utility.h
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
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

#ifndef _UTILITY_H
#define _UTILITY_H

#include <inttypes.h>
#define	SET_TYPE_STRING		0
#define	SET_NOALLOC		-1

struct uhash {
 char *key;
 void *value;
 struct uhash *next;
};

/* some common functions to work with null-terminated arrays */

void **setcombine (void **, void **, int32_t);
void **setadd (void **, void *, int32_t);
void **setdup (void **, int32_t);
void **setdel (void **, void *);
int setcount (void **);

/* some functions to work with string-sets */

char **str2set (const char, char *);
int strinset (char **, const char *);
char **strsetdel (char **, char *);
char **strsetdeldupes (char **);

char **straddtoenviron (char **, char *, char *);

/* same as above, this time with hashes */

struct uhash *hashadd (struct uhash *, char *, void *, int32_t);
struct uhash *hashfind (struct uhash *, char *);
struct uhash *hashdel (struct uhash *, struct uhash *);
void hashfree (struct uhash *);
#define hashnext(h) h->next

/* safe malloc/calloc/realloc/strdup functions */

void *emalloc (size_t);
void *ecalloc (size_t, size_t);
void *erealloc (void *, size_t);
char *estrdup (char *);

/* nifty string functions */
void strtrim (char *);

#endif /* _UTILITY_H */
