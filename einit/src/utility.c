/***************************************************************************
 *            utlity.c
 *
 *  Sat Mar 25 18:15:21 2006
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
#include <string.h>
#include <einit/bitch.h>

char **str2slist (const char sep, char *input) {
 int l = strlen (input), i = 0, sc = 1, cr = 1;
 char **ret;
 l--;

 for (; i < l; i++) {
  if (input[i] == sep) {
   sc++;
   input[i] = 0;
  }
 }
 ret = calloc (sc, sizeof(char *));
 if (!ret) {
  bitch (BTCH_ERRNO);
  for (i = 0; i < l; i++) {
   if (input[i] == 0) {
    input[i] = sep;
   }
  }
  return NULL;
 }
 ret[0] = input;
 for (i = 0; i < l; i++) {
  if (input[i] == 0) {
   ret[cr] = input+i+1;
   cr++;
  }
 }
 return ret;
}

int strinslist (char **haystack, const char *needle) {
 int c = 0;
 for (; haystack[c] != NULL; c++) {
  if (!strcmp (haystack[c], needle)) return 1;
 }
 return 0;
}

char **slistcombine (char **slist1, char **slist2) {
 char **newlist;
 int x = 0, y = 0, s = 1, p = 0;
 char *strbuffer = NULL;
 if (!slist1) return slist2;
 if (!slist1[0]) {
  free (slist1);
  return slist2;
 }
 if (!slist2) return slist1;
 if (!slist2[0]) {
  free (slist2);
  return slist1;
 }

 newlist = calloc (slistcount(slist1) + slistcount(slist2) +1, sizeof (char *));
 if (!newlist) {
  bitch (BTCH_ERRNO);
  return NULL;
 }

 while (slist1[x])
  { newlist [x] = slist1[x]; x++; }
 y = x; x = 0;
 while (slist2[x])
  { newlist [y] = slist2[x]; x++; y++; }

 return newlist;
}

int slistcount (char **slist) {
 int i = 0;
 while (slist[i])
  i++;

 return i;
}

int slistrebuild (char **slist) {
 int y = 0, p = 0, s = 1;
 char *strbuffer;

 while (slist[y])
  { y++; s += strlen (slist[y]); }

 strbuffer = malloc (s*sizeof(char));
 if (!strbuffer) return bitch (BTCH_ERRNO);
 strbuffer[s-1] = 0;
}

char **slistdup (char **slist) {
 char **newlist;
 int y = 0;
 if (!slist) return NULL;
 if (!slist[0]) return NULL;

 newlist = calloc (slistcount(slist) +1, sizeof (char *));
 if (!newlist) {
  bitch (BTCH_ERRNO);
  return NULL;
 }
 while (slist[y]) {
  newlist[y] = strdup (slist[y]);
  if (!newlist [y]) {
   bitch (BTCH_ERRNO);
   for (y = 0; newlist[y] != NULL; y++)
    free (newlist[y]);
   free (newlist);
   return NULL;
  }
  y++;
 }

 return newlist;
}
