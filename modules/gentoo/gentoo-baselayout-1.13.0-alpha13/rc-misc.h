/*
   rc-misc.h
   rc misc functions
   Copyright 2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>
   */

#ifndef __RC_MISC_H__
#define __RC_MISC_H__

#include <stdbool.h>
#include <time.h>

#ifdef __linux__
#define NORETURN __attribute__ ((__noreturn__))
#else
#define NORETURN __dead2
#endif

/* Max buffer to read a line from a file */
#define LINEBUFFER 1024

/* Gentoo style e* messages */
void einfon (const char *fmt, ...);
void ewarnn (const char *fmt, ...);
void eerrorn (const char *fmt, ...);
void einfo (const char *fmt, ...);
void ewarn (const char *fmt, ...);
void eerror (const char *fmt, ...);
void eerrorx (const char *fmt, ...) NORETURN;

void *xcalloc (size_t n, size_t size);
void *xmalloc (size_t size);
char *xstrdup (const char *str);

/* Concat paths adding '/' if needed. */
char *strcatpaths (const char *path1, const char *paths, ...);

bool exists (const char *pathname);
bool is_file (const char *pathname);
bool is_link (const char *pathname);
bool is_dir (const char *pathname);

time_t get_mtime (const char *pathname, bool follow_link);

/* Config file functions */
char **load_config (char **list, const char *file);
char *get_config_entry (char **list, const char *entry);

#endif
