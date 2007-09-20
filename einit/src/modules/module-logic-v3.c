/*
 *  module-logic-v4.c
 *  einit
 *
 *  Created by Magnus Deininger on 09/04/2007.
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <einit/config.h>
#include <einit/module-logic.h>
#include <einit/module.h>
#include <einit/tree.h>
#include <pthread.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit-modules/ipc.h>
#include <einit-modules/configuration.h>

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

#ifdef DEBUG
#undef DEBUG
#endif

#define MAX_ITERATIONS 1000
#define EINIT_PLAN_CHANGE_STALL_TIMEOUT 10

int einit_module_logic_v3_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_module_logic_v3_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "Module Logic Core (V3)",
 .rid       = "einit-module-logic-v3",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = einit_module_logic_v3_configure
};

module_register(einit_module_logic_v3_self);

#endif

extern char shutting_down;

struct stree *module_logic_rid_list = NULL;

void module_logic_ipc_event_handler (struct einit_event *);
void module_logic_einit_event_handler (struct einit_event *);
double mod_get_plan_progress_f (struct mloadplan *);
char initdone = 0;
char mod_isbroken (char *service);
char mod_mark (char *service, char task);
struct group_data *mod_group_get_data (char *group);
char mod_isprovided(char *service);
void module_logic_update_init_d ();

/* new functions: */
char mod_examine_group (char *);
void mod_examine_module (struct lmodule *);
void mod_examine (char *);

void mod_commit_and_wait (char **, char **);

void mod_defer_notice (struct lmodule *, char **);

void workthread_examine (char *service);
void mod_post_examine (char *service);
void mod_pre_examine (char *service);

char mod_isdeferred (char *service);

int einit_module_logic_list_revision = 0;

void mod_ping_all_threads();
void mod_wait_for_ping();
void mod_workthread_create(char *);

char mod_reorder (struct lmodule *, int, char *, char);

#ifdef DEBUG
#define debugfile stderr
// FILE *debugfile = stderr;
#endif

struct module_taskblock
  current = { NULL, NULL, NULL },
  target_state = { NULL, NULL, NULL };

struct stree *module_logics_service_list = NULL; // value is a (struct lmodule **)
struct stree *module_logics_group_data = NULL;

pthread_mutex_t
  ml_tb_current_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_tb_target_state_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_service_list_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_group_data_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_unresolved_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_currently_provided_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_service_update_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_rid_list_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_current_switches_mutex = PTHREAD_MUTEX_INITIALIZER,
  ml_garbage_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t
  ml_cond_service_update = PTHREAD_COND_INITIALIZER;

char **unresolved_services = NULL;
char **broken_services = NULL;
char **current_switches = NULL;

struct group_data {
 char **members;
 uint32_t options;
};

#define MOD_PLAN_GROUP_SEQ_ANY     0x00000001
#define MOD_PLAN_GROUP_SEQ_ALL     0x00000002
#define MOD_PLAN_GROUP_SEQ_ANY_IOP 0x00000004
#define MOD_PLAN_GROUP_SEQ_MOST    0x00000008

#define MARK_BROKEN                0x01
#define MARK_UNRESOLVED            0x02

/* module header functions */

void mod_ping_all_threads() {
#ifdef _POSIX_PRIORITY_SCHEDULING
 sched_yield();
#endif

 pthread_cond_broadcast (&ml_cond_service_update);
}

int einit_module_logic_v3_usage = 0;

struct eml_garbage {
 struct stree **strees;
 void **chunks;
} einit_module_logic_v3_garbage = {
 .strees = NULL,
 .chunks = NULL
};

void einit_module_logic_v3_free_garbage () {
 emutex_lock (&ml_garbage_mutex);
 if (einit_module_logic_v3_garbage.strees) {
  int i = 0;

  for (; einit_module_logic_v3_garbage.strees[i]; i++) {
   streefree (einit_module_logic_v3_garbage.strees[i]);
  }

  free (einit_module_logic_v3_garbage.strees);
  einit_module_logic_v3_garbage.strees = NULL;
 }

 if (einit_module_logic_v3_garbage.chunks) {
  int i = 0;

  for (; einit_module_logic_v3_garbage.chunks[i]; i++) {
   free (einit_module_logic_v3_garbage.chunks[i]);
  }

  free (einit_module_logic_v3_garbage.chunks);
  einit_module_logic_v3_garbage.chunks = NULL;
 }
 emutex_unlock (&ml_garbage_mutex);
}

void mod_wait_for_ping() {
 int e;

 emutex_lock (&ml_service_update_mutex);
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
 struct timespec ts;

 if (clock_gettime(CLOCK_REALTIME, &ts))
  bitch (bitch_stdio, errno, "gettime failed!");

 ts.tv_sec += 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (&ml_cond_service_update, &ml_service_update_mutex, &ts);
#elif defined(DARWIN)
#if 1
 struct timespec ts;
 struct timeval tv;

 gettimeofday (&tv, NULL);

 ts.tv_sec = tv.tv_sec + 1; /* max wait before re-evaluate */

 e = pthread_cond_timedwait (&ml_cond_service_update, &ml_service_update_mutex, &ts);
#else
// notice (2, "warning: un-timed lock.");
 e = pthread_cond_wait (&ml_cond_service_update, &ml_service_update_mutex);
#endif
#else
 notice (2, "warning: un-timed lock.");
 e = pthread_cond_wait (&ml_cond_service_update, &ml_service_update_mutex);
#endif
 emutex_unlock (&ml_service_update_mutex);

 if (e
#ifdef ETIMEDOUT
     && (e != ETIMEDOUT)
#endif
    ) {
  bitch (bitch_epthreads, e, "waiting on conditional variable for plan");
 }/* else {
  notice (1, "woke up, checking plan.\n");
 }*/
}

char mod_is_rid (char *rid) {
 char rv = 0;

 if (!rid) return 0;

 emutex_lock (&ml_rid_list_mutex);

 if (module_logic_rid_list) { 
  rv = (streefind (module_logic_rid_list, rid, tree_find_first)) ? 1 : 0;
 }

 emutex_unlock (&ml_rid_list_mutex);

 return rv;
}

struct lmodule *mod_find_by_rid (char *rid) {
 struct stree *rv = NULL;

 if (!rid) return 0;

 emutex_lock (&ml_rid_list_mutex);

 if (module_logic_rid_list) { 
  rv = streefind (module_logic_rid_list, rid, tree_find_first);
 }

 emutex_unlock (&ml_rid_list_mutex);

 return rv ? rv->value : NULL;
}

char mod_is_unambiguous (char *service) {
 char rv = 0;

 if (!service) return 0;

 if (module_logics_service_list) { 
  struct stree *t = streefind (module_logics_service_list, service, tree_find_first);

  if (t) {
   struct lmodule **lm = t->value;

   if (lm[0] && !lm[1]) rv = 1;
  }
 }

 return rv;
}

char mod_is_requested (char *service) {
 char rv = 0;
 uint32_t i = 0;

 if (!service) return 0;

 emutex_lock (&ml_tb_target_state_mutex);

 if (target_state.enable) {
  for (i = 0; target_state.enable[i]; i++) {
   if (strmatch (service, target_state.enable[i])) {
    rv = 1;
    break;
   }
  }
 }

 if (!rv && target_state.disable) {
  for (i = 0; target_state.disable[i]; i++) {
   if (strmatch (service, target_state.disable[i])) {
    rv = 1;
    break;
   }
  }
 }

 emutex_unlock (&ml_tb_target_state_mutex);

 return rv;
}

char mod_is_requested_rid (char *rid) {
 if (!rid) return 0;

 if (!mod_is_rid(rid)) return 0;

 return mod_is_requested (rid);
}

int einit_module_logic_v3_cleanup (struct lmodule *this) {
 function_unregister ("module-logic-get-plan-progress", 1, mod_get_plan_progress_f);

 event_ignore (einit_event_subsystem_core, module_logic_einit_event_handler);
 event_ignore (einit_event_subsystem_ipc, module_logic_ipc_event_handler);

 return 0;
}

struct eml_resume_data {
 struct stree *module_logics_service_list;
 struct stree *module_logics_group_data;
 struct module_taskblock current, target_state;
};

int einit_module_logic_v3_suspend (struct lmodule *this) {
 if (!einit_module_logic_v3_usage) {
  function_unregister ("module-logic-get-plan-progress", 1, mod_get_plan_progress_f);

  event_ignore (einit_event_subsystem_core, module_logic_einit_event_handler);
  event_ignore (einit_event_subsystem_ipc, module_logic_ipc_event_handler);

  sleep (1);
  event_wakeup (einit_event_subsystem_ipc, this);
  event_wakeup (einit_core_update_configuration, this);
  event_wakeup (einit_core_module_list_update, this);
  event_wakeup (einit_core_service_update, this);
  event_wakeup (einit_core_switch_mode, this);
  event_wakeup (einit_core_change_service_status, this);

  this->resumedata = ecalloc (1, sizeof (struct eml_resume_data));
  struct eml_resume_data *rd = this->resumedata;

  rd->module_logics_service_list = module_logics_service_list;
  rd->module_logics_group_data = module_logics_group_data;
  rd->current.critical = current.critical;
  rd->current.enable = current.enable;
  rd->current.disable = current.disable;
  rd->target_state.critical = target_state.critical;
  rd->target_state.enable = target_state.enable;
  rd->target_state.disable = target_state.disable;

  einit_module_logic_v3_free_garbage ();

  return status_ok;
 } else
  return status_failed;
}

int einit_module_logic_v3_resume (struct lmodule *this) {
 event_wakeup_cancel (einit_event_subsystem_ipc, this);
 event_wakeup_cancel (einit_core_update_configuration, this);
 event_wakeup_cancel (einit_core_module_list_update, this);
 event_wakeup_cancel (einit_core_service_update, this);
 event_wakeup_cancel (einit_core_switch_mode, this);
 event_wakeup_cancel (einit_core_change_service_status, this);

 if (this->resumedata) {
  struct eml_resume_data *rd = this->resumedata;

  module_logics_service_list = rd->module_logics_service_list;
  module_logics_group_data = rd->module_logics_group_data;

  current.critical = rd->current.critical;
  current.enable = rd->current.enable;
  current.disable = rd->current.disable;
  target_state.critical = rd->target_state.critical;
  target_state.enable = rd->target_state.enable;
  target_state.disable = rd->target_state.disable;

  free (this->resumedata);
 }

 return status_ok;
}

int einit_module_logic_v3_configure (struct lmodule *this) {
 module_init(this);

 thismodule->cleanup = einit_module_logic_v3_cleanup;

 thismodule->suspend = einit_module_logic_v3_suspend;
 thismodule->resume = einit_module_logic_v3_resume;

 event_listen (einit_event_subsystem_ipc, module_logic_ipc_event_handler);
 event_listen (einit_event_subsystem_core, module_logic_einit_event_handler);

 function_register ("module-logic-get-plan-progress", 1, mod_get_plan_progress_f);

 return 0;
}

/* end module header */
/* start common functions with v3 */

char mod_isbroken (char *service) {
 int retval = 0;

 emutex_lock (&ml_unresolved_mutex);

 retval = inset ((const void **)broken_services, (void *)service, SET_TYPE_STRING) ||
   inset ((const void **)unresolved_services, (void *)service, SET_TYPE_STRING);

 emutex_unlock (&ml_unresolved_mutex);

 return retval;
}

struct group_data *mod_group_get_data (char *group) {
 struct group_data *ret = NULL;

/* eputs ("mod_group_get_data", stderr);
 fflush (stderr);*/

 emutex_lock (&ml_group_data_mutex);

/* eputs ("got mutex", stderr);
 fflush (stderr);*/

