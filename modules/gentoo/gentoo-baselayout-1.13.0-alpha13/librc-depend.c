/*
   librc-depend
   rc service dependency and ordering
   Copyright 2006-2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>

   We can also handle other dependency files and types, like Gentoos
   net modules. These are used with the --deptree and --awlaysvalid flags.
   */

#ifndef LIBDIR
#define LIBDIR 		"lib"
#endif

#define SVCDIR 		"/" LIBDIR "/rcscripts/init.d"
#define DEPTREE 	SVCDIR "/deptree"

#define BOOTLEVEL	"boot"

#define MAXTYPES 	20 /* We currently only go upto 10 (providedby) */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

/* We use this so we can pass our char array through many functions */
struct lhead
{
  char **list;
};

static char *get_shell_value (char *string)
{
  char *p = string;
  char *e;

  if (! string)
    return (NULL);

  if (*p == '"')
    p++;

  e = p + strlen (p) - 1;
  if (*e == '\n')
    *e-- = 0;
  if (*e == '"')
    *e-- = 0;

  if (*p != 0)
    return p;

  return (NULL);
}

void rc_free_deptree (rc_depinfo_t *deptree)
{
  rc_depinfo_t *di = deptree;
  while (di)
    {
      rc_depinfo_t *dip = di->next;
      rc_deptype_t *dt = di->depends;
      free (di->service);
      while (dt)
	{
	  rc_deptype_t *dtp = dt->next;
	  free (dt->type);  
	  free (dt->services);
	  free (dt);
	  dt = dtp;
	}
      free (di);
      di = dip;
    }
}

/* Load our deptree
   Although the deptree file is pure sh, it is in a fixed format created
   by gendeptree awk script. We depend on this, if you pardon the pun ;)
   */
rc_depinfo_t *rc_load_deptree (const char *file)
{
  FILE *fp;
  char *types[MAXTYPES];
  rc_depinfo_t *deptree = xmalloc (sizeof (rc_depinfo_t));
  rc_depinfo_t *depinfo = NULL;
  rc_deptype_t *deptype = NULL;
  char buffer [LINEBUFFER];
  int rc_type_len = strlen ("declare -r rc_type_");
  int rc_depend_tree_len = strlen ("RC_DEPEND_TREE[");
  int i, max_type = 0;
  char *p, *e, *f, *x;
  long t, idx, val;

  if (! file)
    {
      if (! (fp = fopen (DEPTREE, "r")))
	return (NULL);
    }
  else
    {
      if (! (fp = fopen (file, "r")))
	return (NULL);
    }

  memset (types, 0, MAXTYPES);
  memset (deptree, 0, sizeof (rc_depinfo_t));

  while (fgets (buffer, LINEBUFFER, fp))
    {
      /* Grab our types first */
      if (strncmp (buffer, "declare -r rc_type_", rc_type_len) == 0)
	{
	  p = buffer + rc_type_len;
	  if (! (e = strchr(p, '=')))
	    continue;

	  /* Blank out the = sign so we can just copy the text later */
	  *e = 0;
	  e++;

	  errno = 0;
	  t = strtol (e, &f, 10);
	  if ((errno == ERANGE && (t == LONG_MAX || t == LONG_MIN))
	      || (errno != 0 && t == 0))
	    continue;

	  types[t] = xstrdup (p);
	  if (t > max_type)
	    max_type = t;

	  continue;
	}

      if (strncmp (buffer, "RC_DEPEND_TREE[", rc_depend_tree_len))
	continue;

      p = buffer + rc_depend_tree_len;
      e = NULL;

      errno = 0;
      idx = strtol (p, &e, 10);
      if ((errno == ERANGE && (idx == LONG_MAX || idx == LONG_MIN))
	  || (errno != 0 && idx == 0))
	{
	  warnx ("load_deptree: `%s' is not an index", p);
	  continue;
	}

      if (idx == 0)
	continue;

      /* If we don't have a + then we're a new service
	 OK, this is a hack, but it works :) */
      if (*e == ']')
	{
	  e += 2; // ]=
	  if (! depinfo)
	    depinfo = deptree;
	  else
	    {
	      depinfo->next = xmalloc (sizeof (rc_depinfo_t));
	      depinfo = depinfo->next;
	      memset (depinfo, 0, sizeof (rc_depinfo_t));
	    }
	  deptype = NULL;
	  depinfo->service = xstrdup (get_shell_value (e));
	  continue;
	}

      /* Sanity */
      if (*e != '+')
	{
	  warnx ("load_deptree: expecting `+', got `%s'", e);
	  continue;
	}

      /* Now we need to work out our service value */
      p = e + 1;
      errno = 0;
      val = strtol (p, &e, 10);
      if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
	  || (errno != 0 && val == 0))
	{
	  warnx ("load_deptree: `%s' is not an service type", p);
	  continue;
	}

      if (! types[val])
	{
	  warnx ("load_deptree: we don't value a type for index `%li'", val);
	  continue;
	}

      if (*e != ']')
	{
	  warnx ("load_deptree: expecting `]', got `%s'", e);
	  continue;
	}
      e++;
      if (*e != '=')
	{
	  warnx ("load_deptree: expecting `=', got `%s'", e);
	  continue;
	}
      e++;

      /* If we don't have a value then don't bother to add the dep */
      x = get_shell_value (e++);
      if (! x)
	continue;

      if (deptype)
	{
	  deptype->next = xmalloc (sizeof (rc_deptype_t));
	  deptype = deptype->next;
	}
      else
	{
	  depinfo->depends = xmalloc (sizeof (rc_deptype_t));
	  deptype = depinfo->depends;
	}
      memset (deptype, 0, sizeof (rc_deptype_t));

      deptype->type = xstrdup (types[val]);
      deptype->services = xstrdup (x);
    }

  fclose (fp);

  if (! depinfo)
    {
      free (deptree);
      deptree = NULL;
    }

  for (i = 0; i <= max_type; i++)
    if (types[i])
      free (types[i]);

  return (deptree);
}

