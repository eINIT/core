/*
   rc.h 
   Header file for external applications to get RC information.
   Copyright 2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>
   Released under the GPLv2
   */

#ifndef __RC_H__
#define __RC_H__

#include <stdbool.h>

typedef enum
{
  rc_service_started,
  rc_service_stopped,
  rc_service_starting,
  rc_service_stopping,
  rc_service_inactive,
  rc_service_wasinactive,
  rc_service_coldplugged,
} rc_service_state_t;

bool rc_service_exists (const char *service);
bool rc_service_in_runlevel (const char *runlevel, const char *service);
bool rc_service_state (const char *service, const rc_service_state_t state);
bool rc_runlevel_starting (void);
bool rc_runlevel_stopping (void);
char *rc_get_runlevel (void);

char **rc_get_services_in_runlevel (const char *runlevel);
void rc_free_services (char **services);

/* Dependency tree structs and functions */
typedef struct rc_deptype
{
  char *type;
  char *services;
  struct rc_deptype *next;
} rc_deptype_t;

typedef struct rc_depinfo
{
  char *service;
  rc_deptype_t *depends;
  struct rc_depinfo *next;
} rc_depinfo_t;

void rc_free_deptree (rc_depinfo_t *deptree);
rc_depinfo_t *rc_load_deptree (const char *file);
rc_depinfo_t *get_depinfo (rc_depinfo_t *deptree, const char *service);
rc_deptype_t *get_deptype (rc_depinfo_t *depinfo, const char *type);
char **rc_get_depends (rc_depinfo_t *deptree,
		       char **types, char **services,
		       bool trace, bool always_valid, bool strict);

#endif
