/*
 *  linux-netlink.c
 *  einit
 *
 *  Created by Magnus Deininger on 07/08/2007.
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#include <einit-modules/network.h>

#include <netlink/handlers.h>
#include <netlink/netlink.h>
#include <netlink/cache.h>

#include <netlink/route/link.h>

#include <linux/if.h>
/* somehow this def is there but it isn't being picked up: */
#ifndef IFF_LOWER_UP
#define IFF_LOWER_UP 0x10000
#endif

#include <pthread.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int linux_netlink_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule module_linux_netlink_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = 0,
 .name      = "eINIT <-> NetLink Connector",
 .rid       = "linux-netlink",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = linux_netlink_configure
};

module_register(module_linux_netlink_self);

#endif

struct nl_handle *linux_netlink_handle = NULL;
char linux_netlink_connected = 0;
struct nl_cache *linux_netlink_link_cache = NULL;
struct nl_cb *linux_netlink_callbacks = NULL;

struct nl_cache *linux_netlink_address_cache = NULL;

pthread_mutex_t
  linux_netlink_interfaces_mutex = PTHREAD_MUTEX_INITIALIZER,
  linux_netlink_address_mutex = PTHREAD_MUTEX_INITIALIZER;

char **linux_netlink_interfaces = NULL;

/* nl_cache_update() seems to cause issues in valgrind, so we better not use that... */

struct network_interface *linux_netlink_get_interface_data (char *interface) {
 struct network_interface *rv = NULL;
 ssize_t rvlen = sizeof (struct network_interface) + strlen (interface) + 2;
 rv = emalloc (rvlen);
 memset (rv, 0, rvlen);

 memcpy (((char *)rv) + sizeof (struct network_interface), interface, strlen (interface));

 rv->name = ((char *)rv) + sizeof (struct network_interface);

 emutex_lock (&linux_netlink_interfaces_mutex);

 if (linux_netlink_link_cache) {
  nl_cache_destroy_and_free (linux_netlink_link_cache);
  linux_netlink_link_cache = rtnl_link_alloc_cache(linux_netlink_handle);
 } else
  linux_netlink_link_cache = rtnl_link_alloc_cache(linux_netlink_handle);

 if (!linux_netlink_link_cache) {
  emutex_unlock (&linux_netlink_interfaces_mutex);
  return rv;
 }

 struct rtnl_link *link = rtnl_link_get_by_name(linux_netlink_link_cache, interface);

 if (link) {
  unsigned int flags = rtnl_link_get_flags(link);

  if (flags & IFF_LOWER_UP)
   rv->flags |= interface_has_carrier;
  if (flags & IFF_UP)
   rv->flags |= interface_up;

  rtnl_link_put(link);
 }

 emutex_unlock (&linux_netlink_interfaces_mutex);

 return rv;
}

void linux_netlink_interface_data_collector (struct nl_object *object, void *ignored) {
 char *name = rtnl_link_get_name((struct rtnl_link *)object);
 if (name) {
  linux_netlink_interfaces = (char **)setadd ((void **)linux_netlink_interfaces, name, SET_TYPE_STRING);
 }
}

char **linux_netlink_get_all_interfaces () {
 char **retval = NULL;
 emutex_lock (&linux_netlink_interfaces_mutex);

 if (linux_netlink_link_cache) {
  nl_cache_destroy_and_free (linux_netlink_link_cache);
  linux_netlink_link_cache = rtnl_link_alloc_cache(linux_netlink_handle);
 } else
  linux_netlink_link_cache = rtnl_link_alloc_cache(linux_netlink_handle);

 if (!linux_netlink_link_cache) {
  emutex_unlock (&linux_netlink_interfaces_mutex);
  return NULL;
 }

 if (linux_netlink_link_cache) {
  if (linux_netlink_interfaces) {
   free (linux_netlink_interfaces);
   linux_netlink_interfaces = NULL;
  }
  nl_cache_foreach (linux_netlink_link_cache, linux_netlink_interface_data_collector, NULL);
 }

 retval = (char **)setdup ((const void **)linux_netlink_interfaces, SET_TYPE_STRING);

 emutex_unlock (&linux_netlink_interfaces_mutex);

 return retval;
}

int linux_netlink_cleanup (struct lmodule *this) {
 if (linux_netlink_connected) {
  nl_close (linux_netlink_handle);
  nl_handle_destroy(linux_netlink_handle);
 }

 if (linux_netlink_link_cache) {
  nl_cache_destroy_and_free (linux_netlink_link_cache);
 }

 if (linux_netlink_callbacks) {
  nl_cb_destroy (linux_netlink_callbacks);
 }

 return 0;
}

void *linux_netlink_read_thread (void *irrelevant) {
 while (1) {
  nl_recvmsgs (linux_netlink_handle, linux_netlink_callbacks);
 }

 return NULL;
}

int linux_netlink_main_callback (struct nl_msg *message, void *args) {
 notice (1, "got a netlink message");

 return NL_PROCEED;
}

int linux_netlink_connect() {
 linux_netlink_handle = nl_handle_alloc();
 nl_handle_set_pid(linux_netlink_handle, getpid());
 nl_disable_sequence_check(linux_netlink_handle);

 if ((linux_netlink_connected = (nl_connect(linux_netlink_handle, NETLINK_ROUTE) == 0))) {
  if ((linux_netlink_callbacks = nl_cb_new (NL_CB_DEFAULT))) {
   nl_cb_set_all(linux_netlink_callbacks, NL_CB_DEFAULT, linux_netlink_main_callback, NULL);
  }
 }

 return linux_netlink_connected;
}

int linux_netlink_configure (struct lmodule *irr) {
// pthread_t thread;

 module_init (irr);

 thismodule->cleanup = linux_netlink_cleanup;

 if (!linux_netlink_connect()) {
  notice (2, "eINIT <-> NetLink: could not connect");
  perror ("netlink NOT connected (%s)");
 } else {
  notice (2, "eINIT <-> NetLink: connected");
//  ethread_create (&thread, &thread_attribute_detached, linux_netlink_read_thread, NULL);

  linux_netlink_get_interface_data ("eth1");

  linux_netlink_get_all_interfaces();
  char **ifs = linux_netlink_get_all_interfaces();

  if (ifs) {
   char *n = set2str (' ', (const char **)ifs);

   notice (2, "found the following interfaces: (%s)", n);

   if (n) {
    free (n);
   }

   int xr = 0;
   for (; ifs[xr]; xr++) {
    struct network_interface *ifd = linux_netlink_get_interface_data (ifs[xr]);

    if (ifd) {
     notice (2, "network interface: %s, flags=%i", ifd->name, ifd->flags);

     free (ifd);
    }
   }
  } else {
   notice (2, "error retrieving list of interfaces");
  }
 }

 return 0;
}

/* passive module... */
