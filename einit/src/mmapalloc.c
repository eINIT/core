/*
 *  ealloc.c
 *  ealloc
 *
 *  Created by Magnus Deininger on 01/04/2007.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 *
 * This is a replacement for the default (m|re|c)alloc() set of functions,
 * which strictly relies on mmap() to get memory.
 * (this makes it a lot better if you're likely to need large amounts of mem)
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
};

struct page_header dummy_page = {
 .next_page = &dummy_page,
 .prev_page = &dummy_page,
 .size = 0
};
struct page_header *current_page = &dummy_page;

#define size2pages(size) ((((size)+sizeof(struct page_header)) / pagesize) +1)
#if 0
#define init_page(page, s)\
 ((struct page_header *)(page))->next_page = current_page;\
 ((struct page_header *)(page))->prev_page = current_page->prev_page;\
 current_page->next_page = (page);\
 current_page = (page);\
 ((struct page_header *)(page))->size = s;
#else
#define init_page(page, s)\
 ((struct page_header *)(page))->size = s;
#endif

#define remove_page(page)\
 current_page = ((struct page_header *)(page))->next_page;\
 (((struct page_header *)(page))->next_page)->prev_page = ((struct page_header *)(page))->prev_page;\
 (((struct page_header *)(page))->prev_page)->next_page = ((struct page_header *)(page))->next_page;

void *mmap_malloc(size_t s) {
 size_t pages = 0;

 if (!pagesize) pagesize = sysconf(_SC_PAGESIZE);

 pages = size2pages(s);

 retry:

 void *p = mmap(NULL, pages*pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

 if (p == MAP_FAILED) {
/*  errno = ENOMEM;
  return NULL;*/

  sleep 1;
  goto retry;
 }

 init_page (p, s);

 return p+sizeof(struct page_header);
}

void *mmap_calloc(size_t c, size_t s) {
 size_t l = c*s;
 void *p = mmap_malloc(l);

 if (p) memset (p, 0, l);

 return p;
}

void *mmap_realloc(void *p, size_t s) {
 if (!p) return mmap_malloc (s);
 if (!s) { free (p); return NULL; }

 void *np = NULL;
 size_t oldsize = size2pages(((struct page_header *)(p-sizeof(struct page_header)))->size);
 size_t pages = 0;

 if (!pagesize) pagesize = sysconf(_SC_PAGESIZE);

 pages = size2pages(s);

 if (oldsize == pages) {
  ((struct page_header *)(p-sizeof(struct page_header)))->size = s;

  return p;
 }

 retry:

 np = mmap(NULL, pages*pagesize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

 if (np == MAP_FAILED) {
/*  errno = ENOMEM;
  return NULL; */

  sleep (1);
  goto retry;
 }

 memcpy (np+sizeof(struct page_header), p, (((struct page_header *)(p-sizeof(struct page_header)))->size));

// remove_page(p-sizeof(struct page_header));

 munmap (p-sizeof(struct page_header), oldsize * pagesize);

 init_page (np, s);

 return np+sizeof(struct page_header);
}

void mmap_free(void *p) {
// remove_page(p-sizeof(struct page_header));

 if (p) munmap (p-sizeof(struct page_header), size2pages(((struct page_header *)(p-sizeof(struct page_header)))->size) * pagesize);
}
