/*
 *  ealloc.c
 *  ealloc
 *
 *  Created by Magnus Deininger on 01/04/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 *
 * This is a replacement for the default (m|re|c)alloc() set of functions,
 * which strictly rely on mmap() to get memory.
 *
 * This relies on an implementation of mmap() that supports MAP_ANONYMOUS
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/mman.h>

int pagesize = 0;

struct page_header {
 void *next_page;
 void *prev_page;
 size_t size;
 uint32_t pages;
};

void *current_page = NULL;

/*mmap(void *start, size_t length, int prot, int flags,
     int fd, off_t offset);*/

void *malloc(size_t s) {
 size_t pages = 0;

 if (!pagesize) pagesize = sysconf(_SC_PAGESIZE);

 pages = ((s+sizeof(struct page_header)) / pagesize) +1;

// fprintf (stderr, "allocating %i pages for memory of size %i (pagesize = %i).\n", pages, s, pagesize);

 void *p = mmap(NULL, pages*pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

 if (p == MAP_FAILED) {
  errno = ENOMEM;
  return NULL;
 }

 ((struct page_header *)p)->size = s;
 ((struct page_header *)p)->pages = pages;

 return p+sizeof(struct page_header);
}

void *calloc(size_t c, size_t s) {
 size_t l = c*s;
 void *p = malloc(l);

 if (p) memset (p, 0, l);

 return p;
}

void *realloc(void *p, size_t s) {
 if (!p) return malloc (s);
 if (!s) { free (p); return NULL; }

 void *np = NULL;
 size_t oldsize = ((struct page_header *)(p-sizeof(struct page_header)))->pages;
 size_t pages = 0;

 if (!pagesize) pagesize = sysconf(_SC_PAGESIZE);

 pages = ((s+sizeof(struct page_header)) / pagesize) +1;

// if (oldsize == pages) return p;

// fprintf (stderr, "(re)allocating %i pages for memory of size %i (pagesize = %i).\n", pages, s, pagesize);

 np = mmap(NULL, pages*pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

 if (np == MAP_FAILED) {
  errno = ENOMEM;
  return NULL;
 }

 memcpy (np+sizeof(struct page_header), p, (((struct page_header *)(p-sizeof(struct page_header)))->size));

 munmap (p-sizeof(struct page_header), oldsize * pagesize);

 ((struct page_header *)np)->size = s;
 ((struct page_header *)np)->pages = pages;

 return np+sizeof(struct page_header);
}

void free(void *p) {
 if (p) munmap (p-sizeof(struct page_header), (((struct page_header *)(p-sizeof(struct page_header)))->pages) * pagesize);
}
