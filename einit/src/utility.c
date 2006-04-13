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
#include <einit/config.h>

/* some common functions to work with null-terminated arrays */

void **plcombine (void **list1, void **list2) {
 void **newlist;
 int x = 0, y = 0, s = 1, p = 0;
 char *strbuffer = NULL;
 if (!list1) return list2;
 if (!list1[0]) {
  free (list1);
  return list2;
 }
 if (!list2) return list1;
 if (!list2[0]) {
  free (list2);
  return list1;
 }

 newlist = calloc (plcount(list1) + plcount(list2) +1, sizeof (void *));
 if (!newlist) {
  bitch (BTCH_ERRNO);
  return NULL;
 }

 while (list1[x])
  { newlist [x] = list1[x]; x++; }
 y = x; x = 0;
 while (list2[x])
  { newlist [y] = list2[x]; x++; y++; }

 return newlist;
}

char **pldup (char **list) {
 char **newlist;
 int y = 0;
 if (!list) return NULL;
 if (!list[0]) return NULL;

 newlist = calloc (plcount(slist) +1, sizeof (char *));
 if (!newlist) {
  bitch (BTCH_ERRNO);
  return NULL;
 }
 while (list[y]) {
  newlist[y] = list[y];
  y++;
 }

 return newlist;
}

int plcount (void **slist) {
 int i = 0;
 while (slist[i])
  i++;

 return i;
}

/* some functions to work with string-lists */

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

 newlist = calloc (plcount(slist) +1, sizeof (char *));
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

/* those i-could've-sworn-there-were-library-functions-for-that functions */

char *cfg_getpath (char *id) {
 int mplen;
 struct cfgnode *svpath = cfg_findnode (id, 0);
 if (!svpath || !svpath->svalue) return NULL;
 mplen = strlen (svpath->svalue) +1;
 if (svpath->svalue[mplen-2] != '/') {
  char *tmpsvpath = (char *)realloc (svpath->svalue, mplen+1);
  if (!tmpsvpath) {
   bitch (BTCH_ERRNO);
   return NULL;
  }

  tmpsvpath[mplen-1] = '/';
  tmpsvpath[mplen] = 0;
  svpath->svalue = tmpsvpath;
 }
 return svpath->svalue;
}
