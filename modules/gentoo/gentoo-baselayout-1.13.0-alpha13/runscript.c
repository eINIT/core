/*
 * runscript.c
 * Handle launching of Gentoo init scripts.
 *
 * Copyright 1999-2007 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dlfcn.h>

#include "rc.h"
#include "rc-misc.h"
#include "strlist.h"

#ifndef LIBDIR
#define LIBDIR		"lib"
#endif

#define SHELL		"/bin/bash"
#define SBIN_RC		"/sbin/rc"
#define PROFILE_ENV	"/etc/profile.env"
#define RCSCRIPTS_LIB	"/" LIBDIR "/rcscripts"
#define SYS_WHITELIST	RCSCRIPTS_LIB "/conf.d/env_whitelist"
#define USR_WHITELIST	"/etc/conf.d/env_whitelist"
#define RCSCRIPT_HELP	RCSCRIPTS_LIB "/sh/rc-help.sh"
#define SELINUX_LIB	RCSCRIPTS_LIB "/runscript_selinux.so"
#define SOFTLEVEL	"SOFTLEVEL"

#define DEFAULT_PATH	"PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/sbin"

#define IS_SBIN_RC()	((caller) && (strcmp (caller, SBIN_RC) == 0))

#if defined(WANT_SELINUX)
static void (*selinux_run_init_old) (void);
static void (*selinux_run_init_new) (int argc, char **argv);

void setup_selinux (int argc, char **argv);
#endif

extern char **environ;

#if defined(WANT_SELINUX)
void setup_selinux (int argc, char **argv)
{
  void *lib_handle = NULL;

  lib_handle = dlopen (SELINUX_LIB, RTLD_NOW | RTLD_GLOBAL);
  if (lib_handle)
    {
      selinux_run_init_old = dlsym (lib_handle, "selinux_runscript");
      selinux_run_init_new = dlsym (lib_handle, "selinux_runscript2");

      /* Use new run_init if it exists, else fall back to old */
      if (NULL != selinux_run_init_new)
	selinux_run_init_new (argc, argv);
      else if (NULL != selinux_run_init_old)
	selinux_run_init_old ();
      else
	/* This shouldnt happen... probably corrupt lib */
	eerrorx ("run_init is missing from runscript_selinux.so!");
    }
}
#endif

static char **get_list_file (char **list, const char *file)
{
  FILE *fp;
  char buffer[LINEBUFFER];
  char *p;
  char *token;

  if (! (fp = fopen (file, "r")))
    {
      ewarn ("get_list_file `%s': %s", file, strerror (errno));
      return (list);
    }

  while (fgets (buffer, LINEBUFFER, fp))
    {
      p = buffer;

      /* Strip leading spaces/tabs */
      while ((*p == ' ') || (*p == '\t'))
	p++;

      /* Get entry - we do not want comments, and only the first word
       * on a line is valid */
      token = strsep (&p, "# \t");
      if (token && (strlen (token) > 1))
	{
	  token[strlen (token) - 1] = 0;
	  STRLIST_ADD (list, token);
	}
    }
  fclose (fp);

  return (list);
}

static char **filter_env (char *caller)
{
  char **myenv = NULL;
  char **whitelist = NULL;
  char *env_name = NULL;
  char **profile = NULL;
  int count = 0;

  if (getenv (SOFTLEVEL) && ! IS_SBIN_RC ())
    /* Called from /sbin/rc, but not /sbin/rc itself, so current
     * environment should be fine */
    return environ;

  whitelist = get_list_file (whitelist, SYS_WHITELIST);
  if (! whitelist)
    ewarn ("System environment whitelist missing!");

  whitelist = get_list_file (whitelist, USR_WHITELIST);

  /* If no whitelist is present, revert to old behaviour */
  if (! whitelist)
    return environ;

  if (is_file (PROFILE_ENV))
    profile = load_config (profile, PROFILE_ENV);

  STRLIST_FOREACH (whitelist, env_name, count)
    {
      char *env_var = getenv (env_name);
      int env_len;
      char *tmp_p;

      if (! env_var && profile)
	{
	  env_len = strlen (env_name) + strlen ("export ") + 1;
	  tmp_p = xmalloc (sizeof (char *) * env_len);
	  snprintf (tmp_p, env_len, "export %s", env_name);
	  env_var = get_config_entry (profile, tmp_p);
	  free (tmp_p);
	}

      if (! env_var)
	continue;

      env_len = strlen (env_name) + strlen (env_var) + 2;
      tmp_p = xmalloc (sizeof (char *) * env_len);
      snprintf (tmp_p, env_len, "%s=%s", env_name, env_var);
      STRLIST_ADD (myenv, tmp_p);
      free (tmp_p);
    }

  STRLIST_FREE (whitelist);
  STRLIST_FREE (profile);

  return myenv;
}

int
main (int argc, char *argv[])
{
  char *myargs[32];
  char **myenv = NULL;
  char *caller = argv[1];
  int new = 1;

  /* Need to be /bin/bash, else BASH is invalid */
  myargs[0] = (char *) SHELL;
  while (argv[new])
    {
      myargs[new] = argv[new];
      new++;
    }
  myargs[new] = NULL;

  /* Do not do help for /sbin/rc */
  if (argc < 3 && ! IS_SBIN_RC ())
    {
      execv (RCSCRIPT_HELP, myargs);
      exit (EXIT_FAILURE);
    }

  /* Setup a filtered environment according to the whitelist */
  myenv = filter_env (caller);
  if (! myenv)
    {
      ewarn ("%s: Failed to filter the environment", caller);
      /* XXX: Might think to bail here, but it could mean the system
       *      is rendered unbootable, so rather not */
      myenv = environ;
    }

#if defined(WANT_SELINUX)
  /* Ok, we are ready to go, so setup selinux if applicable */
  setup_selinux (argc, argv);
#endif

  if (! IS_SBIN_RC ())
    {
      if (execve ("/sbin/runscript.sh", myargs, myenv) < 0)
	exit (EXIT_FAILURE);
    }
  else
    {
      if (execve (SHELL, myargs, myenv) < 0)
	exit (EXIT_FAILURE);
    }

  STRLIST_FREE (myenv);
  return (EXIT_SUCCESS);
}