 struct stree *cur = module_logics_group_data ? streefind (module_logics_group_data, group, tree_find_first) : NULL;
 if (cur) { ret = (struct group_data *)cur->value; }
 else {
  char *tnodeid = emalloc (strlen (group)+17);
  struct cfgnode *gnode = NULL;

  memcpy (tnodeid, "services-alias-", 16);
  strcat (tnodeid, group);

  ret = ecalloc (1, sizeof (struct group_data));

  if ((gnode = cfg_getnode (tnodeid, NULL)) && gnode->arbattrs) {
   ssize_t r = 0;

   for (r = 0; gnode->arbattrs[r]; r+=2) {
    if (strmatch (gnode->arbattrs[r], "group")) {
     ret->members = str2set (':', gnode->arbattrs[r+1]);
    } else if (strmatch (gnode->arbattrs[r], "seq")) {
     if (strmatch (gnode->arbattrs[r+1], "any"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY;
     else if (strmatch (gnode->arbattrs[r+1], "all"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ALL;
     else if (strmatch (gnode->arbattrs[r+1], "any-iop"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_ANY_IOP;
     else if (strmatch (gnode->arbattrs[r+1], "most"))
      ret->options |=  MOD_PLAN_GROUP_SEQ_MOST;
    }
   }
  }
  free (tnodeid);

  if (!ret->members || !ret->options) {
   free (ret);
   ret = NULL;
  } else {
   module_logics_group_data = streeadd (module_logics_group_data, group, (void *)ret, SET_NOALLOC, (void *)ret);
  }
 }

 emutex_unlock (&ml_group_data_mutex);

 return ret;
}

void cross_taskblock (struct module_taskblock *source, struct module_taskblock *target) {
 if (source->enable) {
  char **tmp = (char **)setcombine ((const void **)target->enable, (const void **)source->enable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((const void **)target->disable, (const void **)source->enable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;
 }
 if (source->critical) {
  char **tmp = (char **)setcombine ((const void **)target->critical, (const void **)source->critical, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }

 if (source->disable) {
  char **tmp = (char **)setcombine ((const void **)target->disable, (const void **)source->disable, SET_TYPE_STRING);
  if (target->disable) free (target->disable);
  target->disable = tmp;

  tmp = (char **)setslice ((const void **)target->enable, (const void **)source->disable, SET_TYPE_STRING);
  if (target->enable) free (target->enable);
  target->enable = tmp;

  tmp = (char **)setslice ((const void **)target->critical, (const void **)source->disable, SET_TYPE_STRING);
  if (target->critical) free (target->critical);
  target->critical = tmp;
 }
}

struct mloadplan *mod_plan (struct mloadplan *plan, char **atoms, unsigned int task, struct cfgnode *mode) {
 char disable_all_but_feedback = 0, disable_all = 0;

 if (!plan) {
  plan = emalloc (sizeof (struct mloadplan));
  memset (plan, 0, sizeof (struct mloadplan));
 }

 if (mode) {
  char **base = NULL;
  uint32_t xi = 0;
  char **enable   = str2set (':', cfg_getstring ("enable/services", mode));
  char **disable  = str2set (':', cfg_getstring ("disable/services", mode));
  char **critical = str2set (':', cfg_getstring ("enable/critical", mode));
  char *strng;

  if ((strng = cfg_getstring ("options/shutdown", mode)) && parse_boolean(strng)) {
   plan->options |= plan_option_shutdown;
  }

/* old syntax is old.... */
  if (!enable)
   enable  = str2set (':', cfg_getstring ("enable/mod", mode));
  if (!disable)
   disable = str2set (':', cfg_getstring ("disable/mod", mode));

  if (mode->arbattrs) for (; mode->arbattrs[xi]; xi+=2) {
   if (strmatch(mode->arbattrs[xi], "base")) {
    base = str2set (':', mode->arbattrs[xi+1]);
   }
  }

  if (enable) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.enable, (const void **)enable, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (disable) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.disable, (const void **)disable, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
  if (critical) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.critical, (const void **)critical, SET_TYPE_STRING);
   if (plan->changes.critical) free (plan->changes.critical);
   plan->changes.critical = tmp;
  }

  if (base) {
   int y = 0;
   struct cfgnode *cno;
   while (base[y]) {
    if (!inset ((const void **)plan->used_modes, (void *)base[y], SET_TYPE_STRING)) {
     cno = cfg_findnode (base[y], einit_node_mode, NULL);
     if (cno) {
      plan = mod_plan (plan, NULL, 0, cno);
     }
    }

    y++;
   }

   free (base);
  }

  if (mode->id) {
   plan->used_modes = (char **)setadd ((void **)plan->used_modes, mode->id, SET_TYPE_STRING);
  }

  plan->mode = mode;
 } else {
  if (task & einit_module_enable) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.enable, (const void **)atoms, SET_TYPE_STRING);
   if (plan->changes.enable) free (plan->changes.enable);
   plan->changes.enable = tmp;
  }
  if (task & einit_module_disable) {
   char **tmp = (char **)setcombine ((const void **)plan->changes.disable, (const void **)atoms, SET_TYPE_STRING);
   if (plan->changes.disable) free (plan->changes.disable);
   plan->changes.disable = tmp;
  }
 }

 disable_all = inset ((const void **)plan->changes.disable, (void *)"all", SET_TYPE_STRING);
 disable_all_but_feedback = inset ((const void **)plan->changes.disable, (void *)"all-but-feedback", SET_TYPE_STRING);

 if (disable_all || disable_all_but_feedback) {
  struct stree *cur;
  ssize_t i = 0;
  char **tmpy = service_usage_query_cr (service_is_provided, NULL, NULL);

  emutex_lock (&ml_service_list_mutex);
  emutex_lock (&ml_tb_target_state_mutex);
//  char **tmpx = (char **)setcombine ((const void **)plan->changes.enable, (const void **)target_state.enable, SET_TYPE_STRING);
  char **tmpx = (char **)setcombine ((const void **)tmpy, (const void **)target_state.enable, SET_TYPE_STRING);

  emutex_unlock (&ml_tb_target_state_mutex);

  char **tmp = (char **)setcombine ((const void **)tmpx, (const void **)plan->changes.disable, SET_TYPE_STRING);

  free (tmpx);
  free (tmpy);

  if (plan->changes.disable) {
   free (plan->changes.disable);
   plan->changes.disable = NULL;
  }

  if (tmp) {
   for (; tmp[i]; i++) {
    char add = 1;

    if (inset ((const void **)plan->changes.disable, (void *)tmp[i], SET_TYPE_STRING)) {
     add = 0;
    } else if ((disable_all && strmatch(tmp[i], "all")) ||
               (disable_all_but_feedback && strmatch(tmp[i], "all-but-feedback"))) {
     add = 0;
    } else if (module_logics_service_list && (cur = streefind (module_logics_service_list, tmp[i], tree_find_first))) {
     struct lmodule **lm = (struct lmodule **)cur->value;
     if (lm) {
      ssize_t y = 0;
      for (; lm[y]; y++) {
       if (disable_all_but_feedback && (lm[y]->module->mode & einit_module_feedback)) {
        add = 0;

        break;
       }
      }
     }
    } else if (!mod_isprovided (tmp[i])) {
     add = 0;
    }

    if (add) {
     plan->changes.disable = (char **)setadd((void **)plan->changes.disable, (void *)tmp[i], SET_TYPE_STRING);
    }
   }

   free (tmp);
  }

  emutex_unlock (&ml_service_list_mutex);
 }

 return plan;
}

unsigned int mod_plan_commit (struct mloadplan *plan) {
 struct einit_event *fb = evinit (einit_feedback_plan_status);

 if (!plan) return 0;

 if (plan->options & plan_option_shutdown) {
  shutting_down = 1;
 }

// do some extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;

  struct einit_event eex = evstaticinit (einit_core_mode_switching);
  eex.para = (void *)plan->mode;
  event_emit (&eex, einit_event_flag_broadcast);
  evstaticdestroy (eex);

  if ((cmdt = cfg_getstring ("before-switch/emit-event", cmode))) {
   struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);
  }

  if ((cmdt = cfg_getstring ("before-switch/ipc", cmode))) {
   char **cmdts = str2set (':', cmdt);
   uint32_t in = 0;

   if (cmdts) {
    for (; cmdts[in]; in++)
     ipc_process(cmdts[in], stderr);

    free (cmdts);
   }
  }
 }

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_START;
 fb->para = (void *)plan;
 status_update (fb);

 int currentlistrev = einit_module_logic_list_revision;

 do {
  currentlistrev = einit_module_logic_list_revision;

#ifdef DEBUG
  notice (2, "plan iteration");
#endif

  emutex_lock (&ml_tb_target_state_mutex);

  cross_taskblock (&(plan->changes), &target_state);

  emutex_lock (&ml_tb_current_mutex);

  cross_taskblock (&target_state, &current);

  uint32_t i = 0;

  if (current.enable) {
   char **tmp = NULL;
   for (i = 0; current.enable[i]; i++) {
    if (!service_usage_query (service_is_provided, NULL, current.enable[i])) {
     tmp = (char **)setadd ((void **)tmp, (void *)current.enable[i], SET_TYPE_STRING);
    }
   }
   free (current.enable);
   current.enable = tmp;
  }
  if (current.disable) {
   char **tmp = NULL;
   for (i = 0; current.disable[i]; i++) {
    if (service_usage_query (service_is_provided, NULL, current.disable[i])) {
     tmp = (char **)setadd ((void **)tmp, (void *)current.disable[i], SET_TYPE_STRING);
    }
   }
   free (current.disable);
   current.disable = tmp;
  }

  emutex_unlock (&ml_tb_current_mutex);
  emutex_unlock (&ml_tb_target_state_mutex);

  mod_commit_and_wait (plan->changes.enable, plan->changes.disable);

// repeat until the modules haven't changed halfway through:
 } while (einit_module_logic_list_revision != currentlistrev);

 fb->task = MOD_SCHEDULER_PLAN_COMMIT_FINISH;
 status_update (fb);

// do some more extra work if the plan was derived from a mode
 if (plan->mode) {
  char *cmdt;
  cmode = plan->mode;
  amode = plan->mode;

  struct einit_event eex = evstaticinit (einit_core_mode_switch_done);
  eex.para = (void *)plan->mode;
  event_emit (&eex, einit_event_flag_broadcast);
  evstaticdestroy (eex);

  if (amode->id) {
//   notice (1, "emitting feedback notice");

   struct einit_event eema = evstaticinit (einit_core_plan_update);
   eema.string = estrdup(amode->id);
   eema.para   = (void *)amode;
   event_emit (&eema, einit_event_flag_broadcast);
   free (eema.string);
   evstaticdestroy (eema);
  }

  if ((cmdt = cfg_getstring ("after-switch/ipc", amode))) {
//   notice (1, "doing ipc");

   char **cmdts = str2set (':', cmdt);
   uint32_t in = 0;

   if (cmdts) {
    for (; cmdts[in]; in++) {
     ipc_process(cmdts[in], stderr);
    }
    free (cmdts);
   }
  }

  if ((cmdt = cfg_getstring ("after-switch/emit-event", amode))) {
//   notice (1, "emitting event");
   struct einit_event ee = evstaticinit (event_string_to_code(cmdt));
   event_emit (&ee, einit_event_flag_broadcast);
   evstaticdestroy (ee);
  }
 }

 evdestroy (fb);

 module_logic_update_init_d();

 if (plan->mode) return 0; // always return "OK" if it's based on a mode

 return 0;
}

int mod_plan_free (struct mloadplan *plan) {
 if (plan) {
  if (plan->changes.enable) free (plan->changes.enable);
  if (plan->changes.disable) free (plan->changes.disable);
  if (plan->changes.critical) free (plan->changes.critical);

  if (plan->used_modes) free (plan->used_modes);

  free (plan);
 }
 return 0;
}

double mod_get_plan_progress_f (struct mloadplan *plan) {
 if (plan) {
  return 0.0;
 } else {
  double all = 0, left = 0;
  emutex_lock (&ml_tb_target_state_mutex);
  if (target_state.enable) all += setcount ((const void **)target_state.enable);
  if (target_state.disable) all += setcount ((const void **)target_state.disable);
  emutex_unlock (&ml_tb_target_state_mutex);

  emutex_lock (&ml_tb_current_mutex);
  if (current.enable) left += setcount ((const void **)current.enable);
  if (current.disable) left += setcount ((const void **)current.disable);
  emutex_unlock (&ml_tb_current_mutex);

  return 1.0 - (double)(left / all);
 }
}

void mod_sort_service_list_items_by_preference() {
 struct stree *cur;

 emutex_lock (&ml_service_list_mutex);

 cur = module_logics_service_list;

 while (cur) {
  struct lmodule **lm = (struct lmodule **)cur->value;

  if (lm) {
   /* order modules that should be enabled according to the user's preference */
   uint32_t mpx, mpy, mpz = 0;
   char *pnode = NULL, **preference = NULL;

   /* first make sure all modules marked as "deprecated" are last */
   for (mpx = 0; lm[mpx]; mpx++); mpx--;
   for (mpy = 0; mpy < mpx; mpy++) {
    if (lm[mpy]->module && (lm[mpy]->module->mode & einit_module_deprecated)) {
     struct lmodule *t = lm[mpx];
     lm[mpx] = lm[mpy];
     lm[mpy] = t;
     mpx--;
    }
   }

   /* now to the sorting bit... */
   /* step 1: sort everything using <services-prefer></services-prefer> nodes */
   pnode = emalloc (strlen (cur->key)+18);
   pnode[0] = 0;
   strcat (pnode, "services-prefer-");
   strcat (pnode, cur->key);

   if ((preference = str2set (':', cfg_getstring (pnode, NULL)))) {
    debugx ("applying module preferences for service %s", cur->key);

    for (mpx = 0; preference[mpx]; mpx++) {
     for (mpy = 0; lm[mpy]; mpy++) {
      if (lm[mpy]->module && lm[mpy]->module->rid && strmatch(lm[mpy]->module->rid, preference[mpx])) {
       struct lmodule *tm = lm[mpy];

       lm[mpy] = lm[mpz];
       lm[mpz] = tm;

       mpz++;
      }
     }
    }
    free (preference);
   }

   free (pnode);

   /* step 2: sort using the names of services specified (to get "virtual" services sorted properly) */
   mpz = 0;
   for (mpy = 0; lm[mpy]; mpy++) {
    if (lm[mpy]->si && lm[mpy]->si->provides) for (mpx = 0; lm[mpy]->si->provides[mpx]; mpx++) {
     if (mod_is_requested(lm[mpy]->si->provides[mpx]) && mod_is_unambiguous(lm[mpy]->si->provides[mpx])) {
//      notice (2, "reordering %s to %s (indirect)", cur->key, lm[mpy]->si->provides[mpx]);

      struct lmodule *tm = lm[mpy];

      lm[mpy] = lm[mpz];
      lm[mpz] = tm;

      mpz++;

      goto step2_skip;
     }
    }

    step2_skip:;
   }

   /* step 3: make sure to prefer anything that has a requested RID */
   mpz = 0;
   for (mpy = 0; lm[mpy]; mpy++) {
    if (lm[mpy]->module && lm[mpy]->module->rid && mod_is_requested_rid(lm[mpy]->module->rid)) {
     struct lmodule *tm = lm[mpy];

     lm[mpy] = lm[mpz];
     lm[mpz] = tm;

     mpz++;
    }
   }

  }

  cur = streenext(cur);
 }

 emutex_unlock (&ml_service_list_mutex);
}

int mod_switchmode (char *mode) {
 char reject;

 emutex_lock (&ml_current_switches_mutex);
 if (inset ((const void **)current_switches, mode, SET_TYPE_STRING)) {
  reject = 1;
 } else {
  reject = 0;
  current_switches = (char **)setadd ((void **)current_switches, mode, SET_TYPE_STRING);
 }
 emutex_unlock (&ml_current_switches_mutex);

 if (reject) {
  notice (1, "rejecting mode-switch to %s: already switching to that mode", mode);
  return 0;
 }

 if (!mode) return -1;
 struct cfgnode *cur = cfg_findnode (mode, einit_node_mode, NULL);
 struct mloadplan *plan = NULL;

 if (!cur) {
  notice (1, "module-logic-v3: scheduled mode \"%s\" not defined, aborting", mode);
  return -1;
 }

 plan = mod_plan (NULL, NULL, 0, cur);
 if (!plan) {
  notice (1, "module-logic-v3: scheduled mode \"%s\" defined but nothing to be done", mode);
 } else {
  pthread_t th;
  mod_plan_commit (plan);
  /* make it so that the erase operation will not disturb the flow of the program */
  ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_plan_free, (void *)plan);
 }

 emutex_lock (&ml_current_switches_mutex);
 current_switches = strsetdel (current_switches, mode);
 emutex_unlock (&ml_current_switches_mutex);

 return 0;
}

int mod_modaction (char **argv, FILE *output) {
 if (!argv) return -1;

 int argc = setcount ((const void **)argv), ret = 0;
 int32_t task = 0;
 struct mloadplan *plan;
 if (argc < 2) return -1;

 if (strmatch (argv[1], "enable") || strmatch (argv[1], "start")) task = einit_module_enable;
 else if (strmatch (argv[1], "disable") || strmatch (argv[1], "stop")) task = einit_module_disable;
 else {
  struct lmodule **tm = NULL;
  uint32_t r = 0;

  emutex_lock (&ml_service_list_mutex);
  if (module_logics_service_list) {
   struct stree *cur = streefind (module_logics_service_list, argv[0], tree_find_first);
   if (cur) {
    tm = cur->value;
   }
  }

  emutex_unlock (&ml_service_list_mutex);

  ret = 1;

  struct group_data *gd = mod_group_get_data (argv[0]);

  if (strmatch (argv[1], "zap")) {
   emutex_lock (&ml_tb_current_mutex);
   current.enable = strsetdel (current.enable, argv[0]);
   current.disable = strsetdel (current.disable, argv[0]);
   current.critical = strsetdel (current.critical, argv[0]);
   emutex_unlock (&ml_tb_current_mutex);

   emutex_lock (&ml_tb_target_state_mutex);
   target_state.enable = strsetdel (target_state.enable, argv[0]);
   target_state.disable = strsetdel (target_state.disable, argv[0]);
   target_state.critical = strsetdel (target_state.critical, argv[0]);
   emutex_unlock (&ml_tb_target_state_mutex);
  }

  if (strmatch (argv[1], "status") && output && gd) {
   char *members = set2str (' ', (const char **)gd->members);

   ret = 0;

   eprintf (output, "%s: \e[34mgroup\e[0m\n", argv[0]);
   eprintf (output, " \e[32m**\e[0m service \"%s\" is currently %s\e[0m.\n", argv[0], mod_isprovided(argv[0]) ? "\e[32mprovided" : "\e[31mnot provided");
   eprintf (output, " \e[34m>>\e[0m group type: %s\n", (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP) ? "any / any-iop" : (gd->options & (MOD_PLAN_GROUP_SEQ_MOST) ? "most" : "all")));

   if (members) {
    eprintf (output, " \e[34m>>\e[0m group members: ( %s )\n", members);
	free (members);
   }
  }

  if (tm) {
   if (strmatch (argv[1], "status") && output) {
    for (; tm[r]; r++) if (tm[r]->module) {
     if (r == 0) {
	  eprintf (output, "%s: \"%s\".\n", argv[0], tm[r]->module->name);
      if (!gd) eprintf (output, " \e[32m**\e[0m service \"%s\" is currently %s\e[0m.\n", argv[0], mod_isprovided(argv[0]) ? "\e[32mprovided" : "\e[31mnot provided");
	 } else {
	  eprintf (output, "backup candiate #%i: \"%s\".\n", r, tm[r]->module->name);
	 }

     if (tm[r]->module->rid)
	  eprintf (output, " \e[34m>>\e[0m rid: %s.\n", tm[r]->module->rid);
     if (tm[r]->source)
	  eprintf (output, " \e[34m>>\e[0m source: %s.\n", tm[r]->source);

     if (tm[r]->suspend) { eputs (" \e[34m>>\e[0m this module supports suspension.\n", output); }

     eputs (" \e[34m>>\e[0m supported functions:", output);

     if (tm[r]->enable) { eputs (" enable", output); }
     if (tm[r]->disable) { eputs (" disable", output); }
     if (tm[r]->custom) { eputs (" *", output); }

     eputs ("\n \e[34m>>\e[0m status flags: (", output);

     if (tm[r]->status & status_working) {
      ret = 2;
      eputs (" working", output);
     }

     if (tm[r]->status & status_enabled) {
      ret = 0;
      eputs (" enabled", output);
     }

     if (tm[r]->status & status_disabled) {
      eputs (" disabled", output);
     }

     if (tm[r]->status == status_idle) {
      eputs (" idle", output);
     }

     eputs (" )\n", output);
    }
   } else {
    for (; tm[r]; r++) {
     int retx = mod (einit_module_custom, tm[r], argv[1]);

     if (retx == status_ok)
      ret = 0;
    }
   }
  } else if (strmatch (argv[1], "status") && output && !gd) {
   ret = 1;

   eprintf (output, " \e[31m!!\e[0m service \"%s\" is currently not defined.\n", argv[0]);
  }

  return ret;
 }

 argv[1] = NULL;

 if ((plan = mod_plan (NULL, argv, task, NULL))) {
  pthread_t th;

  ret = mod_plan_commit (plan);

  if (task & einit_module_enable) {
   ret = !mod_isprovided (argv[0]);
  } else if (task & einit_module_disable) {
   ret = mod_isprovided (argv[0]);
  }

  ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_plan_free, (void *)plan);
 }

// free (argv[0]);
// free (argv);

