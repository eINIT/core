/*
 *  module-lisp.c
 *  einit
 *
 *  created on 15/05/2007.
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/bitch.h>
#include <einit/utility.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <einit/configuration.h>
#include <einit-modules/lisp.h>

#define EXPECTED_EIV 1

#if EXPECTED_EIV != EINIT_VERSION
#warning "This module was developed for a different version of eINIT, you might experience problems"
#endif

int module_lisp_configure (struct lmodule *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)

const struct smodule module_lisp_self = {
 .eiversion = EINIT_VERSION,
 .eibuild   = BUILDNUMBER,
 .version   = 1,
 .mode      = einit_module_loader,
 .name      = "Module Support (.lisp)",
 .rid       = "module-lisp",
 .si        = {
  .provides = NULL,
  .requires = NULL,
  .after    = NULL,
  .before   = NULL
 },
 .configure = module_lisp_configure
};

module_register(module_lisp_self);

#endif

char *lisp_node_to_string (struct lisp_node *node) {
 char *tmp = NULL;
 struct lisp_node *tx;
 ssize_t len;

 switch (node->type) {
  case lnt_nil:
   return estrdup ("nil");

  case lnt_cons:
   tx = node;
   do {
    char *tmpp = NULL, *tmps = NULL;
    tmpp = lisp_node_to_string (tx->primus);

    if (tmp) {
     len = strlen (tmp) + strlen (tmpp) + 2;
     tmps = emalloc (len);

     esprintf (tmps, len, "%s %s", tmp, tmpp);

     free (tmpp);
     free (tmp);

     tmp = tmps;
	} else {
     tmp = tmpp;
    }

    if (tx->secundus->type != lnt_cons) {
     if (tx->secundus->type == lnt_nil) {
      len = strlen (tmp) + 3;

      tmpp = emalloc (len);
      esprintf (tmpp, len, "(%s)", tmp);

      free (tmp);

      tmp = tmpp;
     } else {
      tmps = lisp_node_to_string (tx->secundus);

      len = strlen (tmp) + strlen (tmps) + 6;

      tmpp = emalloc (len);
      esprintf (tmpp, len, "(%s . %s)", tmp, tmps);

      free (tmp);
      free (tmps);

      tmp = tmpp;
     }

     return tmp;
    }

    tx = tx->secundus;

   } while (tx->type == lnt_cons);
   return tmp;

  case lnt_constant:
   switch (node->constant.type) {
    case lct_string:
     return estrdup (node->constant.string);
    default:
     return estrdup ("(unknown constant)");
   }

  case lnt_symbol:
   return estrdup (node->symbol);

  default:
   return estrdup ("(unknown node-type)");
 }
}

struct lisp_node *lisp_evaluate (struct lisp_node *node) {
/* struct lisp_node *rv = NULL;

 rv = emalloc (sizeof (struct lisp_node));
 memset (rv, 0, sizeof (struct lisp_node));
 rv->type = lnt_nil;*/

 switch (node->type) {
  case lnt_cons:
   switch (node->primus->type) {
    case lnt_symbol:
     if (strmatch (node->primus->symbol, "list")) {
      return node->secundus;
     } else {
      lisp_function f = function_find_one (node->primus->symbol, 6, NULL);

      if (f) {
       return f (node->secundus);
      } else {
       eprintf (stderr, "function not implemented: %s\n", lisp_node_to_string(node));

       return node;
      }
     }

    default:
     return node;
   }

  default:
   return node;
 }

 return node;
}

struct lisp_node *lisp_sexp_add (struct lisp_node *rn, struct lisp_node *rv) {
 if (rn->type == lnt_cons) {
  rn->secundus = lisp_sexp_add (rn->secundus, rv);
 }

 if (rn->type == lnt_nil) {
  rn->type = lnt_cons;
  rn->primus = rv;
  rn->secundus = emalloc (sizeof (struct lisp_node));
  memset (rn->secundus, 0, sizeof (struct lisp_node));

  rn->secundus->type = lnt_nil;
 }

 return rn;
}

struct lisp_node *lisp_sexp_add_cdr (struct lisp_node *rn, struct lisp_node *rv) {
 if (rn->type == lnt_cons) {
  rn->secundus = lisp_sexp_add_cdr (rn->secundus, rv);
 }

