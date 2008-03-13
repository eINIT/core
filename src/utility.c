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

#ifdef __linux__
#include <sys/syscall.h>
#include <linux/sched.h>
#include <signal.h>
#endif

long _getgr_r_size_max = 0, _getpw_r_size_max = 0;

#include <regex.h>
#include <dirent.h>

#ifndef __SuperFastHash__
#define __SuperFastHash__
#include <stdint.h>
#undef get16bits
#if (defined(__GNUC__) && defined(__i386__)) || defined(__WATCOMC__) \
  || defined(_MSC_VER) || defined (__BORLANDC__) || defined (__TURBOC__)
#define get16bits(d) (*((const uint16_t *) (d)))
#endif

#if !defined (get16bits)
#define get16bits(d) ((((uint32_t)(((const uint8_t *)(d))[1])) << 8)\
                       +(uint32_t)(((const uint8_t *)(d))[0]) )
#endif

uint32_t SuperFastHash (const char * data, int len) {
uint32_t hash = len, tmp;
int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= data[sizeof (uint16_t)] << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += *data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

#endif

char **readdirfilter (struct cfgnode const *node, const char *default_dir, const char *default_allow, const char *default_disallow, char recurse) {
 DIR *dir;
 struct dirent *entry;
 char **retval = NULL;
 char *tmp;
 ssize_t mplen = 0;
 char *px = NULL;

 regex_t allowpattern, disallowpattern;
 unsigned char haveallowpattern = 0, havedisallowpattern = 0;
 char *path = (char *)default_dir, *allow = (char *)default_allow, *disallow = (char *)default_disallow;

 if (node && node->arbattrs) {
  uint32_t i = 0;

  for (; node->arbattrs[i]; i+=2) {
   if (strmatch ("path", node->arbattrs[i])) path = node->arbattrs[i+1];
   else if (strmatch ("pattern-allow", node->arbattrs[i])) allow = node->arbattrs[i+1];
   else if (strmatch ("pattern-disallow", node->arbattrs[i])) disallow = node->arbattrs[i+1];
  }
 }

 if (!path) return NULL;

#if 0
 if (!recurse)
#endif
 if (coremode & einit_mode_sandbox) {
// override path in sandbox-mode to be relative
  while (path[0] == '/') path++;
 }

 if (!path[0]) return NULL;

 mplen = strlen(path) + 4;
 px = emalloc (mplen);
 strcpy (px, path);

 if (px[mplen-5] != '/') {
  px[mplen-4] = '/';
  px[mplen-3] = 0;
 }

 if (allow) {
  haveallowpattern = !eregcomp (&allowpattern, allow);
 }

 if (disallow) {
  havedisallowpattern = !eregcomp (&disallowpattern, disallow);
 }

 mplen += 4;
 dir = eopendir (path);
 if (dir != NULL) {
  while ((entry = ereaddir (dir))) {

   if (haveallowpattern && regexec (&allowpattern, entry->d_name, 0, NULL, 0)) continue;
   if (havedisallowpattern && !regexec (&disallowpattern, entry->d_name, 0, NULL, 0)) continue;

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
      efree (n);
     }

     /* add the dir itself afterwards */
     retval = set_str_add(retval, (void *)tmp);

     goto cleanup_continue;
    }
   } else if (!S_ISREG (sbuf.st_mode)) {
    goto cleanup_continue;
   }

   retval = set_str_add(retval, (void *)tmp);

   cleanup_continue:
   efree (tmp);
  }
  eclosedir (dir);
 }

 if (haveallowpattern) { haveallowpattern = 0; eregfree (&allowpattern); }
 if (havedisallowpattern) { havedisallowpattern = 0; eregfree (&disallowpattern); }

 efree (px);

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

 ret = set_str_add(environment, (void*)newitem);
 efree (newitem);

 return ret;
}