rc_depinfo_t *get_depinfo (rc_depinfo_t *deptree, const char *service)
{
  rc_depinfo_t *di;

  if (! deptree || ! service)
    return (NULL);

  for (di = deptree; di; di = di->next)
    if (strcmp (di->service, service) == 0)
      return (di);

  return (NULL);
}

rc_deptype_t *get_deptype (rc_depinfo_t *depinfo, const char *type)
{
  rc_deptype_t *dt;

  if (! depinfo || !type)
    return (NULL);

  for (dt = depinfo->depends; dt; dt = dt->next)
    if (strcmp (dt->type, type) == 0)
      return (dt);

  return (NULL);
}

static bool valid_service (const char *runlevel, const char *service)
{
  return ((strcmp (runlevel, BOOTLEVEL) != 0 &&
	   rc_service_in_runlevel (BOOTLEVEL, service)) ||
	  rc_service_in_runlevel (runlevel, service) ||
	  rc_service_state (service, rc_service_coldplugged) ||
	  rc_service_state (service, rc_service_started));
}

static bool get_provided1 (char *runlevel, struct lhead *providers,
			   rc_deptype_t *deptype,
			   const char *level, bool coldplugged,
			   bool started, bool inactive)
{
  char *service;
  char *p;
  char *op;
  bool retval = false;

  op = p = xstrdup (deptype->services);
  while ((service = strsep (&p, " ")))
    {
      bool ok = true;
      if (level)
	ok = rc_service_in_runlevel (level, service);
      else if (coldplugged)
	ok = (rc_service_state (service, rc_service_coldplugged) &&
	      ! rc_service_in_runlevel (runlevel, service) &&
	      ! rc_service_in_runlevel (BOOTLEVEL, service));

      if (! ok)
	continue;

      if (started)
	ok = (rc_service_state (service, rc_service_starting) ||
	      rc_service_state (service, rc_service_started) ||
	      rc_service_state (service, rc_service_stopping));
      else if (inactive)
	ok = rc_service_state (service, rc_service_inactive);

      if (! ok)
	continue;
    
      retval = true;
      STRLIST_ADD (providers->list, service);
    }
  free (op);

  return (retval);
}