 if (rn->type == lnt_nil) {
  memcpy (rn, rv, sizeof (struct lisp_node));
 }

 return rn;
}

struct lisp_node *lisp_parse_atom (char *data, struct lisp_parser_state *s) {
 char *tmp = emalloc (BUFFERSIZE);
 struct lisp_node *rv = NULL;
 uint32_t n = 0, sbrackets = s->open_brackets;
 enum lisp_parser_bits localbits = lpb_clear;

 rv = emalloc (sizeof (struct lisp_node));
 memset (rv, 0, sizeof (struct lisp_node));
 rv->type = lnt_nil;

 for (; data[s->position]; s->position++) {
  if (s->status & lpb_comment) {
   switch (data[s->position]) {
    case '\n':
	case '\r':
     s->status ^= lpb_comment;
     s->position--;
     break;
   }

   continue;
  }

  if (s->status & lpb_last_char_was_backspace) {
   s->status ^= lpb_last_char_was_backspace;

   switch (data[s->position]) {
    case 'r':
     tmp[n] = '\r';
     break;

    case 't':
     tmp[n] = '\t';
     break;

    case 'v':
     tmp[n] = '\v';
     break;

    case 'n':
     tmp[n] = '\n';
     break;

    default:
     tmp[n] = data[s->position];
     break;
   }

   if ((n % BUFFERSIZE) == 2) {
    tmp = erealloc (tmp, n+2+BUFFERSIZE);
   }

   n++;

   continue;
  }

  if (data[s->position] == '\\') {
   s->status |= lpb_last_char_was_backspace;
   continue;
  }

  if (s->status & lpb_open_double_quotes) {
   if (data[s->position] == '"') {
    tmp[n] = 0;
    s->status ^= lpb_open_double_quotes;

//    eprintf (stderr, " >> have string: %s\n", tmp);

    rv->type = lnt_constant;
    memset (&(rv->constant), 0, sizeof (struct lisp_constant));

    rv->constant.type = lct_string;
	rv->constant.string = estrdup (tmp);

    free (tmp);

    return rv;
   }

   tmp[n] = data[s->position];

   if ((n % BUFFERSIZE) == 2) {
    tmp = erealloc (tmp, n+2+BUFFERSIZE);
   }

   n++;
  } else switch (data[s->position]) {
   case '"':
    s->status |= lpb_open_double_quotes;
    break;

   case '(':
    s->open_brackets++;

    do {
     struct lisp_node *node;
     s->position++;

     node = lisp_parse_atom (data, s);

     if (sbrackets != s->open_brackets) {
      if (localbits & lpb_ignorefurther) {
       continue;
      }

      if (localbits & lpb_next_item_to_cdr) {
       rv = lisp_sexp_add_cdr (rv, node);

       localbits ^= lpb_next_item_to_cdr;
       localbits |= lpb_ignorefurther;

       continue;
      }

      if (node->type == lnt_dot) {
       localbits |= lpb_next_item_to_cdr;
       localbits |= lpb_noeval;

       continue;
      }

      rv = lisp_sexp_add (rv, node);
     }
    } while ((sbrackets != s->open_brackets) && data[s->position]);

    if (localbits & (lpb_open_single_quotes | lpb_noeval))
     return rv;
    else {
// evaluate arguments
     struct lisp_node *arg = rv->secundus;

     while (arg->type == lnt_cons) {
      arg->primus = lisp_evaluate (arg->primus);

      arg = arg->secundus;
     }

     return lisp_evaluate (rv);
    }

   case ';':
    s->status |= lpb_comment;
    break;

   case ')':
    s->open_brackets--;

   case '\t':
   case '\n':
   case '\v':
   case '\r':
   case ' ':
    if (n > 0) {
     tmp[n] = 0;
//     eprintf (stderr, " >> have symbol: %s\n", tmp);

     if (strmatch (tmp, ".")) {
      rv->type = lnt_dot;
     } else if (strmatch (tmp, "nil")) {
      rv->type = lnt_nil;
     } else {
      rv->type = lnt_symbol;
      rv->symbol = estrdup (tmp);
     }

     free (tmp);

     if (data[s->position] == ')') {
	  s->open_brackets++;
	  s->position--;
     }

     return rv;
	}

    if (data[s->position] == ')') return rv;
    break;

   case '\'':
    if (!n) {
     localbits |= lpb_open_single_quotes;
     break;
    }

   default:
    tmp[n] = data[s->position];

    if ((n % BUFFERSIZE) == 2) {
     tmp = erealloc (tmp, n+2+BUFFERSIZE);
    }

    n++;
	break;
  }
 }