char *readfd_l (int fd, ssize_t *rl) {
 int rn = 0;
 void *buf = NULL;
 char *data = NULL;
 ssize_t blen = 0;

 buf = emalloc (BUFFERSIZE * 10);
 do {
  buf = erealloc (buf, blen + BUFFERSIZE * 10);
  if (buf == NULL) return NULL;
  rn = read (fd, (char *)(buf + blen), BUFFERSIZE * 10);
  blen = blen + rn;
 } while ((rn > 0) && (!errno || (errno == EAGAIN) || (errno == EINTR)));

 if (blen > -1) {
  data = erealloc (buf, blen+1);
  data[blen] = 0;
  if (blen > 0) {
   *(data+blen-1) = 0;
  } else {
   efree (data);
   data = NULL;
  }

  if (rl) *rl = blen;
 }

 return data;
}

char *readfile_l (const char *filename, ssize_t *rl) {
 int fd = 0;
 void *buf = NULL;
 char *data = NULL;
 struct stat st;

 if (!filename) return NULL;

/* make an exception to the no-0-length-files rule for stuff in /proc */
 if (stat (filename, &st) || S_ISDIR(st.st_mode) || ((st.st_size <= 0) && (!strprefix (filename, "/proc/")))) return NULL;

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
 size_t l = c*s;

 p = emalloc (l);
 memset (p, 0, l);

#if 0
 while (!(p = calloc (c, s))) {
  bitch(bitch_emalloc, 0, "call to calloc() failed.");
  sleep (1);
 }
#endif

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

void efree (void *p) {
 free (p);
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

pthread_mutex_t
 thread_rendezvous_mutex = PTHREAD_MUTEX_INITIALIZER,
 thread_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t
 thread_rendezvous_cond = PTHREAD_COND_INITIALIZER;

int thread_pool_count = 0;
int thread_pool_free_count = 0;
int thread_pool_max_count = 0;
int thread_pool_max_pool_size = 10;
char thread_pool_prune = 0;

struct thread_wrapper_data {
 void *(*thread)(void *);
 void *param;
};

struct thread_rendezvous_data_s {
 struct thread_wrapper_data *d;
 struct thread_rendezvous_data_s *next;
} *thread_rendezvous_data = NULL;

struct thread_wrapper_data *thread_wrapper_rendezvous () {
 emutex_lock (&thread_rendezvous_mutex);

 moar:

 if (thread_rendezvous_data) {
  struct thread_wrapper_data *d = thread_rendezvous_data->d;
  struct thread_rendezvous_data_s *s = thread_rendezvous_data;

  thread_rendezvous_data = thread_rendezvous_data->next;
  efree (s);
  emutex_unlock (&thread_rendezvous_mutex);

#if 0
  fprintf (stderr, " ** thread recycled!\n");
#endif
  return (d);
 }

 if (!thread_pool_prune && !pthread_cond_wait (&thread_rendezvous_cond, &thread_rendezvous_mutex)) {
  goto moar;
 }

 emutex_unlock (&thread_rendezvous_mutex);

 return NULL;
}

char run_thread_function_in_pool (struct thread_wrapper_data *d) {
 struct thread_rendezvous_data_s *s = emalloc (sizeof (struct thread_rendezvous_data_s));

 s->d = d;
 emutex_lock (&thread_rendezvous_mutex);
 s->next = thread_rendezvous_data;
 thread_rendezvous_data = s;
 emutex_unlock (&thread_rendezvous_mutex);

 pthread_cond_signal (&thread_rendezvous_cond);
 sched_yield();

 emutex_lock (&thread_rendezvous_mutex);
 struct thread_rendezvous_data_s *p = NULL;
 s = thread_rendezvous_data;

 while (s) {
  if (s->d == d) {
   if (p) {
    p->next = s->next;
   } else {
    thread_rendezvous_data = s->next;
   }

   efree (s);
   emutex_unlock (&thread_rendezvous_mutex);

   return 0;
  }

  p = s;
  s = s->next;
 }
 emutex_unlock (&thread_rendezvous_mutex);

 return 1;
}

void ethread_spawn_wrapper (struct thread_wrapper_data *d) {
 /* update the process environment, just in case */
 update_local_environment();

 emutex_lock (&thread_stats_mutex);
 thread_pool_count++;

 if (thread_pool_count > thread_pool_max_count) {
  thread_pool_max_count = thread_pool_count;
 }
 emutex_unlock (&thread_stats_mutex);

 moar:

 d->thread (d->param);
 efree (d);

 emutex_lock (&thread_stats_mutex);
 thread_pool_free_count++;

 if (thread_pool_free_count < thread_pool_max_pool_size) {
  emutex_unlock (&thread_stats_mutex);

  if ((d = thread_wrapper_rendezvous ())) {
   emutex_lock (&thread_stats_mutex);
   thread_pool_free_count--;
   emutex_unlock (&thread_stats_mutex);
   goto moar;
  }

  emutex_lock (&thread_stats_mutex);
 }

 thread_pool_free_count--;
 thread_pool_count--;
 emutex_unlock (&thread_stats_mutex);

 if (!thread_pool_free_count) {
  thread_pool_prune = 0;

//  fprintf (stderr, " ** thread pool pruning complete; %i/%i/%i\n", thread_pool_count, thread_pool_max_count, thread_pool_free_count);
 }

 pthread_exit (NULL);
}

/* thread helpers */
void ethread_spawn_detached (void *(*thread)(void *), void *param) {
 struct thread_wrapper_data *d = emalloc (sizeof (struct thread_wrapper_data));
 pthread_t th;

 d->thread = thread;
 d->param = param;

 if (run_thread_function_in_pool (d)) return;

 if (ethread_create (&th, NULL, (void *(*)(void *))ethread_spawn_wrapper, d))
  efree (d);
 else
  pthread_detach (th);
}

void ethread_spawn_detached_run (void *(*thread)(void *), void *param) {
 struct thread_wrapper_data *d = emalloc (sizeof (struct thread_wrapper_data));
 pthread_t th;

 d->thread = thread;
 d->param = param;

 if (run_thread_function_in_pool (d)) return;

 if (ethread_create (&th, NULL, (void *(*)(void *))ethread_spawn_wrapper, d)) {
  efree (d);
  thread(param);
 } else
  pthread_detach (th);
}

void ethread_prune_thread_pool () {
 thread_pool_prune = 1;

 pthread_cond_broadcast (&thread_rendezvous_cond);

// fprintf (stderr, "pool's closed!\n");
}

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

 struct einit_event *nev = emalloc (sizeof (struct einit_event));

 memcpy (nev, ev, sizeof (struct einit_event));

 if (nev->string) {
  int32_t l;
  char *np;
  nev = erealloc (nev, sizeof (struct einit_event) + (l = strlen (nev->string) +1));

  memcpy (np = ((char*)nev)+sizeof (struct einit_event), nev->string, l);

  nev->string = np;
 }

 if (ev->stringset) nev->stringset = set_str_dup_stable (ev->stringset);

 return nev;
}

struct einit_event *evinit (uint32_t type) {
 struct einit_event *nev = ecalloc (1, sizeof (struct einit_event));

 nev->type = type;

 return nev;
}

void evpurge (struct einit_event *ev) {
 if (ev->string) efree (ev->string);
 if (ev->stringset) efree (ev->stringset);

 evdestroy (ev);
}

void evdestroy (struct einit_event *ev) {
 efree (ev);
}


/* user/group functions */
int lookupuidgid (uid_t *uid, gid_t *gid, const char *user, const char *group) {
#if ! defined (__APPLE__)
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
     efree (buffer);
     return -1;
    case EINTR:
     continue;
    default:
     efree (buffer);
     goto abortusersearch;
   }
  }

  if (pwd.pw_name && strmatch (pwd.pw_name, user)) {
   *uid = pwd.pw_uid;

   if (!group && gid) *gid = pwd.pw_gid;
  }
  efree (buffer);
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
     efree (buffer);
     return -2;
    default:
     efree (buffer);
     goto abortgroupsearch;
   }
  }

  if (grp.gr_name && strmatch (grp.gr_name, group))
   *gid = grp.gr_gid;
  efree (buffer);
 }

 abortgroupsearch:
#endif

 return 0;
}

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
 char *ret = NULL, *vst = NULL, tsin = 0, *string;
 uint32_t len = 0, rpos = 0, spos = 0, rspos = 0;

 if (!ostring || !(string = estrdup(ostring))) return NULL;
 if (!env) return estrdup (ostring);

 ret = emalloc (len = (strlen (string) + 1));
 *ret = 0;

 for (spos = 0, rpos = 0; string[spos]; spos++) {
  if ((string[spos] == '$') && (string[spos+1] == '{')) {
   for (rspos += (tsin && (rspos > 1)) ? -2 : 1; string[rspos] && (rspos < spos); rspos++) {
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

     rspos = spos;
    } else {
     for (rspos -= (rspos > 1) ? 2 : rspos; string[rspos] && (rspos < spos); rspos++) {
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

 efree (string);

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

#ifndef _have_asm_strmatch
char strmatch (const char *str1, const char *str2) {
 while (*str1 == *str2) {
  if (!*str1) return 1;

  str1++, str2++;
 }
 return 0;
}
#endif

#ifndef _have_asm_strprefix
char strprefix (const char *str1, const char *str2) {
 if (!str1) return 0;
 if (!str2) return 1;

 while (*str1 && *str2 && (*str1 == *str2)) {
  str1++, str2++;
 }

 return *str2 == 0;

// return (strstr (str1, str2) == str1);
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

#ifdef DEBUG
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
#endif

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
    if (strprefix (env[i], "PATH=")) {
     char **paths = str2set (':', env[i]+5);

     if (paths) {
      int j = 0;
      for (; paths[j]; j++) {
       char *t = joinpath (paths[j], binary);

       if (!stat (t, &st)) {
        if (!inset ((const void **)rv, t, SET_TYPE_STRING))
         rv = set_str_add_stable(rv, t);
       }

       efree (t);
      }

      efree (paths);
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

     efree (f);
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

char *strip_empty_variables (char *string) {
 size_t i = 0, start = 0;
 char lev = 0;

 for (; string[i]; i++) {
  if (string[i] == '$') {
   lev = 1;
   start = i;
  } else if (lev == 1) {
   if (string[i] == '{') {
    lev = 2;
   } else {
    lev = 0;
   }
  } else if (lev == 2) {
   if (string[i] == '}') {
    for (i++; string[i]; i++, start++) {
     string[start] = string[i];
    }

    string[start] = 0;

    return strip_empty_variables (string);
   }
  }
 }

 return string;
}

struct stree *regex_cache = NULL;

int eregcomp_cache (regex_t * preg, const char * pattern, int cflags) {
 struct stree *cache_hit = regex_cache ? streefind (regex_cache, pattern, tree_find_first) : NULL;

 if (cache_hit) {
  memcpy (preg, cache_hit->value, sizeof (regex_t));
  return 0;
 } else {
  regex_t *n = emalloc (sizeof (regex_t));

  int r = regcomp (n, pattern, cflags);

  if (!r) {
   regex_cache = streeadd (regex_cache, pattern, n, SET_NOALLOC, NULL);
   memcpy (preg, n, sizeof (regex_t));
  } else {
   efree (n);
  }

  return r;
 }
}

void eregfree_cache (regex_t *preg) {
}

struct itree *einit_stable_strings = NULL;
pthread_mutex_t einit_stable_strings_mutex = PTHREAD_MUTEX_INITIALIZER;

const char *str_stabilise (const char *s) {
 if (!s) return NULL;

 //long hash = hashp(s);
 uint32_t hash = SuperFastHash(s,strlen(s));
 
 struct itree *i = einit_stable_strings ? itreefind (einit_stable_strings, hash, tree_find_first) : NULL;
 while (i) {
  if (!s[0]) {
   if (!(i->data)[0])
    return i->data;
  } else {
   if (i->data == s) {
    return s;
   }
   if (strmatch (s, i->data)) {
   // Why did this break the feedback events
   /*if (hash == i->key) {*/
    return i->data;
   }
  }

  i = itreefind (i, hash, tree_find_next);
 }

 /* getting 'ere means we didn't have the right string in the set */
 emutex_lock (&einit_stable_strings_mutex);
 /* we don't really care if we accidentally duplicate the string */
 i = itreeadd (einit_stable_strings, hash, (char *)s, tree_value_string);
 einit_stable_strings = i;
 emutex_unlock (&einit_stable_strings_mutex);

 return i->data;
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

char check_files (char **files) {
 if (files) {
  int i = 0;
  struct stat st;
  for (; files[i]; i++) {
   if (files[i][0] == '/') {
    if (stat (files[i], &st)) {
     return 0;
    }
   } else {
    char **w = which (files[i]);
    if (!w) {
     return 0;
    } else {
     efree (w);
    }
   }
  }
 }

 return 1;
}

char **utility_add_fs (char **xt, char *s) {
 if (s) {
  char **tmp = s[0] == '/' ? str2set ('/', s+1) : str2set ('/', s);
  uint32_t r = 0;

  for (r = 0; tmp[r]; r++);
  for (r--; tmp[r] && r > 0; r--) {
   tmp[r] = 0;
   char *comb = set2str ('-', (const char **)tmp);

   if (!inset ((const void **)xt, comb, SET_TYPE_STRING)) {
    xt = set_str_add (xt, (void *)comb);
   }

   efree (comb);
  }

  if (tmp) {
   efree (tmp);
  }
 }

 return xt;
}

char **utility_add_fs_all (char **xt, char *s) {
 if (s) {
  char **tmp = s[0] == '/' ? str2set ('/', s+1) : str2set ('/', s);
  uint32_t r = 0;

  for (r = 0; tmp[r]; r++);
  for (r--; tmp[r] && (r > 0); r--) {
   char *comb = set2str ('-', (const char **)tmp);

   if (!inset ((const void **)xt, comb, SET_TYPE_STRING)) {
    xt = set_str_add (xt, (void *)comb);
   }

   tmp[r] = 0;
   efree (comb);
  }

  if (!inset ((const void **)xt, tmp[0], SET_TYPE_STRING)) {
   xt = set_str_add (xt, (void *)tmp[0]);
  }

  if (tmp) {
   efree (tmp);
  }
 }

 return xt;
}

char *utility_generate_defer_fs (char **tmpxt) {
 char *tmp = NULL;

 char *tmpx = NULL;
 tmp = emalloc (BUFFERSIZE);

 if (tmpxt) {
  tmpx = set2str ('|', (const char **)tmpxt);
 }

 if (tmpx) {
  esprintf (tmp, BUFFERSIZE, "^fs-(root|%s)$", tmpx);
  efree (tmpx);
 }

 efree (tmpxt);

 return tmp;
}

char **utility_add_all_in_path (char **rv) {
 char n = 0;
 char **env;

 for (; n < 2; n++) {
  switch (n) {
   case 0: env = einit_global_environment; break;
   case 1: env = einit_initial_environment; break;
   default: env = NULL; break;
  }

  if (env) {
   int i = 0;

   for (; env[i]; i++) {
    if (strprefix (env[i], "PATH=")) {
     char **paths = str2set (':', env[i]+5);

     if (paths) {
      int j = 0;
      for (; paths[j]; j++) {
       rv = utility_add_fs_all(rv, paths[j]);
      }

      efree (paths);
     }

     break;
    }
   }
  }
 }

 return rv;
}

char *after_string_from_files (char **files) {
 char **fs = NULL;
 int ix = 0;

 for (; files[ix]; ix++) {
  if (files[ix][0] == '/') {
   fs = utility_add_fs(fs, files[ix]);
  } else {
   fs = utility_add_all_in_path(fs);
  }
 }

 if (fs)
  return utility_generate_defer_fs(fs);
 else
  return NULL;
}

void update_local_environment() {
 struct cfgnode *node = NULL;

 while ((node = cfg_findnode ("configuration-environment-global", 0, node))) {
  if (node->idattr && node->svalue) {
   setenv (node->idattr, node->svalue, 1);
  }
 }
}

pid_t efork() {
 pid_t child;
#ifdef __linux__
 child = syscall(__NR_clone, SIGCHLD, 0, NULL, NULL, NULL);
#else
 child = fork();
#endif

 if (child < 0) {
  sleep (1);
  return efork(); /* just retry... */
 }

 return child;
}
