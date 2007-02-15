/*
   rc-misc.c
   rc misc functions
   Copyright 2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>
   */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "strlist.h"
#include "rc-misc.h"

void einfon (const char *fmt, ...)
{
  va_list va;
  printf (" \033[32;01m*\033[0m ");
  va_start (va, fmt);
  printf (fmt, va);
  va_end (va);
}

void ewarnn (const char *fmt, ...)
{
  va_list va;
  printf (" \033[33;01m*\033[0m ");
  va_start (va, fmt);
  printf (fmt, va);
  va_end (va);
}

void eerrorn (const char *fmt, ...)
{
  va_list va;
  fprintf (stderr, " \033[31;01m*\033[0m ");
  va_start (va, fmt);
  fprintf (stderr, fmt, va);
  va_end (va);
}

void einfo (const char *fmt, ...)
{
  einfon (fmt);
  printf ("\n");
}

void ewarn (const char *fmt, ...)
{
  ewarn (fmt);
  printf ("\n");
}

void eerror (const char *fmt, ...)
{
  eerrorn (fmt);
  fprintf (stderr, "\n");
}

void eerrorx (const char *fmt, ...)
{
  eerror (fmt);
  exit (EXIT_FAILURE);
}

void *xcalloc (size_t n, size_t size)
{
  void *value = calloc (n, size);

  if (value)
    return value;

  eerrorx ("memory exhausted");
}

void *xmalloc (size_t size)
{
  void *value = malloc (size);

  if (value)
    return (value);

  eerrorx ("memory exhausted");
}

char *xstrdup (const char *str)
{
  char *value;

  if (! str)
    return (NULL);

  value = strdup (str);

  if (value)
    return (value);

  eerrorx ("memory exhausted");
}

char *strcatpaths (const char *path1, const char *paths, ...)
{
  va_list va;
  int length;
  int i;
  char *p;
  char *path;
  char *pathp;

  if (! path1 || ! paths)
    return (NULL);

  length = strlen (path1) + strlen (paths) + 3;
  i = 0;
  va_start (va, paths);
  while ((p = va_arg (va, char *)) != NULL)
    length += strlen (p) + 1;
  va_end (va);

  path = xmalloc (length);
  memset (path, 0, length);
  memcpy (path, path1, strlen (path1));
  pathp = path + strlen (path1) - 1;
  if (*pathp != '/')
    *pathp++ = '/';
  else
    pathp++;
  memcpy (pathp, paths, strlen (paths));
  pathp += strlen (paths);

  va_start (va, paths);
  while ((p = va_arg (va, char *)) != NULL)
    {
      if (*pathp != '/')
	*pathp++ = '/';
      i = strlen (p);
      memcpy (pathp, p, i);
      pathp += i;
    }
  va_end (va);

  *pathp++ = 0;

  return (path);
}

bool exists (const char *pathname)
{
  struct stat buf;

  if (! pathname)
    return (false);

  if (lstat (pathname, &buf) == 0)
    return (true);

  errno = 0;
  return (false);
}

bool is_file (const char *pathname)
{
  struct stat buf;

  if (! pathname)
    return (false);

  if (lstat (pathname, &buf) == 0)
    return (S_ISREG (buf.st_mode));
    
  errno = 0;
  return (false);
}

bool is_dir (const char *pathname)
{
  struct stat buf;

  if (! pathname)
    return (false); 

  if (lstat (pathname, &buf) == 0)
    return (S_ISDIR (buf.st_mode));

  errno = 0;
  return (false);
}

bool is_link (const char *pathname)
{
  struct stat buf;

  if (! pathname)
    return (false); 

  if (lstat (pathname, &buf) == 0)
    return (S_ISLNK (buf.st_mode));
    
  errno = 0;
  return (false);
}

time_t get_mtime (const char *pathname, bool follow_link)
{
  struct stat buf;
  int retval;

  if (! pathname)
    return (0);

  retval = follow_link ? stat (pathname, &buf) : lstat (pathname, &buf);
  if (retval == 0)
    return (buf.st_mtime);

  errno = 0;
  return (0);
}

char **load_config (char **list, const char *file)
{
  FILE *fp;
  char buffer[LINEBUFFER];
  char *p;
  char *token;
  char *line;
  char *linep;
  char *linetok;
  int i = 0;
  bool replaced;
  char *entry;
  char *newline;

  if (! (fp = fopen (file, "r")))
    {
      ewarn ("load_config_file `%s': %s", file, strerror (errno));
      return (list);
    }

  while (fgets (buffer, LINEBUFFER, fp))
    {
      p = buffer;

      /* Strip leading spaces/tabs */
      while ((*p == ' ') || (*p == '\t'))
	p++;

      if (! p || strlen (p) < 3 || p[0] == '#')
	continue;

      /* Get entry */
      token = strsep (&p, "=");
      if (! token)
	continue;

      entry = xstrdup (token);

      do
	{
	  /* Bash variables are usually quoted */
	  token = strsep (&p, "\"\'");
	}
      while ((token) && (strlen (token) == 0));

      /* Drop a newline if that's all we have */
      if (token[0] == 10)
	token[0] = 0;

      i = strlen (entry) + strlen (token) + 2;
      newline = xmalloc (i);
      snprintf (newline, i, "%s=%s", entry, token);

      replaced = false;
      /* In shells the last item takes precedence, so we need to remove
	 any prior values we may already have */
      STRLIST_FOREACH (list, line, i)
	{
	  char *tmp = xstrdup (line);
	  linep = tmp; 
	  linetok = strsep (&linep, "=");
	  if (strcmp (linetok, entry) == 0)
	    {
	      /* We have a match now - to save time we directly replace it */
	      free (list[i - 1]);
	      list[i - 1] = newline; 
	      replaced = true;
	      free (tmp);
	      break;
	    }
	  free (tmp);
	}

      if (! replaced)
	{
	  STRLIST_ADD (list, newline);
	  free (newline);
	}
      free (entry);
    }
  fclose (fp);

  return (list);
}

char *get_config_entry (char **list, const char *entry)
{
  char *line;
  int i;
  char *p;

  STRLIST_FOREACH (list, line, i)
    {
      p = strchr (line, '=');
      if (p && strncmp (entry, line, p - line) == 0)
	return (p += 1);
    }

  return (NULL);
}

