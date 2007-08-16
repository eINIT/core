/*
 *  compatibility-sysv-initctl.c
 *  einit
 *
 *  Created by Magnus Deininger on 11/05/2006.
 *  renamed and moved from einit-utmp-forger.c on 2006/12/28
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
#include <stdio.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/event.h>
#include <einit/bitch.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <einit-modules/ipc.h>

#define INITCTL_MAGIC 0x03091969
#define INITCTL_CMD_START        0x00000000
#define INITCTL_CMD_RUNLVL       0x00000001
#define INITCTL_CMD_POWERFAIL    0x00000002
#define INITCTL_CMD_POWERFAILNOW 0x00000003
#define INITCTL_CMD_POWEROK      0x00000004

#define INITCTL_CMD_SETENV       0x00000006
#define INITCTL_CMD_UNSETENV     0x00000007

struct init_command {
 uint32_t signature;    // signature, must be INITCTL_MAGIC
 uint32_t command;      // the request ID
 uint32_t runlevel;     // the runlevel argument
 uint32_t timeout;      // time between TERM and KILL
 char     padding[368]; // padding, legacy applications expect the struct to be 384 bytes long
};

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int compatibility_sysv_initctl_configure (struct lmodule *);

struct einit_cfgvar_info
  compatibility_sysv_initctl_cfgvar_initctl = {
   .options = eco_optional,
   .variable = "configuration-compatibility-sysv-initctl",
   .description = "Location for the initctl interface, and whether or not to use it." },
  *compatibility_sysv_initctl_configuration[] = { &compatibility_sysv_initctl_cfgvar_initctl, NULL };

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule module_compatibility_sysv_initctl_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "System-V Compatibility: initctl",
 .rid       = "compatibility-sysv-initctl",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = compatibility_sysv_initctl_configure,
 .configuration = compatibility_sysv_initctl_configuration
};

module_register(module_compatibility_sysv_initctl_self);

#endif

char compatibility_sysv_initctl_running = 0;

pthread_t initctl_thread;

void * initctl_wait (char *);

void compatibility_sysv_initctl_einit_event_handler(struct einit_event *ev) {
 if (ev->type == einit_core_service_update) {
  if (ev->status & status_enabled) {
   if (ev->module && ev->module->si && ev->module->si->provides && inset ((const void **)ev->module->si->provides, "mount-system", SET_TYPE_STRING)) {
    struct cfgnode *node = cfg_getnode ("configuration-compatibility-sysv-initctl", NULL);

    if (node && !node->flag) return; // check if initctl should actually be used

    char *fifo = (node && node->svalue ? node->svalue : "/dev/initctl");
    mode_t fifomode = (node && node->value ? node->value : 0600);

    if (mkfifo (fifo, fifomode)) {
     if (errno == EEXIST) {
      if (unlink (fifo)) {
       notice (3, "could not remove stale fifo \"%s\": %s: giving up", fifo, strerror (errno));
       return;
      }
      if (mkfifo (fifo, fifomode)) {
       notice (3, "could not recreate fifo \"%s\": %s", fifo, strerror (errno));
      }
     } else {
      notice (3, "could not create fifo \"%s\": %s: giving up", fifo, strerror (errno));
      return;
     }
    }

    ethread_create (&initctl_thread, NULL, (void *(*)(void *))initctl_wait, (void *)fifo);

   }
  } else if (!(ev->status & status_enabled)) {
   if (ev->module && ev->module->si && ev->module->si->provides && inset ((const void **)ev->module->si->provides,"mount-system", SET_TYPE_STRING)) {

    char *fifo = cfg_getstring ("configuration-compatibility-sysv-initctl", NULL);
    if (!fifo) fifo =  "/dev/initctl";

    if (compatibility_sysv_initctl_running)
     ethread_cancel (initctl_thread);

    if (unlink (fifo)) {
     notice (3, "could not remove stale fifo \"%s\": %s", fifo, strerror (errno));
    }

    compatibility_sysv_initctl_running = 0;

   }
  }
 }
}

int compatibility_sysv_initctl_cleanup (struct lmodule *this) {
 ipc_cleanup (irr);
 event_ignore (einit_event_subsystem_core, compatibility_sysv_initctl_einit_event_handler);

 return 0;
}

void * initctl_wait (char *fifo) {
 int nfd;
 compatibility_sysv_initctl_running = 1;

 while ((nfd = eopen (fifo, O_RDONLY))) {
  struct init_command ic;

  if (nfd == -1) { /* open returning -1 is very bad, terminate the thread and disable the module */
   char tmp[BUFFERSIZE];
   esprintf (tmp, BUFFERSIZE, "initctl: opening FIFO failed: %s", strerror (errno));
   notice (4, tmp);
   mod (einit_module_disable, thismodule, NULL);
   compatibility_sysv_initctl_running = 0;
   return NULL;
  }

  memset (&ic, 0, sizeof (struct init_command)); // clear this struct, just in case

  if (read (nfd, &ic, sizeof(struct init_command)) > 12) { // enough bytrs to process were read
   if (ic.signature == INITCTL_MAGIC) {
//  INITCTL_CMD_START: what's that do?
//  INITCTL_CMD_UNSETENV is deliberately ignored
    if (ic.command == INITCTL_CMD_RUNLVL) { // switch runlevels (modes...)
     struct einit_event ee = evstaticinit(einit_core_switch_mode);
     char tmp[BUFFERSIZE], *nmode;

// we need to look up the runlevel to find out what mode it corresponds to:
     esprintf (tmp, BUFFERSIZE, "configuration-compatibility-sysv-runlevel-mode-relations/runlevel%c", ic.runlevel);
     nmode = cfg_getstring (tmp, NULL);
     if (nmode) {
      esprintf (tmp, BUFFERSIZE, "initctl: switching to mode %s (runlevel %c)", nmode, ic.runlevel);
      notice (4, tmp);

      ee.string = nmode; // this is where we need to put the mode to switch to

// timeout semantics are different in einit, still we could use this...
      if (ic.timeout) {
       struct cfgnode tnode;
       memset (&tnode, 0, sizeof(struct cfgnode));

       tnode.type = einit_node_regular;
       tnode.source = self->rid;
       tnode.id = "configuration-system-daemon-term-timeout-primary";
       tnode.value = ic.timeout;

       cfg_addnode (&tnode);
      }

      event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
      evstaticdestroy(ee);
     } else {
      esprintf (tmp, BUFFERSIZE, "initctl: told to switch to runlevel %c, which did not resolve to a valid mode", ic.runlevel);
      notice (3, tmp);
     }
    } else if (ic.command == INITCTL_CMD_POWERFAIL) {
     struct einit_event ee = evstaticinit(einit_power_failing);
     notice (4, "initctl: power is failing");

     event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
     evstaticdestroy(ee);
    } else if (ic.command == INITCTL_CMD_POWERFAILNOW) {
     struct einit_event ee = evstaticinit(einit_power_failure_imminent);
     notice (4, "initctl: power failure is imminent");

     event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
     evstaticdestroy(ee);
    } else if (ic.command == INITCTL_CMD_POWEROK) {
     struct einit_event ee = evstaticinit(einit_power_restored);
     notice (4, "initctl: power was restored");

     event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
     evstaticdestroy(ee);
    } else if (ic.command == INITCTL_CMD_SETENV) { // padding contains the new environment string
     char **cx = str2set (':', ic.padding);
     if (cx) {
      if (cx[0] && cx[1]) {
       if (strmatch (cx[0], "INIT_HALT")) {
        if (strmatch (cx[1], "HALT") || strmatch (cx[1], "POWERDOWN")) {
         struct einit_event ee = evstaticinit(einit_core_switch_mode);
         ee.string = "power-down";
         event_emit (&ee, einit_event_flag_spawn_thread | einit_event_flag_duplicate | einit_event_flag_broadcast);
         evstaticdestroy(ee);
        }
       }
      }

      free (cx);
     }
    } else
      notice (4, "invalid initctl received: unknown command");
   } else {
    notice (4, "invalid initctl received: invalid signature");
   }
  }

  eclose (nfd);
 }

 compatibility_sysv_initctl_running = 0;
 return NULL;
}

int compatibility_sysv_initctl_disable (void *pa, struct einit_event *status) {
 char *fifo = cfg_getstring ("configuration-compatibility-sysv-initctl", NULL);
 if (!fifo) fifo =  "/dev/initctl";

 if (compatibility_sysv_initctl_running)
  ethread_cancel (initctl_thread);

 if (unlink (fifo)) {
  char tmp[BUFFERSIZE];
  esprintf (tmp, BUFFERSIZE, "could not remove stale fifo \"%s\": %s", fifo, strerror (errno));
  status->string = tmp;
  status->flag++;
  status_update (status);
 }

 compatibility_sysv_initctl_running = 0;
 return status_ok;
}

int compatibility_sysv_initctl_configure (struct lmodule *r) {
 module_init (r);

 thismodule->cleanup = compatibility_sysv_initctl_cleanup;

 ipc_configure (r);
 event_listen (einit_event_subsystem_core, compatibility_sysv_initctl_einit_event_handler);

 return 0;
}