 return ret;
}

void module_logic_einit_event_handler(struct einit_event *ev) {
 einit_module_logic_v3_usage++;

 if ((ev->type == einit_core_update_configuration) && !initdone) {
  initdone = 1;

  function_register("module-logic-get-plan-progress", 1, (void (*)(void *))mod_get_plan_progress_f);
 } else if (ev->type == einit_core_module_list_update) {
  /* update list with services */
  struct stree *new_service_list = NULL;
  struct lmodule *cur = ev->para;

  emutex_lock (&ml_rid_list_mutex);
  if (module_logic_rid_list) {
   streefree (module_logic_rid_list);
   module_logic_rid_list = NULL;
  }
  emutex_unlock (&ml_rid_list_mutex);

  emutex_lock (&ml_service_list_mutex);

  while (cur) {
   if (cur->module && cur->module->rid) {
    emutex_lock (&ml_rid_list_mutex);
    module_logic_rid_list = streeadd (module_logic_rid_list, cur->module->rid, cur, SET_NOALLOC, NULL);
    emutex_unlock (&ml_rid_list_mutex);

    struct lmodule **t = NULL;
    t = (struct lmodule **)setadd ((void **)t, cur, SET_NOALLOC);

    new_service_list = streeadd (new_service_list, cur->module->rid, (void *)t, SET_NOALLOC, (void *)t);
   }

   if (cur->si && cur->si->provides) {
    ssize_t i = 0;

    for (; cur->si->provides[i]; i++) {
     struct stree *slnode = new_service_list ?
       streefind (new_service_list, cur->si->provides[i], tree_find_first) :
       NULL;
     struct lnode **curval = (struct lnode **) (slnode ? slnode->value : NULL);

     curval = (struct lnode **)setadd ((void **)curval, cur, SET_NOALLOC);

     if (slnode) {
      slnode->value = curval;
      slnode->luggage = curval;
     } else {
      new_service_list = streeadd (new_service_list, cur->si->provides[i], (void *)curval, SET_NOALLOC, (void *)curval);
     }
    }
   }
   cur = cur->next;
  }

/* need to defer-free this at some point... */
//  if (module_logics_service_list) streefree (module_logics_service_list);
  emutex_lock (&ml_garbage_mutex);
  if (module_logics_service_list)
   einit_module_logic_v3_garbage.strees = (struct stree **)setadd ((void **)einit_module_logic_v3_garbage.strees, module_logics_service_list, SET_NOALLOC);
  emutex_unlock (&ml_garbage_mutex);

  module_logics_service_list = new_service_list;
  einit_module_logic_list_revision++;

  emutex_unlock (&ml_service_list_mutex);

  mod_sort_service_list_items_by_preference();

  emutex_lock (&ml_unresolved_mutex);

  if (unresolved_services) {
   free (unresolved_services);
   unresolved_services = NULL;
  }
  if (broken_services) {
   free (broken_services);
   broken_services = NULL;
  }

  emutex_unlock (&ml_unresolved_mutex);
  emutex_lock (&ml_group_data_mutex);

  if (module_logics_group_data) {
   streefree (module_logics_group_data);
   module_logics_group_data = NULL;
  }

  emutex_unlock (&ml_group_data_mutex);

  ev->chain_type = einit_core_module_list_update_complete;
 } else if ((ev->type == einit_core_service_update) && (!(ev->status & status_working))) {
/* something's done now, update our lists */
  mod_examine_module ((struct lmodule *)ev->para);
 } else switch (ev->type) {
  case einit_core_switch_mode:
   if (!ev->string) goto done;
   else {
    if (ev->output) {
     struct einit_event ee = evstaticinit(einit_feedback_register_fd);
     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     evstaticdestroy(ee);
    }

    mod_switchmode (ev->string);

    if (ev->output) {
     struct einit_event ee = evstaticinit(einit_feedback_unregister_fd);
     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     evstaticdestroy(ee);
    }
   }
   goto done;
  case einit_core_change_service_status:
   if (!ev->set || !ev->set[0] || !ev->set[1]) goto done;
   else {
    if (ev->output) {
     struct einit_event ee = evstaticinit(einit_feedback_register_fd);

     if (ev->set && ev->set[0] && ev->set[1] &&
         (strmatch (ev->set[1], "enable") || strmatch (ev->set[1], "disable") ||
          strmatch (ev->set[1], "start") || strmatch (ev->set[1], "stop"))) {
      uint32_t r = 0;
      char **senable = NULL;
      char **sdisable = NULL;

      eputs ("checking for previously requested but dropped or broken services:", ev->output);

      emutex_lock (&ml_tb_target_state_mutex);
      if (target_state.enable) {
       for (r = 0; target_state.enable[r]; r++) {
        if (!mod_isprovided (target_state.enable[r]))
         senable = (char **)setadd ((void **)senable, (void *)target_state.enable[r], SET_TYPE_STRING);
       }
      }
      if (target_state.disable) {
       for (r = 0; target_state.disable[r]; r++) {
        if (mod_isprovided (target_state.disable[r]))
         sdisable = (char **)setadd ((void **)sdisable, (void *)target_state.disable[r], SET_TYPE_STRING);
       }
      }
      emutex_unlock (&ml_tb_target_state_mutex);

      if (senable) {
       char *x = set2str (' ', (const char **)senable);

       eprintf (ev->output, "\n \e[33m** will also enable: %s\e[0m", x);
       free (senable);
       free (x);
      }
      if (sdisable) {
       char *x = set2str (' ', (const char **)sdisable);

       eprintf (ev->output, "\n \e[33m** will also disable: %s\e[0m", x);
       free (sdisable);
       free (x);
      }
      eputs ("\n \e[32m>> check complete.\e[0m\n", ev->output);
     }

     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     evstaticdestroy(ee);
    }

    ev->integer = mod_modaction ((char **)ev->set, ev->output);

    if (ev->output) {
     struct einit_event ee = evstaticinit(einit_feedback_unregister_fd);

     ee.output = ev->output;
     ee.ipc_options = ev->ipc_options;
     event_emit (&ee, einit_event_flag_broadcast);
     evstaticdestroy(ee);

     fflush (ev->output);

     if (ev->set && ev->set[0] && ev->set[1] && !strmatch(ev->set[1], "status")) {
      if (ev->integer) {
       eputs (" \e[31m!! request failed.\e[0m\n", ev->output);
      } else {
       eputs (" \e[32m>> request succeeded.\e[0m\n", ev->output);
      }
     }

     fflush (ev->output);
    }
   }
   goto done;
  default:
   goto done;
 }

 done:
 einit_module_logic_v3_usage--;
 return;
}

void module_logic_update_init_d () {
 struct cfgnode *einit_d = cfg_getnode ("core-module-logic-maintain-init.d", NULL);

#ifdef DEBUG
 notice (2, "module_logic_update_init_d(): regenerating list of services in init.d.");
#endif

 if (einit_d && einit_d->flag && einit_d->svalue) {
  char *init_d_path = cfg_getstring ("core-module-logic-init.d-path", NULL);

  if (init_d_path) {
   struct stree *cur;
   emutex_lock (&ml_service_list_mutex);
//  struct stree *module_logics_service_list;
   cur = module_logics_service_list;

   while (cur) {
    char tmp[BUFFERSIZE];
    esprintf (tmp, BUFFERSIZE, "%s/%s", init_d_path, cur->key);

    symlink (einit_d->svalue, tmp);

    cur = cur->next;
   }

   emutex_unlock (&ml_service_list_mutex);
  }
 }
}

#define STATUS2STRING(status)\
 (status == status_idle ? "idle" : \
 (status & status_working ? "working" : \
 (status & status_enabled ? "enabled" : "disabled")))

void module_logic_ipc_event_handler (struct einit_event *ev) {
 einit_module_logic_v3_usage++;

 if (ev->argv && ev->argv[0] && ev->argv[1] && ev->output) {
  if (strmatch (ev->argv[0], "update") && strmatch (ev->argv[1], "init.d")) {
   module_logic_update_init_d();

   ev->implemented = 1;
  } else if (strmatch (ev->argv[0], "examine") && strmatch (ev->argv[1], "configuration")) {
   struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);
   char **modes = NULL;

   while (cfgn) {
    if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !inset ((const void **)modes, (const void *)cfgn->mode->id, SET_TYPE_STRING))) {
     uint32_t i = 0;
     modes = (char **)setadd ((void **)modes, (void *)cfgn->mode->id, SET_TYPE_STRING);

     for (i = 0; cfgn->arbattrs[i]; i+=2) {
      if (strmatch(cfgn->arbattrs[i], "services")) {
       char **tmps = str2set (':', cfgn->arbattrs[i+1]);

       if (tmps) {
        uint32_t i = 0;

        emutex_lock(&ml_service_list_mutex);

        for (; tmps[i]; i++) {
         if (!streefind (module_logics_service_list, tmps[i], tree_find_first) && !mod_group_get_data(tmps[i])) {
          eprintf (ev->output, " * mode \"%s\": service \"%s\" referenced but not found\n", cfgn->mode->id, tmps[i]);
          ev->ipc_return++;
         }
        }

        emutex_unlock(&ml_service_list_mutex);

        free (tmps);
       }
       break;
      }
     }
    }

    cfgn = cfg_findnode ("mode-enable", 0, cfgn);
   }

   ev->implemented = 1;
  } else if (strmatch (ev->argv[0], "list")) {
   if (strmatch (ev->argv[1], "services")) {
    struct stree *modes = NULL;
    struct stree *cur = NULL;
    struct cfgnode *cfgn = cfg_findnode ("mode-enable", 0, NULL);

    while (cfgn) {
     if (cfgn->arbattrs && cfgn->mode && cfgn->mode->id && (!modes || !streefind (modes, cfgn->mode->id, tree_find_first))) {
      uint32_t i = 0;
      for (i = 0; cfgn->arbattrs[i]; i+=2) {
       if (strmatch(cfgn->arbattrs[i], "services")) {
        char **tmps = str2set (':', cfgn->arbattrs[i+1]);

        modes = streeadd (modes, cfgn->mode->id, tmps, SET_NOALLOC, tmps);

        break;
       }
      }
     }

     cfgn = cfg_findnode ("mode-enable", 0, cfgn);
    }

    emutex_lock(&ml_service_list_mutex);

    cur = module_logics_service_list;

    while (cur) {
     if (mod_is_rid (cur->key)) {
      cur = streenext (cur);
	  continue;
	 }

     char **inmodes = NULL;
     struct stree *mcur = modes;

     while (mcur) {
      if (inset ((const void **)mcur->value, (void *)cur->key, SET_TYPE_STRING)) {
       inmodes = (char **)setadd((void **)inmodes, (void *)mcur->key, SET_TYPE_STRING);
      }

      mcur = streenext(mcur);
     }

     if (inmodes) {
      char *modestr;
      if (ev->ipc_options & einit_ipc_output_xml) {
       modestr = set2str (':', (const char **)inmodes);
       eprintf (ev->output, " <service id=\"%s\" used-in=\"%s\" provided=\"%s\">\n", cur->key, modestr, mod_isprovided(cur->key) ? "yes" : "no");
      } else {
       modestr = set2str (' ', (const char **)inmodes);
       eprintf (ev->output, (ev->ipc_options & einit_ipc_output_ansi) ?
                            "\e[1mservice \"%s\" (%s)\n\e[0m" :
                            "service \"%s\" (%s)\n",
                            cur->key, modestr);
      }
      free (modestr);
      free (inmodes);
     } else if (!(ev->ipc_options & einit_ipc_only_relevant)) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       eprintf (ev->output, " <service id=\"%s\" provided=\"%s\">\n", cur->key, mod_isprovided(cur->key) ? "yes" : "no");
      } else {
       eprintf (ev->output, (ev->ipc_options & einit_ipc_output_ansi) ?
                            "\e[1mservice \"%s\" (not in any mode)\e[0m\n" :
                            "service \"%s\" (not in any mode)\n",
                            cur->key);
      }
     }

     if (inmodes || (!(ev->ipc_options & einit_ipc_only_relevant))) {
      struct group_data *gd = mod_group_get_data (cur->key);

      if (ev->ipc_options & einit_ipc_output_xml) {
       if (gd && gd->members) {
        char *members = set2str (':', (const char **)gd->members);
        char *members_escaped = escape_xml (members);
        eprintf (ev->output, "  <group members=\"%s\" seq=\"%s\" />\n",
                 members_escaped,
                 ((gd->options & MOD_PLAN_GROUP_SEQ_ANY) ? "any" :
                  ((gd->options & MOD_PLAN_GROUP_SEQ_ANY_IOP) ? "any-iop" :
                   ((gd->options & MOD_PLAN_GROUP_SEQ_MOST) ? "most" :
                   ((gd->options & MOD_PLAN_GROUP_SEQ_ALL) ? "all" : "unknown")))));

        free (members_escaped);
        free (members);
       }

       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         char *name = escape_xml (xs[u]->module && xs[u]->module->rid ? xs[u]->module->name : "unknown");
         char *rid = escape_xml (xs[u]->module && xs[u]->module->name ? xs[u]->module->rid : "unknown");

         eprintf (ev->output, "  <module id=\"%s\" name=\"%s\" status=\"%s\"",
                  rid, name, STATUS2STRING(xs[u]->status));

         free (name);
         free (rid);

         if (xs[u]->si) {
          if (xs[u]->si->provides) {
           char *x = set2str(':', (const char **)xs[u]->si->provides);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  provides=\"%s\"", y);
           free (y);
           free (x);
          }
          if (xs[u]->si->requires) {
           char *x = set2str(':', (const char **)xs[u]->si->requires);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  requires=\"%s\"", y);
           free (y);
           free (x);
          }
          if (xs[u]->si->after) {
           char *x = set2str(':', (const char **)xs[u]->si->after);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  after=\"%s\"", y);
           free (y);
           free (x);
          }
          if (xs[u]->si->before) {
           char *x = set2str(':', (const char **)xs[u]->si->before);
           char *y = escape_xml (x);
           eprintf (ev->output, "\n  before=\"%s\"", y);
           free (y);
           free (x);
          }
         }

         char **functions = (char **)setdup ((const void **)xs[u]->functions, SET_TYPE_STRING);
         if (xs[u]->enable) functions = (char **)setadd ((void **)functions, "enable", SET_TYPE_STRING);
         if (xs[u]->disable) functions = (char **)setadd ((void **)functions, "disable", SET_TYPE_STRING);
         functions = (char **)setadd ((void **)functions, "zap", SET_TYPE_STRING);

         if (functions) {
          char *x = set2str(':', (const char **)functions);
          char *y = escape_xml (x);
          eprintf (ev->output, "\n  functions=\"%s\"", y);
          free (y);
          free (x);

          free (functions);
         }

         eputs (" />\n", ev->output);
        }
       }

       eputs (" </service>\n", ev->output);
      } else {
       if (cur->value) {
        struct lmodule **xs = cur->value;
        uint32_t u = 0;
        for (u = 0; xs[u]; u++) {
         eprintf (ev->output, (ev->ipc_options & einit_ipc_output_ansi) ?
           ((xs[u]->module && (xs[u]->module->mode & einit_module_deprecated)) ?
                                  " \e[31m- \e[0mcandidate \"%s\" (%s)\n" :
                                  " \e[33m* \e[0mcandidate \"%s\" (%s)\n") :
             " * candidate \"%s\" (%s)\n",
           xs[u]->module && xs[u]->module->rid ? xs[u]->module->rid : "unknown",
           xs[u]->module && xs[u]->module->name ? xs[u]->module->name : "unknown");
        }
       }
      }
     }

     cur = streenext (cur);
    }

    emutex_unlock(&ml_service_list_mutex);

    emutex_lock (&ml_group_data_mutex);

