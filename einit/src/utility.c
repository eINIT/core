/*
 *  utility.c
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
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
  uint32_t i = 0;

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

  if (errno && (errno != EAGAIN))
   bitch2(BITCH_STDIO, "readfile()", errno, "reading file failed.");

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
  bitch2(BITCH_EMALLOC, "emalloc", 0, "call to malloc() failed.");
  sleep (1);
 }

 return p;
}

void *ecalloc (size_t c, size_t s) {
 void *p = NULL;

 while (!(p = calloc (c, s))) {
  bitch2(BITCH_EMALLOC, "ecalloc", 0, "call to calloc() failed.");
  sleep (1);
 }

 return p;
}

void *erealloc (void *c, size_t s) {
 void *p = NULL;

 while (!(p = realloc (c, s))) {
  bitch2(BITCH_EMALLOC, "erealloc", 0, "call to realloc() failed.");
  sleep (1);
 }

 return p;
}

char *estrdup (char *s) {
 char *p = NULL;

 while (!(p = strdup (s))) {
  bitch2(BITCH_EMALLOC, "estrdup", 0, "call to strdup() failed.");
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

 if (pthread_mutex_init (&nev->mutex, NULL)) {
  bitch2(BITCH_EPTHREADS, "evdup()", 0, "pthread_mutex_init() failed.");
 }

 return nev;
}

struct einit_event *evinit (uint32_t type) {
 struct einit_event *nev = ecalloc (1, sizeof (struct einit_event));

 nev->type = type;
 if (pthread_mutex_init (&nev->mutex, NULL)) {
  bitch2(BITCH_EPTHREADS, "evinit()", 0, "pthread_mutex_init() failed.");
 }

 return nev;
}

void evdestroy (struct einit_event *ev) {
 if (pthread_mutex_destroy (&ev->mutex)) {
  bitch2(BITCH_EPTHREADS, "evdestroy()", 0, "pthread_mutex_destroy() failed.");
 }
 free (ev);
}


/* user/group functions */
int lookupuidgid (uid_t *uid, gid_t *gid, char *user, char *group) {
 if (!_getgr_r_size_max) _getgr_r_size_max = sysconf (_SC_GETGR_R_SIZE_MAX);
 if (!_getpw_r_size_max) _getpw_r_size_max = sysconf (_SC_GETPW_R_SIZE_MAX);

 if (user) {
  struct passwd pwd, *pwdptr;
  char *buffer = emalloc (_getpw_r_size_max);
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
 char *ret = NULL, *vst = NULL, tsin = 0;
 uint32_t len = 0, rpos = 0, spos = 0, rspos = 0;

 if (!string) return NULL;
 if (!env) return estrdup (string);

 ret = emalloc (len = (strlen (string) + 1));
 *ret = 0;

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

 return ret;
}

char *escape_xml (char *input) {
 char *retval = NULL;
 if (input) {
  ssize_t olen = strlen (input)+1,
   blen = olen + 1024,
   cpos = 0, tpos = 0;

  retval = emalloc (blen);

  for (cpos = 0; input[cpos]; cpos++) {
   if (tpos > (blen -7)) {
    blen += 1024;
    retval = erealloc (retval, blen);
   }

   switch (input[cpos]) {
    case '&':
     retval[tpos] = '&';
     retval[tpos+1] = 'a';
     retval[tpos+2] = 'm';
     retval[tpos+3] = 'p';
     retval[tpos+4] = ';';
     tpos += 5;
     break;
    case '<':
     retval[tpos] = '&';
     retval[tpos+1] = 'l';
     retval[tpos+2] = 't';
     retval[tpos+3] = ';';
     tpos += 4;
     break;
    case '>':
     retval[tpos] = '&';
     retval[tpos+1] = 'g';
     retval[tpos+2] = 't';
     retval[tpos+3] = ';';
     tpos += 4;
     break;
    case '"':
     retval[tpos] = '&';
     retval[tpos+1] = 'q';
     retval[tpos+2] = 'u';
     retval[tpos+3] = 'o';
     retval[tpos+4] = 't';
     retval[tpos+5] = ';';
     tpos += 6;
     break;
    default:
     retval[tpos] = input[cpos];
     tpos++;
     break;
   }
  }

  retval[tpos] = 0;
 }

 return retval;
}

