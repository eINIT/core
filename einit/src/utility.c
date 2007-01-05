/*
 *  utility.c
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
#include <string.h>
#include <stdlib.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

long _getgr_r_size_max = 0, _getpw_r_size_max = 0;

/* some common functions to work with null-terminated arrays */

void **setcombine (void **set1, void **set2, int32_t esize) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
 uint32_t count = 0, size = 0;
 char *strbuffer = NULL;
 if (!set1) return setdup(set2, esize);
 if (!set1[0]) {
  free (set1);
  return setdup(set2, esize);
 }
 if (!set2) return setdup(set1, esize);
 if (!set2[0]) {
  free (set2);
  return setdup(set1, esize);
 }

 if (esize == -1) {
  for (; set1[count]; count++);
  size = count+1;

  for (x = 0; set2[x]; x++);
  size += x;
  count += x;
  size *= sizeof (void*);

  newset = ecalloc (1, size);

  x = 0;
  while (set1[x])
   { newset [x] = set1[x]; x++; }
  y = x; x = 0;
  while (set2[x])
   { newset [y] = set2[x]; x++; y++; }
 } else if (esize == 0) {
  char *cpnt;

  for (; set1[count]; count++)
   size += sizeof(void*) + 1 + strlen(set1[count]);
  size += sizeof(void*);
  for (x = 0; set2[x]; x++)
   size += sizeof(void*) + 1 + strlen(set2[x]);
  count += x;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  x = 0;
  while (set1[x]) {
   esize = 1+strlen(set1[x]);
   memcpy (cpnt, set1[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
   x++;
  }
  y = x; x = 0;
  while (set2[x]) {
   esize = 1+strlen(set2[x]);
   memcpy (cpnt, set2[x], esize);
   newset [y] = cpnt;
   cpnt += esize;
   x++;
   y++;
  }
 } else {
  char *cpnt;

  for (; set1[count]; count++)
   size += sizeof(void*) + 1 + esize;;
  size += sizeof(void*);
  for (x = 0; set2[x]; x++)
   size += sizeof(void*) + 1 + esize;
  count += x;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  x = 0;
  while (set1[x]) {
   memcpy (cpnt, set1[x], esize);
   newset [x] = cpnt;
   cpnt += esize;
   x++;
  }
  y = x; x = 0;
  while (set2[x]) {
   memcpy (cpnt, set2[x], esize);
   newset [y] = cpnt;
   cpnt += esize;
   x++;
   y++;
  }
 }

 return newset;
}

void **setadd (void **set, void *item, int32_t esize) {
 void **newset;
 int x = 0, y = 0, s = 1, p = 0;
 char *strbuffer = NULL;
 uint32_t count = 0, size = 0;
 if (!item) return NULL;
// if (!set) set = ecalloc (1, sizeof (void *));

 if (esize == -1) {
  if (set) for (; set[count]; count++);
  else count = 1;
  size = (count+2)*sizeof(void*);

  newset = ecalloc (1, size);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     free (newset);
     return set;
    }
    newset [x] = set[x];
    x++;
   }
   free (set);
  }

  newset[x] = item;
 } else if (esize == 0) {
  char *cpnt;

//  puts ("adding object to string-set");
  if (set) for (; set[count]; count++) {
   size += sizeof(void*) + 1 + strlen(set[count]);
  }
  size += sizeof(void*)*2 + 1 +strlen(item);

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     free (newset);
     return set;
    }
    esize = 1+strlen(set[x]);
    memcpy (cpnt, set[x], esize);
    newset [x] = cpnt;
    cpnt += esize;
    x++;
   }
   free (set);
  }

  esize = 1+strlen(item);
  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
//  puts(item);
//  cpnt += 1+strlen(item);
 } else {
  char *cpnt;

  if (set) for (; set[count]; count++) {
   size += sizeof(void*) + esize;
  }
  size += sizeof(void*)*2 + esize;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+2)*sizeof(void*);

  if (set) {
   while (set[x]) {
    if (set[x] == item) {
     free (newset);
     return set;
    }
    memcpy (cpnt, set[x], esize);
    newset [x] = cpnt;
    cpnt += esize;
    x++;
   }
   free (set);
  }

  memcpy (cpnt, item, esize);
  newset [x] = cpnt;
//  cpnt += esize;
 }

 return newset;
}