/* eputs ("got mutex", stderr);
    fflush (stderr);*/

    cur = module_logics_group_data;
    while (cur) {
     struct group_data *gd = (struct group_data *)cur->value;

     emutex_lock(&ml_service_list_mutex);
     if (streefind (module_logics_service_list, cur->key, tree_find_first)) { // skip entries we already displayed
      emutex_unlock(&ml_service_list_mutex);
      cur = streenext (cur);
      continue;
     }

     emutex_unlock(&ml_service_list_mutex);

     char **inmodes = NULL;
     struct stree *mcur = modes;

     while (mcur) {
      if (inset ((const void **)mcur->value, (void *)cur->key, SET_TYPE_STRING)) {
       inmodes = (char **)setadd((void **)inmodes, (void *)mcur->key, SET_TYPE_STRING);
      }

      mcur = streenext(mcur);
     }

     if (inmodes) {
      char *modestr;
      if (ev->ipc_options & einit_ipc_output_xml) {
       modestr = set2str (':', (const char **)inmodes);
       eprintf (ev->output, " <service id=\"%s\" used-in=\"%s\" provided=\"%s\">\n", cur->key, modestr, mod_isprovided(cur->key) ? "yes" : "no");
      } else {
       modestr = set2str (' ', (const char **)inmodes);
       eprintf (ev->output, (ev->ipc_options & einit_ipc_output_ansi) ?
                            "\e[1mservice \"%s\" (%s)\n\e[0m" :
                              "service \"%s\" (%s)\n",
                            cur->key, modestr);
      }
      free (modestr);
      free (inmodes);
     } else if (!(ev->ipc_options & einit_ipc_only_relevant)) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       eprintf (ev->output, " <service id=\"%s\" provided=\"%s\">\n", cur->key, mod_isprovided(cur->key) ? "yes" : "no");
      } else {
       eprintf (ev->output, (ev->ipc_options & einit_ipc_output_ansi) ?
                            "\e[1mservice \"%s\" (not in any mode)\e[0m\n" :
                              "service \"%s\" (not in any mode)\n",
                            cur->key);
      }
     }

     if (inmodes || (!(ev->ipc_options & einit_ipc_only_relevant))) {
      if (ev->ipc_options & einit_ipc_output_xml) {
       if (gd && gd->members) {
        char *members = set2str (':', (const char **)gd->members);
        char *members_escaped = escape_xml (members);
        eprintf (ev->output, "  <group members=\"%s\" seq=\"%s\" />\n",
                 members_escaped,
                 ((gd->options & MOD_PLAN_GROUP_SEQ_ANY) ? "any" :
                  ((gd->options & MOD_PLAN_GROUP_SEQ_ANY_IOP) ? "any-iop" :
                   ((gd->options & MOD_PLAN_GROUP_SEQ_MOST) ? "most" :
                   ((gd->options & MOD_PLAN_GROUP_SEQ_ALL) ? "all" : "unknown")))));

        free (members_escaped);
        free (members);
       }

       eputs (" </service>\n", ev->output);
      }
     }

     cur = streenext (cur);
    }

    emutex_unlock (&ml_group_data_mutex);

    ev->implemented = 1;
   }


#ifdef DEBUG
   else if (strmatch (ev->argv[1], "control-blocks")) {
    emutex_lock (&ml_tb_target_state_mutex);

    if (target_state.enable) {
     char *r = set2str (' ', (const char **)target_state.enable);
     if (r) {
      eprintf (ev->output, "target_state.enable = { %s }\n", r);
      free (r);
     }
    }
    if (target_state.disable) {
     char *r = set2str (' ', (const char **)target_state.disable);
     if (r) {
      eprintf (ev->output, "target_state.disable = { %s }\n", r);
      free (r);
     }
    }
    if (target_state.critical) {
     char *r = set2str (' ', (const char **)target_state.critical);
     if (r) {
      eprintf (ev->output, "target_state.critical = { %s }\n", r);
      free (r);
     }
    }

    emutex_unlock (&ml_tb_target_state_mutex);
    emutex_lock (&ml_tb_current_mutex);

    if (current.enable) {
     char *r = set2str (' ', (const char **)current.enable);
     if (r) {
      eprintf (ev->output, "current.enable = { %s }\n", r);
      free (r);
     }
    }
    if (current.disable) {
     char *r = set2str (' ', (const char **)current.disable);
     if (r) {
      eprintf (ev->output, "current.disable = { %s }\n", r);
      free (r);
     }
    }
    if (current.critical) {
     char *r = set2str (' ', (const char **)current.critical);
     if (r) {
      eprintf (ev->output, "current.critical = { %s }\n", r);
      free (r);
     }
    }

    emutex_unlock (&ml_tb_current_mutex);

    ev->implemented = 1;
   }
#endif
  }
 }

 einit_module_logic_v3_usage--;
}

/* end common functions */

/* start new functions */

int mod_gettask (char * service);

pthread_mutex_t
 ml_examine_mutex = PTHREAD_MUTEX_INITIALIZER,
 ml_chain_examine = PTHREAD_MUTEX_INITIALIZER,
 ml_workthreads_mutex = PTHREAD_MUTEX_INITIALIZER,
 ml_commits_mutex = PTHREAD_MUTEX_INITIALIZER,
 ml_changed_mutex = PTHREAD_MUTEX_INITIALIZER;

struct stree *module_logics_chain_examine = NULL; // value is a (char **)
struct stree *module_logics_chain_examine_reverse = NULL;
char **currently_provided = NULL;
char **changed_recently = NULL;
signed char mod_flatten_current_tb_group(char *serv, char task);
void mod_spawn_workthreads ();
char mod_haschanged(char *service);

uint32_t ml_workthreads = 0;
uint32_t ml_commits = 0;
int32_t ignorereorderfor = 0;

char **lm_workthreads_list = NULL;

#ifdef DEBUG
void print_defer_lists() {
 emutex_lock(&ml_chain_examine);

 if (module_logics_chain_examine_reverse) {
  struct stree *st = module_logics_chain_examine_reverse;

  eputs ("module_logics_chain_examine_reverse:\n", debugfile);
  
  do {
   char *val = set2str (' ', st->value);
   eprintf (debugfile, "%s: (%s)\n", st->key, val);
   free (val);

   st = streenext (st);
  } while (st);
 } else {
  eputs ("module_logics_chain_examine_reverse is empty.\n", debugfile);
 }

 if (module_logics_chain_examine) {
  struct stree *st = module_logics_chain_examine;

  eputs ("module_logics_chain_examine:\n", debugfile);
  
  do {
   char *val = set2str (' ', st->value);
   eprintf (debugfile, "%s: (%s)\n", st->key, val);
   free (val);

   st = streenext (st);
  } while (st);
 } else {
  eputs ("module_logics_chain_examine is empty.\n", debugfile);
 }

 emutex_unlock(&ml_chain_examine);

 fflush (debugfile);
}
#endif

char mod_workthreads_dec (char *service) {
#ifdef DEBUG
 notice (1, "\ndone with: %s\n", service);
#endif

// char **donext = NULL;
 uint32_t i = 0;

#ifdef DEBUG
 print_defer_lists();
#endif

 emutex_lock (&ml_workthreads_mutex);

 lm_workthreads_list = strsetdel (lm_workthreads_list, service);

 ml_workthreads--;

 emutex_unlock (&ml_workthreads_mutex);

 emutex_lock (&ml_workthreads_mutex);

// eprintf (stderr, "%s: workthreads: %i (%s)\n", service, ml_workthreads, set2str (' ', lm_workthreads_list));
// fflush (stderr);

 if (!ml_workthreads) {
  char spawn = 0;
  emutex_unlock (&ml_workthreads_mutex);

  einit_module_logic_v3_free_garbage();

  emutex_lock (&ml_tb_current_mutex);
  if (current.enable) {
   for (i = 0; current.enable[i]; i++) {
    if (!mod_isprovided (current.enable[i]) && !mod_isbroken(current.enable[i])) {
     spawn = 1;
     break;
    }
   }
  }
  if (!spawn && current.disable) {
   for (i = 0; current.disable[i]; i++) {
    if (mod_isprovided (current.disable[i]) && !mod_isbroken(current.disable[i])) {
     spawn = 1;
     break;
    }
   }
  }
  emutex_unlock (&ml_tb_current_mutex);

  if (spawn)
   mod_spawn_workthreads ();

 } else {
  emutex_unlock (&ml_workthreads_mutex);
 }

#if 1
 mod_ping_all_threads();
#endif

 einit_module_logic_v3_usage--;

 return 0;
}

char mod_workthreads_dec_changed (char *service) {
 emutex_lock (&ml_changed_mutex);
 if (!inset ((const void **)changed_recently, (const void *)service, SET_TYPE_STRING))
  changed_recently = (char **)setadd ((void **)changed_recently, (const void *)service, SET_TYPE_STRING);
 emutex_unlock (&ml_changed_mutex);

 return mod_workthreads_dec (service);
}

char mod_workthreads_inc (char *service) {
 char retval = 0;
 emutex_lock (&ml_workthreads_mutex);

 if (inset ((const void **)lm_workthreads_list, (void *)service, SET_TYPE_STRING)) {
//  eprintf (stderr, " XX someone's already working on %s...\n", service);
//  fflush (stderr);
  retval = 1;
 } else {
  lm_workthreads_list = (char **)setadd((void **)lm_workthreads_list, (void *)service, SET_TYPE_STRING);

  ml_workthreads++;
  einit_module_logic_v3_usage++;
 }
 emutex_unlock (&ml_workthreads_mutex);

 if (retval) mod_ping_all_threads ();

 return retval;
}

char mod_have_workthread (char *service) {
 char retval = 0;
 emutex_lock (&ml_workthreads_mutex);

 if (inset ((const void **)lm_workthreads_list, (void *)service, SET_TYPE_STRING)) {
  retval = 1;
 }
 emutex_unlock (&ml_workthreads_mutex);

 return retval;
}

void mod_commits_dec () {
#ifdef DEBUG
 notice (5, "plan finished.");
#endif

 char clean_broken = 0, **unresolved = NULL, **broken = NULL, suspend_all = 0;
 emutex_lock (&ml_unresolved_mutex);
 if (broken_services) {
  broken = (char **)setdup ((const void **)broken_services, SET_TYPE_STRING);
 }
 if (unresolved_services) {
  unresolved = (char **)setdup ((const void **)unresolved_services, SET_TYPE_STRING);
 }
 emutex_unlock (&ml_unresolved_mutex);

 if (broken) {
  struct einit_event ee = evstaticinit(einit_feedback_broken_services);
  ee.set = (void **)broken;
  ee.stringset = broken;

  event_emit (&ee, einit_event_flag_broadcast);
  evstaticdestroy (ee);

  free (broken);
 }
 if (unresolved) {
  struct einit_event ee = evstaticinit(einit_feedback_unresolved_services);
  ee.set = (void **)unresolved;
  ee.stringset = unresolved;

  event_emit (&ee, einit_event_flag_broadcast);
  evstaticdestroy (ee);

  free (unresolved);
 }

 emutex_lock (&ml_commits_mutex);
 ml_commits--;
 clean_broken = (ml_commits <= 0);
 suspend_all = (ml_commits <= 0);
 emutex_unlock (&ml_commits_mutex);

 if (clean_broken) {
  emutex_lock (&ml_unresolved_mutex);
  if (unresolved_services) {
   free (unresolved_services);
   unresolved_services = NULL;
  }
  if (broken_services) {
   free (broken_services);
   broken_services = NULL;
  }
  emutex_unlock (&ml_unresolved_mutex);

  emutex_lock (&ml_changed_mutex);
  if (changed_recently) {
   free (changed_recently);
   changed_recently = NULL;
  }
  emutex_unlock (&ml_changed_mutex);

  emutex_lock(&ml_chain_examine);
  if (module_logics_chain_examine) {
   streefree (module_logics_chain_examine);
   module_logics_chain_examine = NULL;
  }
  if (module_logics_chain_examine_reverse) {
   streefree (module_logics_chain_examine_reverse);
   module_logics_chain_examine_reverse = NULL;
  }
  emutex_unlock(&ml_chain_examine);
 }

 if (suspend_all) {
  einit_module_logic_v3_free_garbage();

  notice (3, "suspending and unloading unused modules...");

  struct einit_event eml = evstaticinit(einit_core_suspend_all);
  event_emit (&eml, einit_event_flag_broadcast | einit_event_flag_spawn_thread);
  evstaticdestroy(eml);
 }

#if 0
 mod_ping_all_threads();
#endif
}