/* Work out if a service is provided by another service.
   For example metalog provides logger.
   We need to be able to handle syslogd providing logger too.
   We do this by checking whats running, then what's starting/stopping,
   then what's run in the runlevels and finally alphabetical order.

   If there are any bugs in rc-depend, they will probably be here as
   provided dependancy can change depending on runlevel state.
   */
static char **get_provided (rc_depinfo_t *deptree,
			    rc_depinfo_t *depinfo,
			    bool always_valid, bool strict)
{
  rc_deptype_t *dt;
  struct lhead providers; 
  char *p, *op, *service;
  char *runlevel = rc_get_runlevel ();

  if (! deptree || ! depinfo)
    return (NULL);
  if (rc_service_exists (depinfo->service))
    return (NULL);

  dt = get_deptype (depinfo, "providedby");
  if (! dt)
    return (NULL);

  memset (&providers, 0, sizeof (struct lhead));
  /* If we are stopping then all depends are true, regardless of state.
     This is especially true for net services as they could force a restart
     of the local dns resolver which may depend on net. */
  if (rc_runlevel_stopping () || always_valid)
    {
      op = p = xstrdup (dt->services);
      while ((service = strsep (&p, " ")))
	STRLIST_ADD (providers.list, service);
      free (op);

      return (providers.list);
    }

  /* If we're strict, then only use what we have in our runlevel */
  if (strict)
    {
      op = p = xstrdup (dt->services);
      while ((service = strsep (&p, " ")))
	if (rc_service_in_runlevel (runlevel, service))
	  STRLIST_ADD (providers.list, service);
      free (op);

      if (providers.list)
	return (providers.list);
    }

  /* OK, we're not strict or there were no services in our runlevel.
     This is now where the logic gets a little fuzzy :)
     If there is >1 running service then we return NULL.
     We do this so we don't hang around waiting for inactive services and
     our need has already been satisfied as it's not strict.
     We apply this to our runlevel, coldplugged services, then bootlevel
     and finally any running.*/
#define DO \
  if (providers.list && providers.list[0] && providers.list[1]) \
    { \
      STRLIST_FREE (providers.list); \
      return (NULL); \
    } \
  else if (providers.list)  \
    return providers.list; \

  /* Anything in the runlevel has to come first */
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, true, false))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, false, true))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, runlevel, false, false, false))
    return (providers.list);

  /* Check coldplugged started services */
  if (get_provided1 (runlevel, &providers, dt, NULL, true, true, false))
    { DO }

  /* Check bootlevel if we're not in it */
  if (strcmp (runlevel, BOOTLEVEL) != 0)
    {
      if (get_provided1 (runlevel, &providers, dt, BOOTLEVEL, false, true, false))
	{ DO }
      if (get_provided1 (runlevel, &providers, dt, BOOTLEVEL, false, false, true))
	{ DO }
    }

  /* Check coldplugged inactive services */
  if (get_provided1 (runlevel, &providers, dt, NULL, true, false, true))
    { DO }

  /* Check manually started */
  if (get_provided1 (runlevel, &providers, dt, NULL, false, true, false))
    { DO }
  if (get_provided1 (runlevel, &providers, dt, NULL, false, false, true))
    { DO }

  /* Nothing started then. OK, lets get the stopped services */
  if (get_provided1 (runlevel, &providers, dt, NULL, true, false, false))
    return (providers.list);
  if ((strcmp (runlevel, BOOTLEVEL) != 0)
      && (get_provided1 (runlevel, &providers, dt, BOOTLEVEL, false, false, false)))
    return (providers.list);

  /* Still nothing? OK, list all services */
  op = p = xstrdup (dt->services);
  while ((service = strsep (&p, " ")))
    STRLIST_ADD (providers.list, service);
  free (op);

  return (providers.list);
}