void **setdup (void **set, int32_t esize) {
 void **newset;
 uint32_t y = 0, count = 0, size = 0;
 if (!set) return NULL;
 if (!set[0]) return NULL;

 if (esize == -1) {
  newset = ecalloc (setcount(set) +1, sizeof (char *));
  while (set[y]) {
   newset[y] = set[y];
   y++;
  }
 } else if (esize == 0) {
  char *cpnt;

  for (; set[count]; count++)
   size += sizeof(void*) + 1 + strlen(set[count]);
  size += sizeof(void*)*2;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  while (set[y]) {
   esize = 1+strlen(set[y]);
   memcpy (cpnt, set[y], esize);
   newset [y] = cpnt;
   cpnt += esize;
   y++;
  }
 } else {
  char *cpnt;

  for (; set[count]; count++)
   size += sizeof(void*) + esize;
  size += sizeof(void*)*2;

  newset = ecalloc (1, size);
  cpnt = ((char *)newset) + (count+1)*sizeof(void*);

  while (set[y]) {
   memcpy (cpnt, set[y], esize);
   newset [y] = cpnt;
   cpnt += esize;
   y++;
  }
 }

 return newset;
}

void **setdel (void **set, void *item) {
 void **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
 if (!item || !set) return NULL;

 while (set[y]) {
  if (set[y] != item) {
   newset [x] = set[y];
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}

int setcount (void **set) {
 int i = 0;
 if (!set) return 0;
 if (!set[0]) return 0;
 while (set[i])
  i++;

 return i;
}

void setsort (void **set, char task, signed int(*sortfunction)(void *, void*)) {
 uint32_t c = 0, c2 = 0, x = 0, dc = 1;
 void *tmp;
 if (!set) return;

 if (task == SORT_SET_STRING_LEXICAL)
  sortfunction = (signed int(*)(void *, void*))strcmp;
 else if (!sortfunction) return;

/* this doesn't work, yet */
 for (; set[c]; c++) {
  for (c2 = c+1; set[c2]; c2++) {
   if ((x = sortfunction(set[c], set[c2])) > 0) {
    dc = 1;
    tmp = set[c2];
    set[c2] = set[c];
    set[c] = tmp;
   }
  }
 }

 return;
}

int inset (void **haystack, const void *needle, int32_t esize) {
 int c = 0;

 if (!haystack) return 0;
 if (!haystack[0]) return 0;
 if (!needle) return 0;

 if (esize == SET_TYPE_STRING) {
  for (; haystack[c] != NULL; c++)
   if (!strcmp (haystack[c], needle)) return 1;
 } else if (esize == -1) {
  for (; haystack[c] != NULL; c++)
   if (haystack[c] == needle) return 1;
 }
 return 0;
}

/* some functions to work with string-sets */

char **str2set (const char sep, char *input) {
 int l, i = 0, sc = 1, cr = 1;
 char **ret;
 if (!input) return NULL;
 l = strlen (input)-1;

 for (; i < l; i++) {
  if (input[i] == sep) {
   sc++;
//   input[i] = 0;
  }
 }
 ret = ecalloc (1, ((sc+1)*sizeof(char *)) + 2 + l);
 memcpy ((((char *)ret) + ((sc+1)*sizeof(char *))), input, 2 + l);
 input = (char *)(((char *)ret) + ((sc+1)*sizeof(char *)));
 ret[0] = input;
 for (i = 0; i < l; i++) {
  if (input[i] == sep) {
   ret[cr] = input+i+1;
   input[i] = 0;
   cr++;
  }
 }
 return ret;
}

char *set2str (const char sep, char **input) {
 char *ret = NULL;
 size_t slen = 0;
 uint32_t i = 0;
 char nsep[2] = {sep, 0};

 if (!input) return NULL;

 for (; input[i]; i++) {
  slen += strlen (input[i])+1;
 }

 ret = emalloc (slen);
 *ret = 0;

 for (i = 0; input[i]; i++) {
  if (i != 0)
   strcat (ret, nsep);

  strcat (ret, input[i]);
 }

 return ret;
}

char **strsetdel (char **set, char *item) {
 char **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
 if (!item || !set) return NULL;
 if (!set[0]) {
  free (set);
  return NULL;
 }

 while (set[y]) {
  if (strcmp(set[y], item)) {
   newset [x] = set[y];
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
//  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}

char **strsetdeldupes (char **set) {
 char **newset = set;
 int x = 0, y = 0, s = 1, p = 0;
 if (!set) return NULL;

 while (set[y]) {
  char *tmp = set[y];
  set[y] = NULL;
  if (!inset ((void **)set, (void *)tmp, SET_TYPE_STRING)) {
   newset [x] = tmp;
   x++;
  }
  y++;
/*  else {
   set = set+1;
  }*/
 }

 if (!x) {
  free (set);
  return NULL;
 }

 newset[x] = NULL;

 return newset;
}

char **straddtoenviron (char **environment, char *key, char *value) {
 char **ret;
 char *newitem;
 int len = 2;
 if (!key) return environment;

 len += strlen (key);
 if (value) len += strlen (value);
 newitem = emalloc (sizeof(char)*len);
 newitem[0] = 0;
 {
  uint32_t len = strlen (key), i = 0;

  newitem = strcat (newitem, key);
  for (; newitem[i]; i++) {
   if (!isalnum (newitem[i])) newitem[i] = '_';
  }
 }
 if (value) newitem = strcat (newitem, "=");
 if (value) newitem = strcat (newitem, value);

 ret = (char**) setadd ((void**)environment, (void*)newitem, SET_TYPE_STRING);
 free (newitem);

 return ret;
}

char *readfile (char *filename) {
 int fd = 0, rn = 0;
 void *buf = NULL;
 char *data = NULL;
 uint32_t blen = 0;

 if (!filename) return NULL;
 fd = open (filename, O_RDONLY);

 if (fd != -1) {
  buf = emalloc (BUFFERSIZE*sizeof(char));
  blen = 0;
  do {
   buf = erealloc (buf, blen + BUFFERSIZE);
   if (buf == NULL) return NULL;
   rn = read (fd, (char *)(buf + blen), BUFFERSIZE);
   blen = blen + rn;
  } while (rn > 0);
  close (fd);
  data = erealloc (buf, blen);
  *(data+blen-1) = 0;
 }

 return data;
}

/* safe malloc/calloc/realloc/strdup functions */

void *emalloc (size_t s) {
 void *p = NULL;

 while (!(p = malloc (s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

void *ecalloc (size_t c, size_t s) {
 void *p = NULL;

 while (!(p = calloc (c, s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

void *erealloc (void *c, size_t s) {
 void *p = NULL;

 while (!(p = realloc (c, s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

char *estrdup (char *s) {
 char *p = NULL;

 while (!(p = strdup (s))) {
  bitch (BTCH_ERRNO);
  sleep (1);
 }

 return p;
}

/* nifty string functions */
void strtrim (char *s) {
 if (!s) return;
 uint32_t l = strlen (s), i = 0, offset = 0;

 for (; i < l; i++) {
  if (isspace (s[i])) offset++;
  else {
   if (offset)
    memmove (s, s+offset, l-offset+1);
   break;
  }
 }

 if (i == l) {
  s[0] = 0;
  return;
 }

 l -= offset+1;

 for (i = l; i >= 0; i--) {
  if (isspace (s[i])) s[i] = 0;
  else break;
 }
}

/* event-helpers */
void notice (unsigned char severity, char *message) {
 struct einit_event *ev = evinit (EVE_FEEDBACK_NOTICE);

 ev->flag = severity;
 ev->string = message;

 event_emit (ev, EINIT_EVENT_FLAG_BROADCAST | EINIT_EVENT_FLAG_SPAWN_THREAD | EINIT_EVENT_FLAG_DUPLICATE);

 evdestroy (ev);
}

struct einit_event *evdup (struct einit_event *ev) {
 struct einit_event *nev = emalloc (sizeof (struct einit_event));

 memcpy (nev, ev, sizeof (struct einit_event));
 memset (&nev->mutex, 0, sizeof (pthread_mutex_t));

 if (nev->string) {
  uint32_t l;
  char *np;
  nev = erealloc (nev, sizeof (struct einit_event) + (l = strlen (nev->string) +1));

  memcpy (np = ((char*)nev)+sizeof (struct einit_event), nev->string, l);

  nev->string = np;
 }

 if (pthread_mutex_init (&nev->mutex, NULL))
  perror (" >> evdup(): pthread_mutex_init()");

 return nev;
}

struct einit_event *evinit (uint32_t type) {
 struct einit_event *nev = ecalloc (1, sizeof (struct einit_event));

 nev->type = type;
 if (pthread_mutex_init (&nev->mutex, NULL))
  perror (" >> evinit(): pthread_mutex_init()");

 return nev;
}

void evdestroy (struct einit_event *ev) {
 pthread_mutex_destroy (&ev->mutex);
 free (ev);
}


/* user/group functions */
int lookupuidgid (uid_t *uid, gid_t *gid, char *user, char *group) {
 if (!_getgr_r_size_max) _getgr_r_size_max = sysconf (_SC_GETGR_R_SIZE_MAX);
 if (!_getpw_r_size_max) _getpw_r_size_max = sysconf (_SC_GETPW_R_SIZE_MAX);

 if (user) {
  struct passwd pwd, *pwdptr;
  char *buffer = malloc (_getpw_r_size_max);
  errno = 0;
  while (getpwnam_r(user, &pwd, buffer, _getpw_r_size_max, &pwdptr)) {
   switch (errno) {
    case EIO:
    case EMFILE:
    case ENFILE:
    case ERANGE:
     perror ("getpwnam_r");
     free (buffer);
     return -1;
    case EINTR:
     continue;
    default:
     free (buffer);
     goto abortusersearch;
   }
  }

  *uid = pwd.pw_uid;
  if (!group) *gid = pwd.pw_gid;
  free (buffer);
 }

 abortusersearch:

 if (group) {
  struct group grp, *grpptr;
  char *buffer = emalloc (_getgr_r_size_max);
  errno = 0;
  while (getgrnam_r(group, &grp, buffer, _getgr_r_size_max, &grpptr)) {
   switch (errno) {
    case EIO:
    case EMFILE:
    case ENFILE:
    case ERANGE:
     perror ("getgrnam_r");
     free (buffer);
     return -2;
    default:
     free (buffer);
     goto abortgroupsearch;
   }
  }

  *gid = grp.gr_gid;
  free (buffer);
 }

 abortgroupsearch:

 return 0;
}

signed int parse_integer (char *s) {
 signed int ret = 0;

 if (!s) return ret;

 if (s[strlen (s)-1] == 'b') {
   ret = strtol (s, (char **)NULL, 2); // parse as binary number if argument ends with b, eg 100b
 } else if (s[0] == '0') {
  if (s[1] == 'x')
   ret = strtol (s+2, (char **)NULL, 16); // parse as hex number if argument starts with 0x, eg 0xff3
  else
   ret = strtol (s, (char **)NULL, 8); // parse as octal number if argument starts with 0, eg 0643
 } else
  ret = atoi (s); // if nothing else worked, parse as decimal argument

 return ret;
}

char parse_boolean (char *s) {
 return s && (!strcmp (s, "true") || !strcmp (s, "enabled") || !strcmp (s, "yes"));
}

char *apply_variables (char *string, char **env) {
 char *ret, *vst, tsin = 0;
 uint32_t len = 0, rpos = 0, spos = 0, rspos = 0;

 if (!string) return NULL;
 if (!env) return estrdup (string);

 ret = emalloc (len = (strlen (string) + 1));
 *ret = 0;

// puts (string);
 for (spos = 0, rpos = 0; string[spos]; spos++) {
  if ((string[spos] == '$') && (string[spos+1] == '{')) {
   for (rspos -=2; string[rspos] && (rspos < spos); rspos++) {
    ret[rpos] = string[rspos];
    rpos++;
   }
   spos++;
   rspos = spos+1;
   vst = string+rspos;
   tsin = 2;
  } else if (tsin == 2) {
   if (string[spos] == '}') {
    uint32_t i = 0, xi = 0;
    string[spos] = 0;
    for (; env[i]; i+=2) {
     if (!strcmp (env[i], vst)) {
      xi = i+1; break;
     }
    }
    if (xi) {
     len = len - strlen (vst) - 3 + strlen (env[xi]);
     ret = erealloc (ret, len);
     for (i = 0; env[xi][i]; i++) {
      ret[rpos] = env[xi][i];
      rpos++;
     }
    } else {
     for (rspos -=2; string[rspos] && (rspos < spos); rspos++) {
      ret[rpos] = string[rspos];
      rpos++;
     }
     ret[rpos] = '}';
     rpos++;
    }
    string[spos] = '}';
    tsin = 0;
   }
  } else {
   tsin = 0;
   rspos = spos+3;
   ret[rpos] = string[spos];
   rpos++;
  }
 }
 ret[rpos] = 0;
// puts (ret);

 return ret;
}