void mod_commits_inc () {
 char clean_broken = 0;

 modules_last_change = time (NULL);

// char spawn = 0;
#ifdef DEBUG
 notice (5, "plan started.");
#endif

 emutex_lock (&ml_commits_mutex);
 clean_broken = (ml_commits <= 0);
 ml_commits++;
 emutex_unlock (&ml_commits_mutex);

 if (clean_broken) {
  emutex_lock (&ml_unresolved_mutex);
  if (unresolved_services) {
   free (unresolved_services);
   unresolved_services = NULL;
  }
  if (broken_services) {
   free (broken_services);
   broken_services = NULL;
  }
  emutex_unlock (&ml_unresolved_mutex);

  emutex_lock (&ml_changed_mutex);
  if (changed_recently) {
   free (changed_recently);
   changed_recently = NULL;
  }
  emutex_unlock (&ml_changed_mutex);

  emutex_lock(&ml_chain_examine);
  if (module_logics_chain_examine) {
   streefree (module_logics_chain_examine);
   module_logics_chain_examine = NULL;
  }
  if (module_logics_chain_examine_reverse) {
   streefree (module_logics_chain_examine_reverse);
   module_logics_chain_examine_reverse = NULL;
  }
  emutex_unlock(&ml_chain_examine);
 }

 mod_spawn_workthreads ();
}

void mod_defer_notice (struct lmodule *mod, char **services) {
 char tmp[BUFFERSIZE];
 char *s = set2str (' ', (const char **)services);

 struct einit_event ee = evstaticinit (einit_feedback_module_status);
 mod->status |= status_deferred;

 ee.module = mod;
 ee.status = status_deferred;

 if (s) {
  esprintf (tmp, BUFFERSIZE, "queued after: %s", s);
  ee.string = estrdup(tmp);
 }

 event_emit (&ee, einit_event_flag_broadcast);
 evstaticdestroy (ee);

 if (s) free (s);
}

char mod_check_circular_defer_rec (char *service, char *after, int depth) {
 char ret = 0;
 char **deferrees = NULL;

 if (strmatch (after, service)) {
  return 1;
 }

 if (depth > 200) {
  return 1;
 }

 emutex_lock(&ml_chain_examine);

 struct stree *r =
   streefind (module_logics_chain_examine_reverse, after, tree_find_first);

 if (r) {
  deferrees = (char **)setdup ((const void **)r->value, SET_TYPE_STRING);
 }

 emutex_unlock(&ml_chain_examine);

 if (deferrees) {
  uint32_t i = 0;

  for (; deferrees[i]; i++) {
   if (strmatch (deferrees[i], service)) {
    ret = 1;
    break;
   } else if (mod_check_circular_defer_rec (service, deferrees[i], depth + 1)) {
    ret = 1;
    break;
   }
  }

  free (deferrees);
 }

 return ret;
}

#define mod_check_circular_defer(a,b) mod_check_circular_defer_rec (a, b, 0)

char mod_defer_until (char *service, char *after) {
 struct stree *xn = NULL;

 if (mod_check_circular_defer (service, after)) {
  notice (2, "Circular Dependency detected, not deferring %s after %s", service, after);

  return 1;
 }
#ifdef DEBUG
 eprintf (debugfile, "\n ** deferring %s until after %s\n", service, after);
#endif

 emutex_lock(&ml_chain_examine);

 if ((xn = streefind (module_logics_chain_examine, after, tree_find_first))) {
  if (!inset ((const void **)xn->value, service, SET_TYPE_STRING)) {
   char **n = (char **)setadd ((void **)xn->value, service, SET_TYPE_STRING);

   xn->value = (void *)n;
   xn->luggage = (void *)n;
  }
 } else {
  char **n = (char **)setadd ((void **)NULL, service, SET_TYPE_STRING);

  module_logics_chain_examine =
   streeadd(module_logics_chain_examine, after, n, SET_NOALLOC, n);
 }

 if ((xn = streefind (module_logics_chain_examine_reverse, service, tree_find_first))) {
  if (!inset ((const void **)xn->value, after, SET_TYPE_STRING)) {
   char **n = (char **)setadd ((void **)xn->value, after, SET_TYPE_STRING);

   xn->value = (void *)n;
   xn->luggage = (void *)n;
  }
 } else {
  char **n = (char **)setadd ((void **)NULL, after, SET_TYPE_STRING);

  module_logics_chain_examine_reverse =
   streeadd(module_logics_chain_examine_reverse, service, n, SET_NOALLOC, n);
 }

 emutex_unlock(&ml_chain_examine);

#ifdef DEBUG
 print_defer_lists();
#endif

#if 0
 mod_ping_all_threads();
#endif
 return 0;
}

void mod_remove_defer (char *service) {
 struct stree *xn = NULL;

#ifdef DEBUG
 eprintf (debugfile, "\n ** removing deferred-status from %s\n", service);
#endif

 emutex_lock(&ml_chain_examine);

 if ((xn = streefind (module_logics_chain_examine_reverse, service, tree_find_first))) {
  uint32_t i = 0;

  if (xn->value) {
   for (; ((char **)xn->value)[i]; i++) {
    struct stree *yn = streefind (module_logics_chain_examine, ((char **)xn->value)[i], tree_find_first);

    if (yn) {
     yn->value = (void *)strsetdel ((char **)yn->value, ((char **)xn->value)[i]);

     if (!yn->value) {
      module_logics_chain_examine = streedel (yn);
     } else {
      yn->luggage = yn->value;
     }
    }
   }
  }

  module_logics_chain_examine_reverse = streedel (xn);
 }

 emutex_unlock(&ml_chain_examine);

#ifdef DEBUG
 print_defer_lists();
#endif
}

void mod_decrease_deferred_by (char *service) {
 struct stree *xn = NULL;
 char **do_examine = NULL;

 emutex_lock(&ml_chain_examine);

 if ((xn = streefind (module_logics_chain_examine, service, tree_find_first))) {
  uint32_t i = 0;

  if (xn->value) {
   for (; ((char **)xn->value)[i]; i++) {
    struct stree *yn = streefind (module_logics_chain_examine_reverse, ((char **)xn->value)[i], tree_find_first);

    if (yn) {
     yn->value = (void *)strsetdel ((char **)yn->value, ((char **)xn->value)[i]);
     yn->luggage = yn->value;

     if (!yn->value) {
      do_examine = (char **)setadd ((void **)do_examine, yn->key, SET_TYPE_STRING);

      module_logics_chain_examine_reverse = streedel (yn);
     }
    }
   }
  }

  module_logics_chain_examine = streedel (xn);
 }

 emutex_unlock(&ml_chain_examine);

/* if (do_examine) {
  uint32_t i = 0;

  for (; do_examine[i]; i++) {
   pthread_t th;
   ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))workthread_examine, estrdup (do_examine[i]));
//   mod_examine (do_examine[i]);
  }
  free (do_examine);
 }*/
}

char mod_isdeferred (char *service) {
 char ret = 0;
 char **deferrees = NULL;

 emutex_lock(&ml_chain_examine);

 struct stree *r =
  streefind (module_logics_chain_examine_reverse, service, tree_find_first);

 if (r) {
  deferrees = (char **)setdup ((const void **)r->value, SET_TYPE_STRING);
 }

 emutex_unlock(&ml_chain_examine);

 if (deferrees) {
  uint32_t i = 0;

  for (; deferrees[i]; i++) {
   if (!mod_haschanged(deferrees[i]) && !mod_isbroken(deferrees[i])) {
    ret++;
#ifdef DEBUG
    notice (1, " -- %s: deferred by %s\n", service, deferrees[i]);
#endif
   }
#ifdef DEBUG
   else {
    notice (1, " -- %s: invalid defer: %s\n", service, deferrees[i]);
   }
#endif

   mod_workthread_create (deferrees[i]);
  }

  free (deferrees);
 }

// ret = (ret > 0);

#ifdef DEBUG
 if (ret) {
  notice (1, "service %s is deferred! (%i)", service, ret);
 } else {
  notice (1, "service %s is NOT deferred! (%i)", service, ret);
 }
#endif

 return ret;
}

char mod_mark (char *service, char task) {
 char retval = 0;

 emutex_lock (&ml_unresolved_mutex);

 if ((task & MARK_BROKEN) && !inset ((const void **)broken_services, (void *)service, SET_TYPE_STRING)) {
  broken_services = (char **)setadd ((void **)broken_services, (void *)service, SET_TYPE_STRING);
 }
 if ((task & MARK_UNRESOLVED) && !inset ((const void **)unresolved_services, (void *)service, SET_TYPE_STRING)) {
  unresolved_services = (char **)setadd ((void **)unresolved_services, (void *)service, SET_TYPE_STRING);
 }

 emutex_unlock (&ml_unresolved_mutex);

 mod_remove_defer (service);

 mod_ping_all_threads();

 return retval;
}

char mod_isprovided(char *service) {
/* char ret = 0;

 emutex_lock (&ml_currently_provided_mutex);

 ret = inset ((const void **)currently_provided, (const void *)service, SET_TYPE_STRING);

 emutex_unlock (&ml_currently_provided_mutex);*/

 if (!service) return 0;

 if (service_usage_query (service_is_provided, NULL, service)) return 1;

 struct lmodule *lm;

 if ((lm = mod_find_by_rid (service))) {
  if (lm->status & status_enabled) return 1;
 }

 return 0;
}

char mod_haschanged(char *service) {
 char ret = 0;

 emutex_lock (&ml_changed_mutex);

 ret = inset ((const void **)changed_recently, (const void *)service, SET_TYPE_STRING);

 emutex_unlock (&ml_changed_mutex);

// if (!ret) ret = mod_isbroken (service);

 return ret;
}

void mod_queue_enable (char *service) {
 emutex_lock (&ml_tb_current_mutex);

 current.enable = (char **)setadd ((void **)current.enable, (const void *)service, SET_TYPE_STRING);
 current.disable = strsetdel ((char **)current.disable, (void *)service);

 emutex_unlock (&ml_tb_current_mutex);

 mod_workthread_create (service);
}

void mod_queue_disable (char *service) {
 emutex_lock (&ml_tb_current_mutex);

 current.disable = (char **)setadd ((void **)current.disable, (const void *)service, SET_TYPE_STRING);
 current.enable = strsetdel ((char **)current.enable, (void *)service);

 emutex_unlock (&ml_tb_current_mutex);

 mod_workthread_create (service);
}

signed char mod_flatten_current_tb_group(char *serv, char task) {
 struct group_data *gd = mod_group_get_data (serv);

#ifdef DEBUG
 eputs ("g", stderr);
 fflush (stderr);
#endif

 if (gd) {
  uint32_t changes = 0;
  char *service = estrdup (serv);

  if (!gd->members || !gd->members[0])
   return -1;

  if (gd->options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
   uint32_t i = 0;

   for (; gd->members[i]; i++) {
    if (((task & einit_module_enable) && mod_isprovided (gd->members[i])) ||
          ((task & einit_module_disable) && !mod_isprovided (gd->members[i]))) {
     free (service);
     return 0;
    }

    if (mod_isbroken (gd->members[i])) {
     continue;
    }

    if (mod_defer_until(service, gd->members[i])) {
     mod_mark (gd->members[i], MARK_BROKEN);

     continue;
    }

    if (!inset ((const void **)(task & einit_module_enable ? current.enable : current.disable), gd->members[i], SET_TYPE_STRING)) {
     changes++;

     if (task & einit_module_enable) {
      current.enable = (char **)setadd ((void **)current.enable, (const void *)gd->members[i], SET_TYPE_STRING);
     } else {
      current.disable = (char **)setadd ((void **)current.disable, (const void *)gd->members[i], SET_TYPE_STRING);
     }

     free (service);
     return 1;
    }

    free (service);
    return 0;
   }

   notice (2, "marking group %s broken (...)", service);

   mod_mark (service, MARK_BROKEN);
  } else { // MOD_PLAN_GROUP_SEQ_ALL | MOD_PLAN_GROUP_SEQ_MOST
   uint32_t i = 0, bc = 0, sc = 0;

   for (; gd->members[i]; i++) {
    if (((task & einit_module_enable) && mod_isprovided (gd->members[i])) ||
        ((task & einit_module_disable) && !mod_isprovided (gd->members[i]))) {
#ifdef DEBUG
     eprintf (stderr, "%s: skipping %s (already in proper state)\n", service, gd->members[i]);
#endif

     sc++;
     continue;
    }

    if (mod_isbroken (gd->members[i])) {
#ifdef DEBUG
     eprintf (stderr, "%s: skipping %s (broken)\n", service, gd->members[i]);
#endif

     bc++;
     continue;
    }

    if (mod_defer_until(service, gd->members[i])) {
     mod_mark (gd->members[i], MARK_BROKEN);

     bc++;
     continue;
    }

    if (!inset ((const void **)(task & einit_module_enable ? current.enable : current.disable), gd->members[i], SET_TYPE_STRING)) {
     changes++;

#ifdef DEBUG
     eprintf (stderr, "%s: deferring after %s\n", service, gd->members[i]);
#endif

     if (task & einit_module_enable) {
      current.enable = (char **)setadd ((void **)current.enable, (const void *)gd->members[i], SET_TYPE_STRING);
     } else {
      current.disable = (char **)setadd ((void **)current.disable, (const void *)gd->members[i], SET_TYPE_STRING);
     }
    }
   }

#ifdef DEBUG
   notice (6, "group %s: i=%i; sc=%i; bc=%i\n", service, i, sc, bc);
#endif

   if (bc) {
    if (bc == i) {
     notice (5, "group %s broken!\n", service);

     mod_mark (service, MARK_BROKEN);
    } else if (gd->options & MOD_PLAN_GROUP_SEQ_ALL) {
     notice (5, "group %s broken!\n", service);

     mod_mark (service, MARK_BROKEN);
    }
   }
  }

  free (service);
  return changes != 0;
 }

 return -1;
}

