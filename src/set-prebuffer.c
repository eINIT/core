/*
 *  set-prebuffer.c
 *  einit
 *
 *  Created on 02/12/2007.
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <errno.h>
#include <limits.h>
#include <sys/types.h>

#define SET_POINTERS 32
#define SET_CHUNKSIZE 1024

/* some common functions to work with null-terminated arrays */

struct chunk_data_block {
 size_t real_size;
};

void **setadd (void **set, const void *item, int32_t esize) {
 if (!item) return set;
 else {
  void **newset = NULL;
  char need_reindex = 0;
  size_t size = 0, oldsize = 0;
  int elements = 0, i = 0;
  ssize_t indexsize = 0, oldindexsize = 0;
  uintptr_t c = 0;
  div_t dt;
  struct chunk_data_block *db = NULL;

/* get the new exact number of elements, i.e. elements+1 */
  if (set) {
   do { elements++; } while (set[elements]);

/* if we have SET_POINTERS*n new elements, that means we had SET_POINTERS-1 + the NULL before,
   so we need to reindex */
   if ((elements % SET_POINTERS) == 0)
    need_reindex = 1;

   elements++;

   dt = div (elements +1, SET_POINTERS);
   indexsize = (dt.quot + ((dt.rem == 0) ? 0 : 1)) * SET_POINTERS * sizeof(void *);
//   fprintf (stderr, "index blocks (%i elements): %i (%i)\n", elements, (dt.quot + ((dt.rem == 0) ? 0 : 1)), indexsize);

   dt = div (elements, SET_POINTERS);
   oldindexsize = (dt.quot + ((dt.rem == 0) ? 0 : 1)) * SET_POINTERS * sizeof(void *);
//   fprintf (stderr, "old index blocks: %i (%i)\n", (dt.quot + ((dt.rem == 0) ? 0 : 1)), oldindexsize);

   indexsize += sizeof (struct chunk_data_block);
   oldindexsize += sizeof (struct chunk_data_block);

   db = ((char *)set) + oldindexsize - sizeof (struct chunk_data_block);

#if 1
   oldsize = db->real_size;

   switch (esize) {
    case SET_NOALLOC:
     size = indexsize;
     break;
    case SET_TYPE_STRING:
     size = (db->real_size - oldindexsize + indexsize) + strlen  (item) + 1;
     break;
    default:
     size = indexsize + elements * esize;
     break;
   }

//   fprintf (stderr, "sizes: %i, %i (type=%i)", oldsize, size, esize);
//   fflush (stderr);
#else
   oldsize = oldindexsize;
   size = indexsize;

   switch (esize) {
    case SET_NOALLOC: break;
    case SET_TYPE_STRING:
     for (i = 0; set[i]; i++) {
//      fprintf (stderr, "count %s\n", set[i]);
//      fflush (stderr);
	  size_t l = strlen  (set[i]) + 1;

      size += l;
      oldsize += l;
	 }

     size += strlen  (item) + 1;
     break;
    default:
     oldsize += (elements -1) * esize;
     size += elements * esize;
     break;
   }
#endif

   need_reindex = (oldindexsize != indexsize);
  } else {
   elements = 1;
   oldsize = 0;
   indexsize = (SET_POINTERS * sizeof (void *)) + sizeof (struct chunk_data_block);
//   fprintf (stderr, "index blocks: 1 (%i)\n", indexsize);

   switch (esize) {
    case SET_NOALLOC:
     size = indexsize;
     break;
    case SET_TYPE_STRING:
     size = indexsize + strlen (item) + 1;
     break;
    default:
     size = indexsize + esize;
     break;
   }
  }

/* at this point we got the minimum sizes we need... so we need to make that a multiple of SET_CHUNKSIZE*/
  dt = div (size, SET_CHUNKSIZE);
//  size = (dt.quot + ((dt.rem == 0) ? 0 : 1)) * SET_CHUNKSIZE;
//  fprintf (stderr, "chunks: %i (%i)\n", (dt.quot + ((dt.rem == 0) ? 0 : 1)), size);

  dt = div (oldsize, SET_CHUNKSIZE);
  oldsize = (dt.quot + ((dt.rem == 0) ? 0 : 1)) * SET_CHUNKSIZE;
//  fprintf (stderr, "old chunks: %i (%i)\n", (dt.quot + ((dt.rem == 0) ? 0 : 1)), oldsize);
//  fflush (stderr);

  if (need_reindex || (size != oldsize)) {
   newset = emalloc (size);
   memset (newset, 0, size);
//   fprintf (stderr, "resize %i (block @%x)\n", size, newset);
//   fflush (stderr);

   db = ((char *)newset) + indexsize - sizeof (struct chunk_data_block);
   db->real_size = indexsize;

//   fprintf (stderr, "cdb @%x\n", db);
//   fflush (stderr);

   if (set) {
//    fprintf (stderr, "copy\n");
    switch (esize) {
     case SET_NOALLOC:
      for (i = 0; set[i]; i++) {
	   newset[i] = set[i];
      }
      break;
     case SET_TYPE_STRING:
      c = (uintptr_t)(((char *)newset) + indexsize);
      for (i = 0; set[i]; i++) {
	   int l = strlen (set[i]) + 1;

	   newset[i] = (void *)c;
	   memcpy (newset[i], set[i], l);
	   c += l;
       db->real_size += l;
	  }

      break;
     default:
      for (i = 0; set[i]; i++) {
	   newset[i] = ((char *)newset) + indexsize + (i * esize);
	   memcpy (newset[i], set[i], esize);
       db->real_size += esize;
	  }
      break;
    }

    free (set);
   }

   set = newset;
  } else {
   newset = set;
  }

  switch (esize) {
   case SET_NOALLOC:
    newset[elements -1] = (void *)item;
    break;
   case SET_TYPE_STRING:
    c = (uintptr_t)(((char *)newset) + indexsize);
    for (i = 0; newset[i]; i++) {
	 size_t l = strlen (newset[i]) + 1;
     c += l;
    }

    newset[i] = (void *)c;
	{
	 size_t l = strlen (item) + 1;
     memcpy (newset[i], item, l);
     db->real_size += l;
	}
    break;
   default:
    i = elements - 1;
    newset[i] = ((char *)newset) + indexsize + (i * esize);
    memcpy (newset[i], item, esize);
    db->real_size += esize;
    break;
  }

  return newset;
 }
}

void **setdup (const void **set, int32_t esize) {
 void **newset = NULL;
 uint32_t y = 0;
 if (!set) return NULL;

 for (; set[y]; y++)
  newset = setadd (newset, set[y], esize);

 return newset;
}

/* some functions to work with string-sets */

char **str2set (const char sep, const char *oinput) {
 int i;
 char *input, *li, **ret = NULL;
 if (!oinput || !(input = estrdup (oinput))) return NULL;

 li = input;

 for (i = 0; input[i]; i++) {
  if (input[i] == sep) {
   input[i] = 0;
   if (li[0] != 0)
    ret = (char **)setadd ((void **)ret, li, SET_TYPE_STRING);
   li = input + i +1;
   input[i] = sep;
  }
 }
 if (li[0] != 0)
  ret = (char **)setadd ((void **)ret, li, SET_TYPE_STRING);

 efree (input);
 return ret;
}
