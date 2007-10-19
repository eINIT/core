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
#include <einit/configuration.h>
#include <einit/utility.h>
#include <einit/set.h>
#include <einit/event.h>
#include <ctype.h>
#include <stdio.h>

#include <sys/mman.h>

#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h> 
#include <sys/resource.h>

long _getgr_r_size_max = 0, _getpw_r_size_max = 0;

#if ! defined (EINIT_UTIL)
#ifdef POSIXREGEX
#include <regex.h>
#endif

#include <dirent.h>

char **readdirfilter (struct cfgnode const *node, const char *default_dir, const char *default_allow, const char *default_disallow, char recurse) {
 DIR *dir;
 struct dirent *entry;
 char **retval = NULL;
 char *tmp;
 ssize_t mplen = 0;
 char *px = NULL;

#ifdef POSIXREGEX
 regex_t allowpattern, disallowpattern;
 unsigned char haveallowpattern = 0, havedisallowpattern = 0;
 char *path = (char *)default_dir, *allow = (char *)default_allow, *disallow = (char *)default_disallow;
#endif

 if (node && node->arbattrs) {
  uint32_t i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch ("path", node->arbattrs[i])) path = node->arbattrs[i+1];
#ifdef POSIXREGEX
   else if (strmatch ("pattern-allow", node->arbattrs[i])) allow = node->arbattrs[i+1];
   else if (strmatch ("pattern-disallow", node->arbattrs[i])) disallow = node->arbattrs[i+1];
#endif
  }
 }

 if (!path) return NULL;

#if 0
 if (!recurse)
#endif
 if (coremode == einit_mode_sandbox) {
// override path in sandbox-mode to be relative
  if (path[0] == '/') path++;
 }

 mplen = strlen(path) + 4;
 px = emalloc (mplen);
 strcpy (px, path);

 if (px[mplen-5] != '/') {
  px[mplen-4] = '/';
  px[mplen-3] = 0;
 }


#ifdef POSIXREGEX
 if (allow) {
  haveallowpattern = !eregcomp (&allowpattern, allow);
 }

 if (disallow) {
  havedisallowpattern = !eregcomp (&disallowpattern, disallow);
 }
#endif

 mplen += 4;
 dir = eopendir (path);
 if (dir != NULL) {
  while ((entry = ereaddir (dir))) {

#ifdef POSIXREGEX
   if (haveallowpattern && regexec (&allowpattern, entry->d_name, 0, NULL, 0)) continue;
   if (havedisallowpattern && !regexec (&disallowpattern, entry->d_name, 0, NULL, 0)) continue;
#else
   if (entry->d_name[0] == '.') continue;
#endif

   tmp = (char *)emalloc (mplen + strlen (entry->d_name));
   struct stat sbuf;
   *tmp = 0;
   strcat (tmp, px);
   strcat (tmp, entry->d_name);

   if (lstat (tmp, &sbuf)) goto cleanup_continue;

   if (recurse) {
    if (S_ISLNK(sbuf.st_mode)) goto cleanup_continue;

    if (S_ISDIR (sbuf.st_mode)) {
     if ((entry->d_name[0] == '.') && (!entry->d_name[1] ||
          ((entry->d_name[1] == '.') && !entry->d_name[2]))) goto cleanup_continue;

     tmp = strcat (tmp, "/");

     char **n = readdirfilter (NULL, tmp, allow, disallow, 1);

     if (n) {
      retval = (char **)setcombine_nc ((void **)retval, (const void **)n, SET_TYPE_STRING);
      free (n);
     }

     /* add the dir itself afterwards */
     retval = (char **)setadd((void **)retval, (void *)tmp, SET_TYPE_STRING);

     goto cleanup_continue;
    }
   } else if (!S_ISREG (sbuf.st_mode)) {
    goto cleanup_continue;
   }

   retval = (char **)setadd((void **)retval, (void *)tmp, SET_TYPE_STRING);

   cleanup_continue:
   free (tmp);
  }
  eclosedir (dir);
 }

#ifdef POSIXREGEX
 if (haveallowpattern) { haveallowpattern = 0; regfree (&allowpattern); }
 if (havedisallowpattern) { havedisallowpattern = 0; regfree (&disallowpattern); }
#endif

 free (px);

 return retval;
}