signed char mod_flatten_current_tb_module(char *serv, char task) {
 emutex_lock (&ml_service_list_mutex);
 struct stree *xn = streefind (module_logics_service_list, serv, tree_find_first);

#ifdef DEBUG
 eputs ("m", stderr);
 fflush (stderr);
#endif

 if (xn && xn->value) {
  struct lmodule **lm = xn->value;
  uint32_t changes = 0;
  char *service = estrdup (serv);

  if (task & einit_module_enable) {
   struct lmodule *first = lm[0];
   char broken = 0, rotate = 0;

   do {
/*    eputs (".", stderr);
    fflush (stderr);*/
    struct lmodule *rcurrent = lm[0];

    rotate = 0;
    broken = 0;

    first = 0;
    if (rcurrent && rcurrent->si && rcurrent->si->requires) {
     uint32_t i = 0;

     for (; rcurrent->si->requires[i]; i++) {
      if (mod_isprovided (rcurrent->si->requires[i])) {
       continue;
      }

      if (mod_isbroken (rcurrent->si->requires[i])) {
       rotate = 1;
       broken = 1;

       break;
      }

      if (!inset ((const void **)current.enable, (const void *)rcurrent->si->requires[i], SET_TYPE_STRING)) {
       current.enable = (char **)setadd ((void **)current.enable, (const void *)rcurrent->si->requires[i], SET_TYPE_STRING);

       changes++;
      }

      broken = 0;
     }
    }

    if (rotate && lm[1]) {
     ssize_t rx = 1;

     for (; lm[rx]; rx++) {
      lm[rx-1] = lm[rx];
     }

     lm[rx-1] = rcurrent;
    } else {
     rotate = 0;
    }
   } while (broken && rotate && (lm[0] != first));

   if (broken) {
    notice (2, "marking module %s broken (broken != 0)", service);

    mod_mark (service, MARK_BROKEN);
   }
  } else { /* disable... */
   uint32_t z = 0;

   for (; lm[z]; z++) {
    char **t = service_usage_query_cr (service_get_services_that_use, lm[z], NULL);

    if (t) {
     uint32_t i = 0;

     for (; t[i]; i++) {
      if (!mod_isprovided (t[i]) || mod_isbroken (t[i])) {
       continue;
      }

      if (!inset ((const void **)current.disable, (const void *)t[i], SET_TYPE_STRING)) {
       current.disable = (char **)setadd ((void **)current.disable, (const void *)t[i], SET_TYPE_STRING);

       changes++;
      }
     }
    }
   }
  }

  emutex_unlock (&ml_service_list_mutex);

  free (service);

  return changes != 0;
 }

 emutex_unlock (&ml_service_list_mutex);

 return -1;
}

void mod_flatten_current_tb () {
 emutex_lock (&ml_tb_current_mutex);

 repeat_ena:
 if (current.enable) {
  uint32_t i;

#ifdef DEBUG
  eputs ("e", stderr);
  fflush (stderr);
#endif

  for (i = 0; current.enable[i]; i++) {
   signed char t = 0;
   if (mod_isprovided (current.enable[i]) || mod_isbroken(current.enable[i])) {
    current.enable = (char **)setdel ((void **)current.enable, current.enable[i]);
    goto repeat_ena;
   }

#ifdef DEBUG
   eputs ("+", stderr);
/*   eputs (current.enable[i], stderr);*/
   fflush (stderr);
#endif

   if (((t = mod_flatten_current_tb_group(current.enable[i], einit_module_enable)) == -1) &&
       ((t = mod_flatten_current_tb_module(current.enable[i], einit_module_enable)) == -1)) {
    notice (2, "can't resolve service %s\n", current.enable[i]);

    mod_mark (current.enable[i], MARK_UNRESOLVED);
   } else {
    if (t) {
     goto repeat_ena;
    }
   }

#ifdef DEBUG
   eputs ("-", stderr);
   fflush (stderr);
#endif
  }

#if 0
  for (i = 0; current.enable[i]; i++) {
   struct stree *xn = streefind (module_logics_service_list, current.enable[i], tree_find_first);

   if (!mod_group_get_data (current.enable[i]) && xn && xn->value) {
    struct lmodule **lm = xn->value;
//    mod_defer_notice (lm[0], NULL);
   }
  }
#endif
 }

 repeat_disa:
 if (current.disable) {
  uint32_t i;

#ifdef DEBUG
  eputs ("d", stderr);
  fflush (stderr);
#endif

  for (i = 0; current.disable[i]; i++) {
   signed char t = 0;
   if (!mod_isprovided (current.disable[i]) || mod_isbroken(current.disable[i])) {
    current.disable = (char **)setdel ((void **)current.disable, current.disable[i]);
    goto repeat_disa;
   }

#ifdef DEBUG
   eputs ("z", stderr);
   fflush (stderr);
#endif

   if (((t = mod_flatten_current_tb_group(current.disable[i], einit_module_disable)) == -1) &&
       ((t = mod_flatten_current_tb_module(current.disable[i], einit_module_disable)) == -1)) {
    notice (2, "can't resolve service %s\n", current.disable[i]);

    mod_mark (current.disable[i], MARK_UNRESOLVED);
   } else {
    if (t) {
     goto repeat_disa;
    }
   }

#ifdef DEBUG
   eputs ("!", stderr);
   fflush (stderr);
#endif
  }

/*  for (i = 0; current.disable[i]; i++) {
   struct stree *xn = streefind (module_logics_service_list, current.disable[i], tree_find_first);

   if (!mod_group_get_data (current.disable[i]) && xn && xn->value) {
    struct lmodule **lm = xn->value;
//    mod_defer_notice (lm[0], NULL);
   }
  }*/
 }

#ifdef DEBUG
 eputs ("R", stderr);
 fflush (stderr);
#endif

 emutex_unlock (&ml_tb_current_mutex);
}

void mod_examine_module (struct lmodule *module) {
 if (!(module->status & status_working)) {
  if (module->si && module->si->provides) {
   uint32_t i = 0;

   if (module->status & status_enabled) {
//    eprintf (stderr, " ** service enabled, module=%s...\n", module->module->rid);

    emutex_lock (&ml_tb_current_mutex);
    current.enable = (char **)setslice_nc ((void **)current.enable, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_tb_current_mutex);

    emutex_lock (&ml_currently_provided_mutex);
    currently_provided = (char **)setcombine_nc ((void **)currently_provided, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_currently_provided_mutex);

    emutex_lock (&ml_changed_mutex);
    changed_recently = (char **)setcombine_nc ((void **)changed_recently, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_changed_mutex);

    mod_ping_all_threads();
   } else if ((module->status & status_disabled) || (module->status == status_idle)) {
//    eputs ("service disabled...\n", stderr);

    emutex_lock (&ml_tb_current_mutex);
    current.disable = (char **)setslice_nc ((void **)current.disable, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_tb_current_mutex);

    emutex_lock (&ml_currently_provided_mutex);
    currently_provided = (char **)setslice_nc ((void **)currently_provided, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_currently_provided_mutex);

    emutex_lock (&ml_changed_mutex);
    changed_recently = (char **)setcombine_nc ((void **)changed_recently, (const void **)module->si->provides, SET_TYPE_STRING);
    emutex_unlock (&ml_changed_mutex);

    mod_ping_all_threads();
   }

   for (; module->si->provides[i]; i++) {
    mod_post_examine (module->si->provides[i]);
   }
  }
 }

#if 0
 mod_ping_all_threads();
#endif
}

void mod_post_examine (char *service) {
 char **pex = NULL;
 struct stree *post_examine;

 emutex_lock (&ml_chain_examine);

 if ((post_examine = streefind (module_logics_chain_examine, service, tree_find_first))) {
  pex = (char **)setdup ((const void **)post_examine->value, SET_TYPE_STRING);
 }

 emutex_unlock (&ml_chain_examine);

 mod_decrease_deferred_by (service);

#ifdef DEBUG
 notice (1, "post-examining service %s: (%s)", service, pex ? set2str (' ', pex) : "none");
#endif

 if (pex) {
  uint32_t j = 0;

  for (; pex[j]; j++) {
   mod_workthread_create (pex[j]);
  }

  free (pex);
 }
}

void mod_pre_examine (char *service) {
 char **pex = NULL;
 struct stree *post_examine;

 emutex_lock (&ml_chain_examine);

 if ((post_examine = streefind (module_logics_chain_examine_reverse, service, tree_find_first))) {
  pex = (char **)setdup ((const void **)post_examine->value, SET_TYPE_STRING);
 }

 emutex_unlock (&ml_chain_examine);

 if (pex) {
  uint32_t j = 0, broken = 0, done = 0;

  for (; pex[j]; j++) {
   if (mod_isbroken (pex[j])) {
    broken++;
   } else {
    int task = mod_gettask (service);

    if (!task ||
        ((task & einit_module_enable) && mod_isprovided (pex[j])) ||
        ((task & einit_module_disable) && !mod_isprovided (pex[j]))) {
     done++;

     mod_post_examine (pex[j]);
    }

    mod_workthread_create (pex[j]);
   }
  }
  free (pex);

  if ((broken + done) == j) {
   mod_remove_defer (service);
   mod_workthread_create (service);
  }
 }
}

char mod_disable_users (struct lmodule *module, char *sname) {
 if (!service_usage_query(service_not_in_use, module, NULL)) {
  ssize_t i = 0;
  char **need = NULL;
  char **t = service_usage_query_cr (service_get_services_that_use, module, NULL);
  char retval = 1;
  char def = 0;

  if (t) {
   for (; t[i]; i++) {
    if (mod_isbroken (t[i]) && !shutting_down) { /* the is-broken rule actually only holds true if we're not shutting down (if we are, then broken services get zapp'd, so they're still gone */
     if (need) free (need);
     return 0;
    } else {
     emutex_lock (&ml_tb_current_mutex);

     if (!inset ((const void **)current.disable, (void *)t[i], SET_TYPE_STRING)) {
      retval = 2;
      need = (char **)setadd ((void **)need, t[i], SET_TYPE_STRING);
     }

/*     if (module->si && module->si->provides) {
      uint32_t y = 0;
      for (; module->si->provides[y]; y++)
       if (!mod_haschanged (t[i])) {
        retval = 2;
        def++;
        need = (char **)setadd ((void **)need, t[i], SET_TYPE_STRING);
        mod_defer_until (sname, t[i]);
//        notice (2, "%s: goes after %s!", module->si->provides[y], t[i]);
       }
     }*/

     if (!mod_haschanged (t[i])) {
      retval = 2;
      def++;
      need = (char **)setadd ((void **)need, t[i], SET_TYPE_STRING);
      if (mod_defer_until (sname, t[i])) {
       emutex_unlock (&ml_tb_current_mutex);
       return 0; /* circular dependency, we'll just bail out */
      }
//      notice (2, "%s: goes after %s!", module->si->provides[y], t[i]);
     }

     emutex_unlock (&ml_tb_current_mutex);
    }
   }

   if (retval == 2) {
//    mod_defer_notice (module, need);
//    notice (2, "%s: still need to disable %s!", module->module->rid, set2str(':', need));

    emutex_lock (&ml_tb_current_mutex);

    char **tmp = (char **)setcombine ((const void **)current.disable, (const void **)need, SET_TYPE_STRING);
    if (current.disable) free (current.disable);
    current.disable = tmp;

    emutex_unlock (&ml_tb_current_mutex);

    for (i = 0; need[i]; i++) {
     mod_workthread_create (need[i]);
    }
   }

   if (!def) {
//    notice (2, "%s: wtf!?", module->module->rid);

    return 0;
   }

  }

  return retval;
 } else {
  char hd = 0;
  if (module && module->si && module->si->provides) {
   uint32_t r = 0;

   for (; module->si->provides[r]; r++) {
    if (mod_reorder (module, einit_module_disable, module->si->provides[r], 1)) hd = 2;
   }
  }

  return hd;
 }

 return 0;
}

char mod_enable_requirements (struct lmodule *module, char *sname) {
 if (!service_usage_query(service_requirements_met, module, NULL)) {
  char retval = 1;
  if (module->si && module->si->requires) {
   ssize_t i = 0;
   char **need = NULL;

   for (; module->si->requires[i]; i++) {
    if (mod_isbroken (module->si->requires[i])) {
     if (need) free (need);
     return 0;
    } else if (!service_usage_query (service_is_provided, NULL, module->si->requires[i])) {
     emutex_lock (&ml_tb_current_mutex);

#ifdef DEBUG
     notice (4, "(%s) still need %s:", set2str(' ', (const char **)module->si->provides), module->si->requires[i]);

     if (mod_haschanged (module->si->requires[i])) {
      notice (4, "not provided but still has changed?");
     }
#endif

     if (!inset ((const void **)current.enable, (void *)module->si->requires[i], SET_TYPE_STRING)) {
      retval = 2;
      need = (char **)setadd ((void **)need, module->si->requires[i], SET_TYPE_STRING);
     }

     emutex_unlock (&ml_tb_current_mutex);
    }

//    mod_defer_until (sname, module->si->requires[i]);
    if (mod_defer_until (sname, module->si->requires[i])) return 0; /* circular dependency, we'll just bail out */

/*    if (module->si && module->si->provides) {
     uint32_t y = 0;
//     for (; module->si->provides[y]; y++) {
      mod_defer_until (sname, module->si->requires[i]);
//     }
    }*/
   }

   if (retval == 2) {
//    mod_defer_notice (module, need);

    emutex_lock (&ml_tb_current_mutex);

    char **tmp = (char **)setcombine ((const void **)current.enable, (const void **)need, SET_TYPE_STRING);
    if (current.enable) free (current.enable);
    current.enable = tmp;

    emutex_unlock (&ml_tb_current_mutex);

    for (i = 0; need[i]; i++) {
     mod_workthread_create (need[i]);
    }
   }
  }

  return retval;
 }

 return 0;
}

