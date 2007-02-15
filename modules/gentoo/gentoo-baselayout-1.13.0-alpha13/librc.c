/*
   librc 
   core RC functions
   Copyright 2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>
   Released under the GPLv2
   */

#ifndef LIBDIR
#define LIBDIR "lib"
#endif

#define SVCDIR 		"/" LIBDIR "/rcscripts/init.d/"
#define SOFTLEVEL	SVCDIR "softlevel"
#define DEPTREE 	SVCDIR "deptree"
#define RUNLEVELDIR 	"/etc/runlevels/"
#define INITDIR		"/etc/init.d/"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

static const char *rc_service_state_names[] = {
  "started",
  "stopped",
  "starting",
  "stopping",
  "inactive",
  "wasinactive",
  "coldplugged",
  NULL
};

bool rc_runlevel_starting (void)
{
  return (is_dir(SVCDIR "/softscripts.old"));
}

bool rc_runlevel_stopping (void)
{
  return (is_dir (SVCDIR "/softscripts.new"));
}

char *rc_get_runlevel (void)
{
  FILE *fp;
  static char buffer [LINEBUFFER];

  if (! (fp = fopen (SOFTLEVEL, "r")))
    {
      strcpy (buffer, "single");
      return (buffer);
    }

  if (fgets (buffer, LINEBUFFER, fp))
    {
      int i = strlen (buffer) - 1;
      if (buffer[i] == '\n')
	buffer[i] = 0;
      fclose (fp);
      return (buffer);
    }

  fclose (fp);
  strcpy (buffer, "single");
  return (buffer);
}

bool rc_service_exists (const char *service)
{
  char *file;
  bool retval;

  if (! service)
    return (false);

  file = strcatpaths (INITDIR, service, NULL);
  retval = exists (file);
  free (file);
  return (retval);
}

bool rc_service_in_runlevel (const char *runlevel, const char *service)
{
  char *file;
  bool retval;

  if (! runlevel || ! service)
    return (false);

  if (! rc_service_exists (service))
    return (false);

  file = strcatpaths (RUNLEVELDIR, runlevel, service, NULL);
  retval = exists (file);
  free (file);
  return (retval);
}

bool rc_service_state (const char *service, const rc_service_state_t state)
{
  char *file;
  bool retval;

  if (! service)
    return (rc_service_stopped);

  /* If the init script does not exist then we are stopped */
  if (! rc_service_exists (service))
      return (rc_service_stopped);

  /* We check stopped state by not being in any of the others */
  if (state == rc_service_stopped)
    return ( ! (rc_service_state (service, rc_service_started) ||
		rc_service_state (service, rc_service_starting) ||
		rc_service_state (service, rc_service_stopping) ||
		rc_service_state (service, rc_service_inactive)));

  /* Now we just check if a file by the service name exists
     in the state dir */
  file = strcatpaths (SVCDIR, rc_service_state_names[state], service, NULL);
  retval = exists (file);
  free (file);
  return (retval);
}

char **rc_get_services_in_runlevel (const char *runlevel)
{
  DIR *dp;
  char *dir;
  struct dirent *d;
  char **list = NULL;

  if (! runlevel)
    return (NULL);

  dir = strcatpaths (RUNLEVELDIR, runlevel, NULL);
  if ((dp = opendir (dir)) == NULL)
    {
      free (dir);
      return (NULL);
    }

  errno = 0;
  while (((d = readdir (dp)) != NULL) && errno == 0)
    {
      if (d->d_name[0] != '.' &&
	  (d->d_type == DT_REG || d->d_type == DT_LNK))
	STRLIST_ADDSORT (list, d->d_name);
    }
  closedir (dp);

  if (errno != 0)
    {
      printf ("failed to readdir `%s': %s", dir, strerror (errno));
      STRLIST_FREE (list);
      return (NULL);
    }

  return (list);
}

void rc_free_services (char **services)
{
  STRLIST_FREE (services);
}