 return rv;
}

struct lisp_node *lisp_parse (char *data) {
 struct lisp_parser_state ls;
 uint32_t l = strlen (data);

 memset (&ls, 0, sizeof (struct lisp_parser_state));

 ls.status = lpb_clear;

// notice (4, "got this lisp file: %s", data);

 if (data) {
  struct lisp_node *n;

  n = emalloc (sizeof (struct lisp_node));
  memset (n, 0, sizeof (struct lisp_node));
  n->type = lnt_nil;

  while (ls.position < l) {
   n = lisp_sexp_add (n, lisp_evaluate(lisp_parse_atom (data, &ls)));
   ls.position++;
  }

  return n;

//  notice (4, "parsed and evaluated to this: %s", lisp_node_to_string(n));
 }

 return NULL;
}

void lisp_free (struct lisp_node *node) {
}

int module_lisp_scanmodules (struct lmodule *);

int module_lisp_scanmodules ( struct lmodule *modchain ) {
 char **modules = NULL;

 modules = readdirfilter(cfg_getnode ("subsystem-lisp-import", NULL), 
                         "/lib/einit/modules-lisp/", "(\\.e?lisp)$", NULL, 0);

 if (modules) {
  uint32_t i = 0;

  for (; modules[i]; i++) {
   char *filecontents = readfile (modules[i]);

   if (filecontents) {
    struct lisp_node *lisp = lisp_parse (filecontents);

	lisp_free (lisp);
   }
  }
 }

 return 1;
}

struct lisp_node *lisp_function_print (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct lisp_node *t = node;
 char ps = 0;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_nil;

// struct lisp_node *rnode = node;

 while (t->type != lnt_nil) {
  if (t->type == lnt_cons) {
   char *s = lisp_node_to_string(t->primus);

   if (ps) {
    eputs (" ", stderr);
   } else
    ps = 1;
   eputs (s, stderr);

   free (s);
  } else {
   char *s = lisp_node_to_string(t);
   eputs (s, stderr);
   free (s);

   eputs ("\n", stderr);

   return rnode;
  }

  t = t->secundus;
 }

 eputs ("\n", stderr);

 return rnode;
}

struct lisp_node *lisp_function_dump (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 char *s = lisp_node_to_string(node);

 eprintf (stderr, "%s\n", s);

 free (s);

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_nil;

 return rnode;
}

struct lisp_node *lisp_function_car (struct lisp_node *node) {
 if ((node->type == lnt_cons) && (node->primus->type == lnt_cons)) {
  return node->primus->primus;
 } else {
  struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));

  memset (rnode, 0, sizeof (struct lisp_node));
  rnode->type = lnt_nil;

  return rnode;
 }
}

struct lisp_node *lisp_function_cdr (struct lisp_node *node) {
 if ((node->type == lnt_cons) && (node->primus->type == lnt_cons)) {
  return node->primus->secundus;
 } else {
  struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));

  memset (rnode, 0, sizeof (struct lisp_node));
  rnode->type = lnt_nil;

  return rnode;
 }
}

int module_lisp_cleanup (struct lmodule *pa) {
 function_unregister ("car", 6, lisp_function_car);
 function_unregister ("cdr", 6, lisp_function_cdr);
 function_unregister ("dump", 6, lisp_function_dump);
 function_unregister ("print", 6, lisp_function_print);

 return 0;
}

int module_lisp_configure (struct lmodule *pa) {
 module_init (pa);

 pa->scanmodules = module_lisp_scanmodules;
 pa->cleanup = module_lisp_cleanup;

 function_register ("print", 6, lisp_function_print);
 function_register ("dump", 6, lisp_function_dump);
 function_register ("cdr", 6, lisp_function_cdr);
 function_register ("car", 6, lisp_function_car);

 return 0;
}