void mod_apply_enable (struct stree *des) {
 if (!des) return;
  struct lmodule **lm = (struct lmodule **)des->value;

  if (lm && lm[0]) {
   struct lmodule *first = lm[0];

   do {
    struct lmodule *current = lm[0];

    if (current->status & status_enabled) {
#ifdef DEBUG
     notice (4, "not spawning thread thread for %s; (already up)", des->key);
#endif

     mod_post_examine(des->key);

     mod_workthreads_dec_changed(des->key);
     return;
    }

//    if (mod_isprovided (des->key) || mod_haschanged(des->key) || mod_isbroken(des->key)) {
    if (mod_isprovided (des->key)) {
#ifdef DEBUG
     notice (4, "%s; exiting (is already up)", des->key);
#endif

     mod_post_examine(des->key);

     mod_workthreads_dec_changed(des->key);
     return;
    }

    if (mod_enable_requirements (current, des->key)) {
#ifdef DEBUG
     notice (4, "not spawning thread thread for %s; exiting (not quite there yet)", des->key);
#endif
     mod_pre_examine(des->key);

     mod_workthreads_dec(des->key);
     return;
    }

    mod (einit_module_enable, current, NULL);

/* check module status or return value to find out if it's appropriate for the task */
    if ((current->status & status_enabled) || mod_isprovided (des->key)) {
#ifdef DEBUG
     notice (4, "%s; exiting (is up)", des->key);
#endif

     mod_post_examine(des->key);

     mod_workthreads_dec_changed(des->key);
     return;
    }

/* next module */
    emutex_lock (&ml_service_list_mutex);

/* make sure there's not been a different thread that did what we want to do */
    if ((lm[0] == current) && lm[1]) {
     ssize_t rx = 1;

#ifdef DEBUG
     notice (10, "service %s: done with module %s, rotating the list", des->key, (current->module && current->module->rid ? current->module->rid : "unknown"));
#endif

     for (; lm[rx]; rx++) {
      lm[rx-1] = lm[rx];
     }

     lm[rx-1] = current;
    }

    emutex_unlock (&ml_service_list_mutex);
   } while (lm[0] != first);
/* if we tried to enable something and end up here, it means we did a complete
   round-trip and nothing worked */

   emutex_lock (&ml_tb_current_mutex);
   current.enable = strsetdel(current.enable, des->key);
   emutex_unlock (&ml_tb_current_mutex);

/* mark service broken if stuff went completely wrong */
   notice (2, "ran out of options for service %s (enable), marking as broken", des->key);

   mod_mark (des->key, MARK_BROKEN);
  }

#ifdef DEBUG
  notice (4, "not spawning thread thread for %s; exiting (end of function)", des->key);
#endif
  mod_post_examine(des->key);

  mod_workthreads_dec(des->key);
  return;
}

void mod_apply_disable (struct stree *des) {
 if (!des) return;

  struct lmodule **lm = (struct lmodule **)des->value;

  if (lm && lm[0]) {
   struct lmodule *first = lm[0];
   char any_ok = 0, failures = 0;

   do {
    struct lmodule *current = lm[0];

    if (!mod_isprovided (des->key) || mod_haschanged(des->key) || mod_isbroken(des->key)) {
#ifdef DEBUG
     notice (4, "%s; exiting (not up yet)", des->key);
#endif

     mod_post_examine(des->key);

     mod_workthreads_dec_changed(des->key);
     return;
    }

    if ((current->status & status_disabled) || (current->status == status_idle)) {
#ifdef DEBUG
     eprintf (stderr, "%s (%s) disabled...", des->key, current->module->rid);
#endif
     any_ok = 1;
     goto skip_module;
    }

    if (mod_disable_users (current, des->key)) {
#ifdef DEBUG
     eprintf (stderr, "cannot disable %s yet...", des->key);
#endif

     mod_pre_examine(des->key);

     mod_workthreads_dec(des->key);
     return;
    }

    mod (einit_module_disable, current, NULL);

    /* check module status or return value to find out if it's appropriate for the task */
    if ((current->status & status_disabled) || (current->status == status_idle)) {
     any_ok = 1;
    } else {
//     eputs ("gonna ZAPP! something later...\n", stderr);
     failures = 1;
    }

    if (!mod_isprovided (des->key)) {
#ifdef DEBUG
     notice (4, "%s; exiting (done already)", des->key);
#endif

     mod_post_examine(des->key);

     mod_workthreads_dec_changed(des->key);
     return;
    }

    skip_module:
/* next module */
    emutex_lock (&ml_service_list_mutex);

/* make sure there's not been a different thread that did what we want to do */
    if ((lm[0] == current) && lm[1]) {
     ssize_t rx = 1;

#ifdef DEBUG
     notice (10, "service %s: done with module %s, rotating the list", des->key, (current->module && current->module->rid ? current->module->rid : "unknown"));
#endif

     for (; lm[rx]; rx++) {
      lm[rx-1] = lm[rx];
     }

     lm[rx-1] = current;
    }

    emutex_unlock (&ml_service_list_mutex);
   } while (lm[0] != first);
/* if we tried to disable something and end up here, it means we did a complete
   round-trip and nothing worked */

/* zap stuff that's broken */
/*   if (failures) {
    eputs ("ZAPP...?\n", stderr);
   }*/
   if (shutting_down && mod_isprovided (des->key)) {
    struct lmodule *first = lm[0];

    notice (1, "was forced to ZAPP! %s", des->key);

    do {
     struct lmodule *current = lm[0];

     if (current->status & status_enabled)
      mod (einit_module_custom, current, "zap");

     emutex_lock (&ml_service_list_mutex);
     if ((lm[0] == current) && lm[1]) {
      ssize_t rx = 1;

#ifdef DEBUG
      notice (10, "service %s: done with module %s, rotating the list", des->key, (current->module && current->module->rid ? current->module->rid : "unknown"));
#endif

      for (; lm[rx]; rx++) {
       lm[rx-1] = lm[rx];
      }

      lm[rx-1] = current;
     }
     emutex_unlock (&ml_service_list_mutex);
    } while (lm[0] != first);
   }

   emutex_lock (&ml_tb_current_mutex);
   current.disable = strsetdel(current.disable, des->key);
   emutex_unlock (&ml_tb_current_mutex);

   if (any_ok) {
    mod_post_examine(des->key);

    mod_workthreads_dec_changed(des->key);
    return;
   }

/* mark service broken if stuff went completely wrong */
   notice (2, "ran out of options for service %s (disable), marking as broken", des->key);

   mod_mark (des->key, MARK_BROKEN);
  }

  mod_post_examine(des->key);

  mod_workthreads_dec(des->key);
  return;
}

int mod_gettask (char * service) {
 int task = shutting_down ? einit_module_disable : einit_module_enable;

 emutex_lock (&ml_tb_current_mutex);
 if (inset ((const void **)current.disable, service, SET_TYPE_STRING))
  task = einit_module_disable;
 else if (inset ((const void **)current.enable, service, SET_TYPE_STRING))
  task = einit_module_enable;
 else {
  emutex_unlock (&ml_tb_current_mutex);

  emutex_lock (&ml_tb_target_state_mutex);
  if (inset ((const void **)target_state.disable, service, SET_TYPE_STRING))
   task = einit_module_disable;
  else if (inset ((const void **)target_state.enable, service, SET_TYPE_STRING))
   task = einit_module_enable;
  emutex_unlock (&ml_tb_target_state_mutex);

  return task;
 }
 emutex_unlock (&ml_tb_current_mutex);

 return task;
}

char mod_examine_group (char *groupname) {
 struct group_data *gd = mod_group_get_data (groupname);
 if (!gd) return 0;
 char post_examine = 0;
 char **members = NULL;
 uint32_t options = 0;

 emutex_lock (&ml_group_data_mutex);
 if (gd->members) {
  members = (char **)setdup ((const void **)gd->members, SET_TYPE_STRING);
 }
 options = gd->options;
 emutex_unlock (&ml_group_data_mutex);

 if (members) {
  int task = mod_gettask (groupname);

  if ((task & einit_module_enable) && mod_isprovided (groupname)) {
   emutex_lock (&ml_changed_mutex);
   if (!inset ((const void **)changed_recently, (const void *)groupname, SET_TYPE_STRING))
    changed_recently = (char **)setadd ((void **)changed_recently, (const void *)groupname, SET_TYPE_STRING);
   emutex_unlock (&ml_changed_mutex);

   mod_post_examine (groupname);

   return 1;
  }

//  notice (2, "group %s: examining members", groupname);

  ssize_t x = 0, mem = setcount ((const void **)members), failed = 0, on = 0, changed = 0, groupc = 0;
  struct lmodule **providers = NULL;
  char group_failed = 0, group_ok = 0;

  for (; members[x]; x++) {
   if (mod_haschanged (members[x]) || mod_isbroken (members[x]))
    changed++;

   if (mod_isbroken (members[x])) {
    failed++;
   } else {
    struct stree *serv = NULL;

    emutex_lock (&ml_service_list_mutex);

    if (mod_isprovided(members[x])) {
     on++;

     if (module_logics_service_list && (serv = streefind(module_logics_service_list, members[x], tree_find_first))) {
      struct lmodule **lm = (struct lmodule **)serv->value;

      if (lm) {
       ssize_t y = 0;

       for (; lm[y]; y++) {
        if ((lm[y]->status & status_enabled) && (!providers || !inset ((const void **)providers, (const void *)lm[y], SET_NOALLOC))) {
         providers = (struct lmodule **)setadd ((void **)providers, (void *)lm[y], SET_NOALLOC);

         break;
        }
       }
      }
     } else {
      struct lmodule **lm = (struct lmodule **)service_usage_query_cr (service_get_providers, NULL, members[x]);

      groupc++; /* must be a group... */

      if (lm) {
       ssize_t y = 0;

       for (; lm[y]; y++) {
        if (!providers || !inset ((const void **)providers, (const void *)lm[y], SET_NOALLOC)) {
         providers = (struct lmodule **)setadd ((void **)providers, (void *)lm[y], SET_NOALLOC);

#ifdef DEBUG
         eprintf (stderr, " ** group %s provided by %s (groupc=%i)", groupname, lm[y]->module->name, (int)groupc);
#endif
        }
       }
      }
     }
    }

    emutex_unlock (&ml_service_list_mutex);

   }
  }

#if 0
  if ((changed + failed) >= mem) { // well, no matter what's gonna happen, if all members changed or are broken, then this changed too
   emutex_lock (&ml_changed_mutex);
   if (!inset ((const void **)changed_recently, (const void *)groupname, SET_TYPE_STRING))
    changed_recently = (char **)setadd ((void **)changed_recently, (const void *)groupname, SET_TYPE_STRING);
   emutex_unlock (&ml_changed_mutex);
  }
#endif

  if (!on || ((task & einit_module_disable) && ((changed >= mem) || (on == groupc)))) {
   if (task & einit_module_disable) {
    emutex_lock (&ml_changed_mutex);
    if (!inset ((const void **)changed_recently, (const void *)groupname, SET_TYPE_STRING))
     changed_recently = (char **)setadd ((void **)changed_recently, (const void *)groupname, SET_TYPE_STRING);
    emutex_unlock (&ml_changed_mutex);
   }

   if (mod_isprovided (groupname)) {
    emutex_lock (&ml_currently_provided_mutex);
    currently_provided = (char **)strsetdel ((char **)currently_provided, (char *)groupname);
    emutex_unlock (&ml_currently_provided_mutex);

    emutex_lock (&ml_tb_current_mutex);
    current.enable = strsetdel (current.enable, groupname);
    current.disable = strsetdel (current.disable, groupname);
    emutex_unlock (&ml_tb_current_mutex);

    emutex_lock (&ml_changed_mutex);
    if (!inset ((const void **)changed_recently, (const void *)groupname, SET_TYPE_STRING))
     changed_recently = (char **)setadd ((void **)changed_recently, (const void *)groupname, SET_TYPE_STRING);
    emutex_unlock (&ml_changed_mutex);

#ifdef DEBUG
    notice (2, "marking group %s off", groupname);
#endif

    post_examine = 1;

    mod_ping_all_threads();
   }

   if (task & einit_module_disable) {
    post_examine = 1;
   }
  }

  if (task & einit_module_enable) {
   if (on) {
    if (options & (MOD_PLAN_GROUP_SEQ_ANY | MOD_PLAN_GROUP_SEQ_ANY_IOP)) {
     if (on > 0) {
      group_ok = 1;
     }
    } else if (options & MOD_PLAN_GROUP_SEQ_MOST) {
     if (on && ((on + failed) >= mem)) {
      group_ok = 1;
     } else if (changed >= mem) {
      if (on) group_ok = 1;
      else group_failed = 1;
     }
    } else if (options & MOD_PLAN_GROUP_SEQ_ALL) {
     if (on >= mem) {
      group_ok = 1;
     } else if (failed) {
      group_failed = 1;
     }
    } else {
#ifdef DEBUG
     notice (2, "marking group %s broken (bad group type)", groupname);
#endif

     mod_mark (groupname, MARK_BROKEN);
    }
   } else {
    if (failed >= mem) {
     group_failed = 1;
    } else if (changed >= mem) {
     group_failed = 1;
    }
   }

   if (group_ok) {
    notice (5, "marking group %s up", groupname);

    emutex_lock (&ml_tb_current_mutex);
    current.enable = strsetdel (current.enable, groupname);
    current.disable = strsetdel (current.disable, groupname);
    emutex_unlock (&ml_tb_current_mutex);

    emutex_lock (&ml_changed_mutex);
    if (!inset ((const void **)changed_recently, (const void *)groupname, SET_TYPE_STRING))
     changed_recently = (char **)setadd ((void **)changed_recently, (const void *)groupname, SET_TYPE_STRING);
    emutex_unlock (&ml_changed_mutex);

    service_usage_query_group (service_set_group_providers, (struct lmodule *)providers, groupname);

    if (!mod_isprovided (groupname)) {
     emutex_lock (&ml_currently_provided_mutex);
     currently_provided = (char **)setadd ((void **)currently_provided, (void *)groupname, SET_TYPE_STRING);
     emutex_unlock (&ml_currently_provided_mutex);
    }

    post_examine = 1;

    mod_ping_all_threads();
   } else if (group_failed) {
    notice (2, "marking group %s broken (group requirements failed)", groupname);

    mod_mark (groupname, MARK_BROKEN);
   } else {
/* just to make sure everything will actually be enabled/disabled */
    emutex_lock (&ml_tb_current_mutex);
    mod_flatten_current_tb_group(groupname, task);
    emutex_unlock (&ml_tb_current_mutex);
   }
  } else { /* mod_disable */
/* just to make sure everything will actually be enabled/disabled */
   emutex_lock (&ml_tb_current_mutex);
   mod_flatten_current_tb_group(groupname, task);
   emutex_unlock (&ml_tb_current_mutex);
  }

  if (providers) free (providers);

  free (members);
 }

 if (post_examine) {
  mod_post_examine (groupname);
 }

#if 0
 mod_ping_all_threads();
#endif
 return 1;
}