char **straddtoenviron (char **environment, const char *key, const char *value) {
 char **ret = NULL;
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

#endif

char *readfd_l (int fd, ssize_t *rl) {
 int rn = 0;
 void *buf = NULL;
 char *data = NULL;
 uint32_t blen = 0;

 buf = emalloc (BUFFERSIZE * 10);
 do {
  buf = erealloc (buf, blen + BUFFERSIZE * 10);
  if (buf == NULL) return NULL;
  rn = read (fd, (char *)(buf + blen), BUFFERSIZE * 10);
  blen = blen + rn;
 } while (rn > 0);

 if (errno && (errno != EAGAIN))
  bitch(bitch_stdio, errno, "reading file failed.");

 data = erealloc (buf, blen+1);
 data[blen] = 0;
 if (blen > 0) {
  *(data+blen-1) = 0;
 } else {
  free (data);
  data = NULL;
 }

 if (rl) *rl = blen;

 return data;
}

char *readfile_l (const char *filename, ssize_t *rl) {
 int fd = 0;
 void *buf = NULL;
 char *data = NULL;
 struct stat st;

 if (!filename) return NULL;

/* make an exception to the no-0-length-files rule for stuff in /proc */
 if (stat (filename, &st) || ((st.st_size <= 0) && (strstr (filename, "/proc/") != filename))) return NULL;

 fd = eopen (filename, O_RDONLY);

 if (fd != -1) {
  if ((st.st_size > 0) && ((buf = mmap (NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) != MAP_FAILED)) {
   eclose (fd);
   data = emalloc (st.st_size +1);
   memcpy (data, buf, st.st_size);
   munmap (buf, st.st_size);

   *(data+st.st_size) = 0;

   if (rl) *rl = st.st_size;
  } else {
   data = readfd_l (fd, rl);
   eclose (fd);
  }
 }

 return data;
}

/* safe malloc/calloc/realloc/strdup functions */

void *emalloc (size_t s) {
 void *p = NULL;

 while (!(p = malloc (s))) {
  bitch(bitch_emalloc, 0, "call to malloc() failed.");
  sleep (1);
 }

 return p;
}

void *ecalloc (size_t c, size_t s) {
 void *p = NULL;

 while (!(p = calloc (c, s))) {
  bitch(bitch_emalloc, 0, "call to calloc() failed.");
  sleep (1);
 }

 return p;
}

void *erealloc (void *c, size_t s) {
 void *p = NULL;

 while (!(p = realloc (c, s))) {
  bitch(bitch_emalloc, 0, "call to realloc() failed.");
  sleep (1);
 }

 return p;
}

char *estrdup (const char *s) {
 size_t len = strlen(s)+1;
 char *p = emalloc (len);

 memcpy (p, s, len);

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

#if ! defined (EINIT_UTIL)

/* event-helpers */
void notice_macro (unsigned char severity, const char *message) {
 struct einit_event *ev = evinit (einit_feedback_notice);

 ev->flag = severity;
 ev->string = (char *)message;

 event_emit (ev, einit_event_flag_broadcast | einit_event_flag_spawn_thread | einit_event_flag_duplicate);

 evdestroy (ev);
}

struct einit_event *evdup (const struct einit_event *ev) {
 if (!ev) return NULL;

 uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

 struct einit_event *nev = emalloc (sizeof (struct einit_event));

 memcpy (nev, ev, sizeof (struct einit_event));
 memset (&nev->mutex, 0, sizeof (pthread_mutex_t));

 if (subsystem == einit_event_subsystem_ipc) {
  if (nev->command) {
   int32_t l;
   char *np;
   nev = erealloc (nev, sizeof (struct einit_event) + (l = strlen (nev->command) +1));

   memcpy (np = ((char*)nev)+sizeof (struct einit_event), nev->command, l);

   nev->command = np;
  }

  if (ev->argv) nev->argv = (char **)setdup ((const void **)ev->argv, SET_TYPE_STRING);
 } else {
  if (nev->string) {
   int32_t l;
   char *np;
   nev = erealloc (nev, sizeof (struct einit_event) + (l = strlen (nev->string) +1));

   memcpy (np = ((char*)nev)+sizeof (struct einit_event), nev->string, l);

   nev->string = np;
  }

  if (ev->stringset) nev->stringset = (char **)setdup ((const void **)ev->stringset, SET_TYPE_STRING);
 }

 emutex_init (&nev->mutex, NULL);

 return nev;
}

struct einit_event *evinit (uint32_t type) {
 struct einit_event *nev = ecalloc (1, sizeof (struct einit_event));

 nev->type = type;
 emutex_init (&nev->mutex, NULL);

 return nev;
}

void evpurge (struct einit_event *ev) {
 uint32_t subsystem = ev->type & EVENT_SUBSYSTEM_MASK;

 if (subsystem == einit_event_subsystem_ipc) {
  if (ev->argv) free (ev->argv);
  if (ev->command) free (ev->command);
 } else {
  if (ev->string) free (ev->string);
  if (ev->stringset) free (ev->stringset);
 }

 evdestroy (ev);
}

void evdestroy (struct einit_event *ev) {
 emutex_destroy (&ev->mutex);
 free (ev);
}


/* user/group functions */
int lookupuidgid (uid_t *uid, gid_t *gid, const char *user, const char *group) {
 if (!_getgr_r_size_max) _getgr_r_size_max = sysconf (_SC_GETGR_R_SIZE_MAX);
 if (!_getpw_r_size_max) _getpw_r_size_max = sysconf (_SC_GETPW_R_SIZE_MAX);

 if (user && uid) {
  struct passwd pwd, *pwdptr;
  char *buffer = emalloc (_getpw_r_size_max);
  errno = 0;

  memset (buffer, 0, _getpw_r_size_max);

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

  if (pwd.pw_name && strmatch (pwd.pw_name, user)) {
   *uid = pwd.pw_uid;

   if (!group && gid) *gid = pwd.pw_gid;
  }
  free (buffer);
 }

 abortusersearch:

 if (group && gid) {
  struct group grp, *grpptr;
  char *buffer = emalloc (_getgr_r_size_max);
  errno = 0;

  memset (buffer, 0, _getgr_r_size_max);

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

  if (grp.gr_name && strmatch (grp.gr_name, group))
   *gid = grp.gr_gid;
  free (buffer);
 }

 abortgroupsearch:

 return 0;
}

#endif

signed int parse_integer (const char *s) {
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

char parse_boolean (const char *s) {
 return s && (strmatch (s, "true") || strmatch (s, "enabled") || strmatch (s, "yes"));
}

char *apply_variables (const char *ostring, const char **env) {
 char *ret = NULL, *vst = NULL, tsin = 0;
 uint32_t len = 0, rpos = 0, spos = 0, rspos = 0;
 char *string;

 if (!ostring || !(string = estrdup(ostring))) return NULL;
 if (!env) return estrdup (ostring);

 ret = emalloc (len = (strlen (string) + 1));
 *ret = 0;

 for (spos = 0, rpos = 0; string[spos]; spos++) {
  if ((string[spos] == '$') && (string[spos+1] == '{')) {
   for (rspos -= (rspos > 1) ? 2 : rspos; string[rspos] && (rspos < spos); rspos++) {
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
     if (strmatch (env[i], vst)) {
      xi = i+1; break;
     }
    }
    if (xi) {
     len = len - strlen (vst) - 2 + strlen (env[xi]);
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

 free (string);

 return ret;
}

char *escape_xml (const char *input) {
 char *retval = NULL;
 if (input) {
  ssize_t olen = strlen (input)+1,
   blen = olen + BUFFERSIZE,
   cpos = 0, tpos = 0;

  retval = emalloc (blen);

  for (cpos = 0; input[cpos]; cpos++) {
   if (tpos > (blen -7)) {
    blen += BUFFERSIZE;
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

#ifdef DEBUG

/* some stdio wrappers with error reporting */
FILE *exfopen (const char *filename, const char *mode, const char *file, const int line, const char *function) {
 const char *lfile          = file ? file : "unknown";
 const char *lfunction      = function ? function : "unknown";
 const int lline            = line ? line : 0;

 FILE *retval = fopen (filename, mode);

 if (retval) return retval;

 bitch_macro (bitch_stdio, lfile, lline, lfunction, errno, "fopen() failed.");
 return NULL;
}

DIR *exopendir (const char *name, const char *file, const int line, const char *function) {
 const char *lfile          = file ? file : "unknown";
 const char *lfunction      = function ? function : "unknown";
 const int lline            = line ? line : 0;

 DIR *retval = opendir (name);

 if (retval) return retval;

 bitch_macro (bitch_stdio, lfile, lline, lfunction, errno, "opendir() failed.");
 return NULL;
}

struct dirent *exreaddir (DIR *dir, const char *file, const int line, const char *function) {
 const char *lfile          = file ? file : "unknown";
 const char *lfunction      = function ? function : "unknown";
 const int lline            = line ? line : 0;

 errno = 0;

 struct dirent *retval = readdir (dir);

 if (retval) return retval;

 if (errno) {
  bitch_macro (bitch_stdio, lfile, lline, lfunction, errno, "readdir() failed.");
 }
 return NULL;
}

int exopen(const char *pathname, int mode, const char *file, const int line, const char *function) {
 const char *lfile          = file ? file : "unknown";
 const char *lfunction      = function ? function : "unknown";
 const int lline            = line ? line : 0;

 int retval = open (pathname, mode);

 if (retval != -1) return retval;

 bitch_macro (bitch_stdio, lfile, lline, lfunction, errno, "open() failed.");
 return -1;
}

#endif

#ifndef _have_asm_strmatch
char strmatch (const char *str1, const char *str2) {
 while (*str1 == *str2) {
  if (!*str1) return 1;

  str1++, str2++;
 }
 return 0;
}
#endif

#ifndef _have_asm_hashp
uintptr_t hashp (const char *str) {
 uintptr_t rv = 0;

 while (*str) {
  rv += *str;
  str++;
 }

 return rv;
}
#endif

void enable_core_dumps() {
 struct rlimit infinite = {
  .rlim_cur = RLIM_INFINITY,
  .rlim_max = RLIM_INFINITY
 };

 setrlimit(RLIMIT_CORE, &infinite);
}

void disable_core_dumps() {
 struct rlimit zero = {
  .rlim_cur = 0,
  .rlim_max = 0
 };

 setrlimit(RLIMIT_CORE, &zero);
}

#if ! defined (EINIT_UTIL)

char **getpath_filter (char *filter) {
 char **rv = NULL;

 return rv;
}

char *joinpath (char *path1, char *path2) {
 char *rv = NULL;
 int tlen = strlen (path1);

 if (path1[tlen] == '/') {
  tlen += strlen (path2) + 1;
  rv = emalloc (tlen);

  esprintf (rv, tlen, "%s%s", path1, path2);
 } else {
  tlen += strlen (path2) + 2;
  rv = emalloc (tlen);

  esprintf (rv, tlen, "%s/%s", path1, path2);
 }

 return rv;
}

char **which (char *binary) {
 char **rv = NULL;
 char n = 0;
 char **env;

 if (!binary) return NULL;

 for (; n < 2; n++) {
  struct stat st;

  switch (n) {
   case 0: env = einit_global_environment; break;
   case 1: env = einit_initial_environment; break;
   default: env = NULL; break;
  }

  if (env) {
   int i = 0;

   for (; env[i]; i++) {
    if (strstr (env[i], "PATH=") == env[i]) {
     char **paths = str2set (':', env[i]+5);

     if (paths) {
      int j = 0;
      for (; paths[j]; j++) {
       char *t = joinpath (paths[j], binary);

       if (!stat (t, &st)) {
        if (!inset ((const void **)rv, t, SET_TYPE_STRING))
         rv = (char **)setadd ((void **)rv, t, SET_TYPE_STRING);
       }

       free (t);
      }
     }

     break;
    }
   }
  }
 }

 return rv;
}

int unlink_recursive (const char *file, char self) {
 struct stat st;
 int c = 0;

 if (!file || lstat (file, &st)) return 0;

 if (S_ISLNK (st.st_mode)) {
  if (self) unlink (file);
  return 1;
 }

 if (S_ISDIR (st.st_mode)) {
  DIR *d;
  struct dirent *e;

  d = eopendir (file);
  if (d != NULL) {
   while ((e = ereaddir (d))) {
    if (strmatch (e->d_name, ".") || strmatch (e->d_name, "..")) {
     continue;
    }

    char *f = joinpath ((char *)file, e->d_name);

    if (f) {
     if (!lstat (f, &st) && !S_ISLNK (st.st_mode) && S_ISDIR (st.st_mode)) {
      unlink_recursive (f, 1);
     }

     unlink (f);

     c++;

     free (f);
    }
   }

   eclosedir(d);
  }
 }

 if (self) {
  unlink (file);
  c++;
 }

 return c;
}

#endif