static void visit_service (rc_depinfo_t *deptree, char **types,
			   struct lhead *sorted, struct lhead *visited,
			   rc_depinfo_t *depinfo,
			   bool descend, bool always_valid, bool strict)
{
  int i, j;
  char *p, *op, *lp, *item;
  char *service;
  rc_depinfo_t *di;
  rc_deptype_t *dt;
  char **provides;
  char *svcname;
  char *runlevel = rc_get_runlevel ();

  if (! deptree || !sorted || !visited || !depinfo)
    return;

  /* Check if we have already visited this service or not */
  STRLIST_FOREACH (visited->list, item, i)
   if (strcmp (item, depinfo->service) == 0)
     return;

  /* Add ourselves as a visited service */
  STRLIST_ADD (visited->list, depinfo->service);

  STRLIST_FOREACH (types, item, i)
    {
      if ((dt = get_deptype (depinfo, item)))
	{
	  op = p = xstrdup (dt->services);
	  while ((service = strsep (&p, " ")))
	    {
	      if (! descend || strcmp (item, "iprovide") == 0)
		{
		  STRLIST_ADD (sorted->list, service);
		  continue;
		}

	      di = get_depinfo (deptree, service);
	      if ((provides = get_provided (deptree, di, always_valid, strict)))
		{
		  STRLIST_FOREACH (provides, lp, j) 
		    {
		      di = get_depinfo (deptree, lp);
		      if (di && (strcmp (item, "ineed") == 0 ||
				 always_valid ||
				 valid_service (runlevel, di->service)))
			visit_service (deptree, types, sorted, visited, di,
				       true, always_valid, strict);
		    }
		  STRLIST_FREE (provides);
		}
	      else
		if (di && (strcmp (item, "ineed") == 0 ||
			   always_valid ||
			   valid_service (runlevel, service)))
		  visit_service (deptree, types, sorted, visited, di,
				 true, always_valid, strict);
	    }
	  free (op);
	}
    }

  /* Now visit the stuff we provide for */
  if ((dt = get_deptype (depinfo, "iprovide")) && descend)
    {
      op = p = xstrdup (dt->services);
      while ((service = strsep (&p, " ")))
	{
	  if ((di = get_depinfo (deptree, service)))
	    if ((provides = get_provided (deptree, di, always_valid, strict)))
	      {
		i = 0;
		STRLIST_FOREACH (provides, lp, i)
		  if (strcmp (lp, depinfo->service) == 0)
		    {
		      visit_service (deptree, types, sorted, visited, di,
				     true, always_valid, strict);
		      break;
		    }
		STRLIST_FREE (provides);
	      }
	}
      free (op);
    }

  /* We've visited everything we need, so add ourselves unless we
     are also the service calling us or we are provided by something */
  svcname = getenv("SVCNAME");
  if (! svcname || strcmp (svcname, depinfo->service) != 0)
    if (! get_deptype (depinfo, "providedby"))
      STRLIST_ADD (sorted->list, depinfo->service);
}

char **rc_get_depends (rc_depinfo_t *deptree,
		       char **types, char **services,
		       bool trace, bool always_valid, bool strict)
{  
  struct lhead sorted;
  struct lhead visited;
  rc_depinfo_t *di;
  char *service;
  int i;

  if (! deptree || ! types || ! services)
    return (NULL);

  memset (&sorted, 0, sizeof (struct lhead));
  memset (&visited, 0, sizeof (struct lhead));

  STRLIST_FOREACH (services, service, i)
    {
      di = get_depinfo (deptree, service);
      visit_service (deptree, types, &sorted, &visited, di, trace,
		     always_valid, strict);
    }

  STRLIST_FREE (visited.list);
  return (sorted.list);
}
