/*
   rc-depend
   rc service dependency and ordering
   Copyright 2006-2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>

   We can also handle other dependency files and types, like Gentoo
   net modules. These are used with the --deptree and --awlaysvalid flags.
   */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

int main (int argc, char **argv)
{
  char **types = NULL;
  char **services = NULL;
  char **depends = NULL;
  rc_depinfo_t *deptree = NULL;
  rc_depinfo_t *di;
  char *service;
  
  bool trace = true;
  bool always_valid = false;
  bool strict = false;
  bool first = true;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--alwaysvalid") == 0)
	{
	  always_valid = true;
	  continue;
	}

      if (strcmp (argv[i], "--strict") == 0)
	{
	  strict = true;
	  continue;
	}

      if (strcmp (argv[i], "--deptree") == 0)
	{
	  i++;
	  if (i == argc)
	    eerrorx ("no deptree specified");

	  if ((deptree = rc_load_deptree (argv[i])) == NULL)
	    {
	      STRLIST_FREE (types);
	      STRLIST_FREE (services);
	      eerrorx ("failed to load deptree `%s'", argv[i]);
	    }

	  continue;
	}

      if (strcmp (argv[i], "--notrace") == 0)
	{
	  trace = false;
	  continue;
	}

      if (argv[i][0] == '-')
	{
	  argv[i]++;
	  STRLIST_ADD (types, argv[i]);
	}
      else
	{
	  if (! deptree && ((deptree = rc_load_deptree (NULL)) == NULL))
	    {
	      STRLIST_FREE (types);
	      STRLIST_FREE (services);
	      eerrorx ("failed to load deptree");
	    }

	  di = get_depinfo (deptree, argv[i]);
	  if (! di)
	    ewarn ("no dependency info for service `%s'", argv[i]);
	  else
	    STRLIST_ADD (services, argv[i]);
	}
    }

  if (! services)
    {
      STRLIST_FREE (types);
      rc_free_deptree (deptree);
      eerrorx ("no services specified");
    }

  /* If we don't have any types, then supply some defaults */
  if (! types)
    {
      STRLIST_ADD (types, "ineed");
      STRLIST_ADD (types, "iuse");
    }

  depends = rc_get_depends (deptree, types, services,
			    trace, always_valid, strict);

  if (depends)
    {
      STRLIST_FOREACH (depends, service, i)
	{
	  if (first)
	    first = false;
	  else
	    printf (" ");

	  if (service)
	    printf ("%s", service);

	}
      printf ("\n");
    }

  STRLIST_FREE (types);
  STRLIST_FREE (services);
  STRLIST_FREE (depends);
  rc_free_deptree (deptree);

  return (EXIT_SUCCESS);
}
