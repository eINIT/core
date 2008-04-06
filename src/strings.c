/*
 *  utility.c
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006-2008 Magnus Deininger
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

#include <einit/utility.h>
#include <einit/bitch.h>
#include <einit/configuration.h>


#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

// Taken from http://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
#define HAS_ZERO_BYTE(v)  (~((((v & 0x7F7F7F7FUL) + 0x7F7F7F7FUL) | v) | 0x7F7F7F7FUL))

// data is guarranteed to be 32-bit aligned)
uint32_t StrSuperFastHash (const char * data, int *len) {
 uint32_t hash = 0, tmp, lw, mask, *p32;
 int rem = 0;
 char *p = (char *)data;
 *len = 0;

 if (data == NULL) return 0;

    // this unrolls to better optimized code on most compilers
 do {
  p32 = (uint32_t *)(p);
  lw = *p32;
  mask = HAS_ZERO_BYTE(lw);
        // if mask == 0 p32 has no zero in it
  if (mask == 0) {
             // Do the hash calculation
   hash  += get16bits (p);
   tmp    = (get16bits (p+2) << 11) ^ hash;
   hash   = (hash << 16) ^ tmp;
   hash  += hash >> 11;
   p += sizeof(uint32_t);
             // Increase len as well
   (*len)++;
  }
 } while (mask == 0);

    // Now we've got out of the loop, because we hit a zero byte,
    // find out which exactly
 if (p[0] == '\0')
  rem = 0;
 if (p[1] == '\0')
  rem = 1;
 if (p[2] == '\0')
  rem = 2;
 if (p[3] == '\0')
  rem = 3;

 /* Handle end cases */
 switch (rem) {
  case 3: hash += get16bits (p);
  hash ^= hash << 16;
  hash ^= p[2] << 18;
  hash += hash >> 11;
  break;
  case 2: hash += get16bits (p);
  hash ^= hash << 11;
  hash += hash >> 17;
  break;
  case 1: hash += *data;
  hash ^= hash << 10;
  hash += hash >> 1;
 }
    // len was calculated in 4-byte tuples,
    // multiply it by 4 to get the number in bytes
 *len = (*len << 2) + rem;
 
 /* Force "avalanching" of final 127 bits */
 hash ^= hash << 3;
 hash += hash >> 5;
 hash ^= hash << 4;
 hash += hash >> 17;
 hash ^= hash << 25;
 hash += hash >> 6;

 return hash;
}

struct itree *einit_stable_strings = NULL;

#undef DEBUG

const char *str_stabilise_l (const char *s, uint32_t *h, int *l) {
 if (!s) return NULL;
 if (!s[0]) return ""; /* use a real static string for this one since it can fuck things up hard */

// uintptr_t pi = (uintptr_t)s;
 int len;
 uint32_t hash;
 char *nv = NULL;

#ifdef DEBUG
 static int bad_lookups = 0;
 static int good_lookups = 0;
 static int prefail_lookups = 0;
 static int strings = 0;
#endif

 struct itree *i = 0;

// if (pi % 8) {
  /* this means we'd be unaligned */
#ifdef DEBUG
  prefail_lookups++;
#endif

  nv = estrdup (s);

  hash = StrSuperFastHash(nv, &len);
/* } else {
  hash = StrSuperFastHash(s, &len);
 }*/

#ifdef DEBUG
 fprintf (stderr, "hash result: %i, len %i\n", hash, len);
#endif

 i = einit_stable_strings ? itreefind (einit_stable_strings, hash, tree_find_first) : NULL;
#if 0
 while (i) {
  if (i->value == s) {
#ifdef DEBUG
   good_lookups++;

   fprintf (stderr, "stabilisation result: %i bad, %i good, %i prefail, strings %i\n", bad_lookups, good_lookups, prefail_lookups, strings);
#endif

   break;
  }
  if (strmatch (s, i->value)) {
#ifdef DEBUG
   good_lookups++;

   fprintf (stderr, "stabilisation result: %i bad, %i good, %i prefail, strings %i\n", bad_lookups, good_lookups, prefail_lookups, strings);
#endif

   break;
  } else {
#ifdef DEBUG
   bad_lookups++;
#endif
   printf ("BAD LOOKUP: %s<>%s, hash: %i\n", s, i->value, hash);
  }

  i = itreefind (i, hash, tree_find_next);
 }
#endif

 if (i) {
  if (nv) efree (nv);

#ifdef DEBUG
  fprintf (stderr, "stabilisation result: %i bad, %i good, %i prefail, strings %i\n", bad_lookups, good_lookups, prefail_lookups, strings);
#endif

  goto ret;
 }

 /* getting 'ere means we didn't have the right string in the set */

 if (!nv)
  nv = estrdup (s);

 /* we don't really care if we accidentally duplicate the string */
 i = itreeadd (einit_stable_strings, hash, nv, tree_value_noalloc);
 einit_stable_strings = i;

#ifdef DEBUG
 strings++;
 fprintf (stderr, "stabilisation result: %i bad, %i good, %i prefail, strings %i\n", bad_lookups, good_lookups, prefail_lookups, strings);
#endif

 ret:

 if (h) *h = hash;
 if (l) *l = len;

 return i->value;
}

const char *str_stabilise (const char *s) {
 return str_stabilise_l (s, NULL, NULL);
}

char **set_str_dup_stable (char **s) {
 void **d = NULL;
 int i = 0;

 if (!s) return NULL;

 for (; s[i]; i++) {
  d = set_noa_add (d, (void *)str_stabilise (s[i]));
 }

 return (char **)d;
}

char **set_str_add_stable (char **s, char *e) {
 s = (char **)set_noa_add ((void **)s, (void *)str_stabilise (e));

 return s;
}