char mod_reorder (struct lmodule *lm, int task, char *service, char dolock) {
 char **before = NULL, **after = NULL, **xbefore = NULL, hd = 0;

 if (!lm) {
  fflush (stderr);
  emutex_lock (&ml_service_list_mutex);
  struct stree *v = streefind (module_logics_service_list, service, tree_find_first);
  struct lmodule **lmx = v ? v->value : NULL;
  emutex_unlock (&ml_service_list_mutex);
  fflush (stderr);

  if (lmx && lmx[0]) lm = lmx[0];
  else return 0;
 }

 if (lm->si && (lm->si->before || lm->si->after)) {
  if (task & einit_module_enable) {
   before = lm->si->before;
   after = lm->si->after;
  } else if (task & einit_module_disable) {
   if (shutting_down && lm->si->shutdown_before)
    before = lm->si->shutdown_before;
   else
    before = lm->si->after;

   if (shutting_down && lm->si->shutdown_after)
    after = lm->si->shutdown_after;
   else
    after = lm->si->before;
  }
 }

 /* "loose" service ordering */
 if (before) {
  uint32_t i = 0;

  for (; before[i]; i++) {
   char **d;

   if (dolock) emutex_lock (&ml_tb_current_mutex);
   if ((d = inset_pattern ((const void **)(task & einit_module_enable ? current.enable : current.disable), before[i], SET_TYPE_STRING)) && (d = strsetdel (d, service))) {
    uint32_t y = 0;
    if (dolock) emutex_unlock (&ml_tb_current_mutex);

    if (lm->si->requires) {
     for (y = 0; lm->si->requires[y]; y++) {
      d = strsetdel (d, lm->si->requires[y]);
     }

     if (!d) continue;
    }

    if (lm->si->provides) {
     for (y = 0; lm->si->provides[y]; y++) {
      d = strsetdel (d, lm->si->provides[y]);
     }

     if (!d) continue;
    }

    for (y = 0; d[y]; y++) {
    if (mod_isbroken (d[y]) || mod_haschanged (d[y])) continue;
     struct group_data *gd = mod_group_get_data(d[y]);

     if (!gd || !gd->members || !inset ((const void **)gd->members, (void *)service, SET_TYPE_STRING)) {
//      notice (1, "%s is before: %s", service, d[y]);

      mod_defer_until (d[y], service);
//      mod_defer_until (service, d[y]);

      xbefore = (char **)setadd ((void **)xbefore, (void *)d[y], SET_TYPE_STRING);
     }
    }

    free (d);
   } else {
    if (dolock) emutex_unlock (&ml_tb_current_mutex);
   }
  }
 }
 if (after) {
  uint32_t i = 0;

  for (; after[i]; i++) {
   char **d;
   if (dolock) emutex_lock (&ml_tb_current_mutex);
   if ((d = inset_pattern ((const void **)(task & einit_module_enable ? current.enable : current.disable), after[i], SET_TYPE_STRING)) && (d = strsetdel (d, service))) {
    uint32_t y = 0;
    if (dolock) emutex_unlock (&ml_tb_current_mutex);

    if (lm->si->requires) {
     for (y = 0; lm->si->requires[y]; y++) {
      d = strsetdel (d, lm->si->requires[y]);
     }

     if (!d) continue;
    }

    if (lm->si->provides) {
     for (y = 0; lm->si->provides[y]; y++) {
      d = strsetdel (d, lm->si->provides[y]);
     }

     if (!d) continue;
    }

    for (y = 0; d[y]; y++) {
     if (mod_isbroken (d[y]) || mod_haschanged (d[y])) continue;
     struct group_data *gd = mod_group_get_data(d[y]);

     if ((!xbefore || !inset ((const void **)xbefore, (void *)d[y], SET_TYPE_STRING)) &&
           (!gd || !gd->members || !inset ((const void **)gd->members, (void *)service, SET_TYPE_STRING))) {
      if (!mod_defer_until (service, d[y])) {
//      notice (1, "%s goes after %s", service, d[y]);
       hd = 1;
      }
     }
    }

//    mod_defer_notice (lm, d);

    free (d);
   } else {
    if (dolock) emutex_unlock (&ml_tb_current_mutex);
   }
  }
 }

 return hd;
}

void mod_examine (char *service) {
 char rdloops = 5;

/* if (mod_haschanged (service)) {
  mod_post_examine (service);
  
 }*/

 if (mod_isbroken (service)) {
  is_broken:

  notice (2, "service %s marked as being broken", service);

  mod_workthreads_dec(service);

/* make sure this is not still queued for something */
  int task = mod_gettask (service);

  if (task) {
   emutex_lock (&ml_tb_current_mutex);
   if (task & einit_module_enable) {
    current.enable = strsetdel (current.enable, service);
   } else if (task & einit_module_disable) {
    current.disable = strsetdel (current.disable, service);
   }
   emutex_unlock (&ml_tb_current_mutex);
  }

  mod_post_examine(service);

  return;
 } else if (mod_examine_group (service)) {
#ifdef DEBUG
  notice (2, "service %s: group examination complete", service);
#endif

  if (!mod_haschanged (service)) {
   char retries = 5;

   do {
//    mod_pre_examine(service);

    mod_wait_for_ping();

    mod_examine_group (service);

    retries--;

    if ((retries <= 0) || mod_isbroken (service)) {
     mod_workthreads_dec(service);

     return;
    }
   } while ((mod_isdeferred(service) || (!mod_haschanged (service) && !mod_isbroken (service))));
  }

  mod_workthreads_dec(service);

  return;
 } else if (mod_isdeferred (service)) {

  recycle_wait:
  { /* try to save some threads */
   char retries = 5;

   do {

//    mod_pre_examine(service);

    mod_wait_for_ping();

    if (mod_isbroken (service) || mod_haschanged (service)) {
     goto is_broken;
    }

    retries--;

    if ((retries <= 0) || mod_isbroken (service)) {
     mod_workthreads_dec(service);

     return;
    }
   } while (mod_isdeferred (service));
  }
 }

 {
  int task = mod_gettask (service);

  if (!task ||
      ((task & einit_module_enable) && mod_isprovided (service)) ||
      ((task & einit_module_disable) && !mod_isprovided (service))) {

#ifdef DEBUG
   notice (1, "service %s is already in the right state", service);
#endif

   mod_post_examine (service);

   mod_workthreads_dec_changed(service);

   return;
  } else {
#ifdef DEBUG
   notice (1, "service %s is not in the right state", service);
#endif
  }

#ifdef DEBUG
  notice (1, " ** examining service %s (%s).\n", service,
                   task & einit_module_enable ? "enable" : "disable");
#endif

  emutex_lock (&ml_service_list_mutex);
  struct stree *v = streefind (module_logics_service_list, service, tree_find_first);
  struct lmodule **lm = v ? v->value : NULL;
  emutex_unlock (&ml_service_list_mutex);

  if (lm && lm[0]) {
   pthread_t th;
   char hd;
   hd = mod_reorder (lm[0], task, service, 1);

   if (hd) {
    if (rdloops > 0) {
#ifdef DEBUG
     notice (1, "service %s: jumping back", service);
#endif
     rdloops--;
     goto recycle_wait;
    } else {
#ifdef DEBUG
     notice (1, "service %s: giving up", service);
#endif

     if (mod_workthreads_dec(service)) return;

     return;
    }
   }

#ifdef DEBUG
   notice (1, "service %s: spawning thread", service);
#endif

   if (task & einit_module_enable) {
    if (ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_apply_enable, v)) {
     mod_apply_enable(v);
    }
   } else {
    if (ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))mod_apply_disable, v)) {
     mod_apply_disable(v);
    }
   }
  } else {
   notice (2, "cannot resolve service %s.", service);

   mod_mark (service, MARK_UNRESOLVED);

   mod_workthreads_dec(service);
  }

  return;
 }
 mod_workthreads_dec(service);
}

void workthread_examine (char *service) {
 mod_examine (service);
 free (service);
}

void mod_workthread_create(char *service) {
 if (mod_haschanged (service) || mod_isbroken(service)) {
  mod_post_examine (service);  
 } else if (mod_workthreads_inc(service)) {
  mod_ping_all_threads();
 } else {
  char *drx = estrdup (service);
  pthread_t th;

  ethread_create (&th, &thread_attribute_detached, (void *(*)(void *))workthread_examine, drx);
 }
}

void mod_spawn_batch(char **batch, int task) {
 char **dospawn = NULL;
 uint32_t i, deferred, broken, groupc;

 retry:
 
 deferred = 0; broken = 0; groupc = 0;

 if (dospawn) {
  free(dospawn);
  dospawn = NULL;
 }

 if (!batch) return;

 for (i = 0; batch[i]; i++) {
  if (mod_isbroken(batch[i])) {
   broken++;
//   eprintf (stderr, " !! %s\n", batch[i]);
  } else if (mod_isdeferred(batch[i]) || mod_reorder(NULL, task, batch[i], 0)) {
   deferred++;
//   eprintf (stderr, " !! %s\n", batch[i]);

   groupc += mod_group_get_data (batch[i]) ? 1 : 0;
  } else if ((task == einit_module_enable) && mod_isprovided (batch[i])) {
   current.enable = strsetdel (current.enable, batch[i]);
   batch = current.enable;
   goto retry;
  } else if ((task == einit_module_disable) && !mod_isprovided (batch[i])) {
   current.disable = strsetdel (current.disable, batch[i]);
   batch = current.disable;
   goto retry;
  } else {
   dospawn = (char **)setadd ((void **)dospawn, batch[i], SET_TYPE_STRING);

   groupc += mod_group_get_data (batch[i]) ? 1 : 0;
  }
 }

#ifdef DEBUG
 char *alist = set2str (' ', (const char **)batch);

 eprintf (stderr, "i=%i (%s), broken=%i, deferred=%i, groups=%i\n", i, alist ? alist : "none", broken, deferred, groupc);

 if (alist) free (alist);
#endif

 if (i == (broken + deferred + groupc)) {
/* foo: circular dependencies? kill the whole chain and hope for something good... */
  emutex_lock(&ml_chain_examine);
  if (module_logics_chain_examine) {
   streefree (module_logics_chain_examine);
   module_logics_chain_examine = NULL;
  }
  if (module_logics_chain_examine_reverse) {
   streefree (module_logics_chain_examine_reverse);
   module_logics_chain_examine_reverse = NULL;
  }
  emutex_unlock(&ml_chain_examine);
 }

 if (dospawn) {
  for (i = 0; dospawn[i]; i++) {
   mod_workthread_create (dospawn[i]);
  }

  free (dospawn);
  dospawn = NULL;
 }

// ignorereorderfor =
}

void mod_spawn_workthreads () {
 emutex_lock(&ml_chain_examine);
 if (module_logics_chain_examine) {
  streefree (module_logics_chain_examine);
  module_logics_chain_examine = NULL;
 }
 if (module_logics_chain_examine_reverse) {
  streefree (module_logics_chain_examine_reverse);
  module_logics_chain_examine_reverse = NULL;
 }
 emutex_unlock(&ml_chain_examine);

 emutex_lock (&ml_tb_current_mutex);
 if (current.enable) {
  mod_spawn_batch(current.enable, einit_module_enable);
 }

 if (current.disable) {
  mod_spawn_batch(current.disable, einit_module_disable);
 }
 emutex_unlock (&ml_tb_current_mutex);

#if 0
 mod_ping_all_threads();
#endif
}

void mod_commit_and_wait (char **en, char **dis) {
 int remainder;
// uint32_t iterations = 0;

#if 0
 mod_ping_all_threads();
#endif

 mod_sort_service_list_items_by_preference();

#ifdef DEBUG
 eputs ("flattening...\n", stderr);
 fflush(stderr);
#endif
 mod_flatten_current_tb();

#ifdef DEBUG
 eputs ("flat as a pancake now...\n", stderr);
 fflush(stderr);
#endif

 mod_commits_inc();

 while (1) {
  remainder = 0;
//  iterations++;

#ifdef DEBUG
  char **stillneed = NULL;
#endif

  if (en) {
   uint32_t i = 0;

   for (; en[i]; i++) {
    if (!mod_isbroken (en[i]) && !mod_haschanged(en[i]) && !mod_isprovided(en[i])) {
#ifdef DEBUG
     eprintf (stderr, "not yet provided: %s\n", en[i]);
     stillneed = (char **)setadd ((void **)stillneed, en[i], SET_TYPE_STRING);
#endif

     remainder++;

     emutex_lock (&ml_tb_current_mutex);
     if (!inset ((const void **)current.enable, en[i], SET_TYPE_STRING)) {
      emutex_unlock (&ml_tb_current_mutex);

#ifdef DEBUG
      notice (2, "something must've gone wrong with service %s...", en[i]);
#endif

      remainder--;
     } else
      emutex_unlock (&ml_tb_current_mutex);
    }
   }
  }

  if (dis) {
   uint32_t i = 0;

   for (; dis[i]; i++) {
    if (!mod_isbroken (dis[i]) && !mod_haschanged(dis[i]) && mod_isprovided(dis[i])) {
#ifdef DEBUG
     eprintf (stderr, "still provided: %s\n", dis[i]);
     stillneed = (char **)setadd ((void **)stillneed, dis[i], SET_TYPE_STRING);
#endif

     remainder++;

     emutex_lock (&ml_tb_current_mutex);
     if (!inset ((const void **)current.disable, dis[i], SET_TYPE_STRING)) {
      current.disable = (char **)setadd ((void **)current.disable, dis[i], SET_TYPE_STRING);
      emutex_unlock (&ml_tb_current_mutex);

#ifdef DEBUG
      notice (2, "something must've gone wrong with service %s...", dis[i]);
#endif

      remainder--;
     } else
      emutex_unlock (&ml_tb_current_mutex);
    }
   }
  }

  if (remainder <= 0) {
   mod_commits_dec();

//   pthread_mutex_destroy (&ml_service_update_mutex);
   return;
  }

#ifdef DEBUG
  if (!stillneed) {
   notice (4, "still need %i services\n", remainder);
  } else {
   notice (4, "still need %i services (%s)\n", remainder, set2str (' ', (const char **)stillneed));
  }
  fflush (stderr);

  emutex_lock (&ml_workthreads_mutex);

  notice (4, "workthreads: %i (%s)\n", ml_workthreads, set2str (' ', (const char **)lm_workthreads_list));
  emutex_unlock (&ml_workthreads_mutex);
#endif

#if 0
  if (iterations >= MAX_ITERATIONS) {
   notice (1, "plan aborted (too many iterations: %i).\n", iterations);

#ifdef DEBUG
   notice (4, "aborted: enable=%s; disable=%s\n", en ? set2str (' ', (const char **)en) : "none", dis ? set2str (' ', (const char **)dis) : "none");
   fflush (stderr);
#endif

   mod_commits_dec();
//   pthread_mutex_destroy (&ml_service_update_mutex);
   return;
  }
#endif

//  notice (1, "waiting for ping...");
  mod_wait_for_ping();
//  notice (1, "got ping...");

  if (!modules_work_count && modules_last_change) {
   if ((modules_last_change + EINIT_PLAN_CHANGE_STALL_TIMEOUT) < time(NULL)) {
    notice (1, "PLAN ABORTED: didn't do anything at all for too long");

    if (en) {
     uint32_t i = 0;

     for (; en[i]; i++) {
      if (!mod_isbroken (en[i]) && !mod_haschanged(en[i]) && !mod_isprovided(en[i])) {
       mod_mark (en[i], MARK_BROKEN);
      }
     }
    }

    if (dis) {
     uint32_t i = 0;

     for (; dis[i]; i++) {
      if (!mod_isbroken (dis[i]) && !mod_haschanged(dis[i]) && mod_isprovided(dis[i])) {
       mod_mark (dis[i], MARK_BROKEN);
      }
     }
    }

    while (ml_workthreads) {
     emutex_lock (&ml_workthreads_mutex);
     if (lm_workthreads_list) {
      uint32_t i = 0;

      for (; lm_workthreads_list[i]; i++) {
       mod_mark (lm_workthreads_list[i], MARK_BROKEN);
      }
     }
     emutex_unlock (&ml_workthreads_mutex);

     mod_ping_all_threads();
     sleep (1);
    }

    mod_commits_dec();

    return;
   }
  }
 };

/* never reached */
 mod_commits_dec();
}

/* end new functions */
