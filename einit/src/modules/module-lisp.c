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
#include <ctype.h>

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

char **lisp_have_modules = NULL;

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
     len = strlen(node->constant.string) + 3;
     tmp = emalloc (len);

     esprintf (tmp, len, "\"%s\"", node->constant.string);

     return tmp;

    case lct_float:
     tmp = emalloc (BUFFERSIZE);

     esprintf (tmp, BUFFERSIZE, "%g", node->constant.number_float);

     return tmp;

    case lct_integer:
     tmp = emalloc (BUFFERSIZE);

     esprintf (tmp, BUFFERSIZE, "%i", node->constant.integer);

     return tmp;

    default:
     return estrdup ("(unknown constant)");
   }

  case lnt_symbol:
   return estrdup (node->symbol);

  default:
   return estrdup ("(unknown node-type)");
 }
}

struct lisp_node *lisp_evaluate_special (struct lisp_node *node) {
 if (node->type == lnt_cons) {
  char *name = (node->primus->type == lnt_symbol) ? node->primus->symbol : NULL;

  if (name) {
   struct lisp_node *args = node->secundus;

   if (strmatch ("list", name)) {
    struct lisp_node *arg = args;
    while (arg->type == lnt_cons) {
     arg->primus = lisp_evaluate (arg->primus);

     if (arg->secundus != lnt_cons) {
      arg->secundus = lisp_evaluate (arg->secundus);
     }

     arg = arg->secundus;
    }

    return args;
   } else if (strmatch ("eval", name)) {
    struct lisp_node *arg = args;
    while (arg->type == lnt_cons) {
     if (arg->primus->status & lns_quoted) {
      arg->primus->status ^= lns_quoted;
     }
     arg->primus = lisp_evaluate (arg->primus);

     if (arg->secundus != lnt_cons) {
      arg->secundus = lisp_evaluate (arg->secundus);
     }

     arg = arg->secundus;
    }

    return args;
   } else if (strmatch ("defvar", name)) {
    if ((args->type == lnt_cons) && (args->primus->type == lnt_symbol) &&
	    (args->secundus->type == lnt_cons)) {
     char *symbol = args->primus->symbol;
     struct stree *sv = streefind (node->stack->variables, symbol, tree_find_first);

     if (!sv) {
      node->stack->variables =
       streeadd (node->stack->variables, symbol, lisp_evaluate(args->secundus->primus), SET_NOALLOC, NULL);
     }
     return args->primus;
    }

    return NULL;
   } else if (strmatch ("setf", name)) {
    if ((args->type == lnt_cons) && (args->primus->type == lnt_symbol) &&
	    (args->secundus->type == lnt_cons)) {
     char *symbol = args->primus->symbol;
     struct lisp_node *val = lisp_evaluate (args->secundus->primus);
     struct stree *sv = streefind (node->stack->variables, symbol, tree_find_first);

     if (sv) {
      sv->value = val;
     } else {
      node->stack->variables =
       streeadd (node->stack->variables, symbol, val, SET_NOALLOC, NULL);
     }

	 return val;
    }

    return NULL;
   }
  }
 }

 return NULL;
}

struct lisp_node *lisp_evaluate (struct lisp_node *node) {
 struct lisp_node *arg = node->secundus;
 if (node->status & lns_quoted) return node;
 struct lisp_node *tar = NULL;
 struct lisp_stack_frame *stack = node->stack;

 switch (node->type) {
  case lnt_cons:
   switch (node->primus->type) {
    case lnt_symbol:
     if ((tar = lisp_evaluate_special (node))) {
      return tar;
	 } else {
      arg = node->secundus;

// evaluate arguments
      while (arg->type == lnt_cons) {
       arg->primus = lisp_evaluate (arg->primus);

       arg = arg->secundus;
      }

      lisp_function f = function_find_one (node->primus->symbol, 6, NULL);

      if (f) {
       return f (node->secundus);
      } else {
       eprintf (stderr, "function not implemented: %s\n", lisp_node_to_string(node));

       return node;
      }
     }

     return node;

    default:
     arg = node;
     while (arg->type == lnt_cons) {
      arg->primus = lisp_evaluate (arg->primus);

      if (arg->secundus != lnt_cons) {
       arg->secundus = lisp_evaluate (arg->secundus);
      }

      arg = arg->secundus;
     }

     return node;
   }

  case lnt_symbol:
   while (stack) {
    struct stree *sv = streefind (stack->variables, node->symbol, tree_find_first);
    if (sv) return sv->value;

    stack = stack->up;
   }
   break;

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
  enum lisp_node_status e = (rn->status & lns_quoted);

  rn->type = lnt_cons;
  rn->primus = rv;
  rn->secundus = emalloc (sizeof (struct lisp_node));
  memset (rn->secundus, 0, sizeof (struct lisp_node));

  rn->stack = rv->stack;

  rn->secundus->type = lnt_nil;
  rn->secundus->stack = rn->stack;

  rn->status |= e;
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
 rv->stack = s->stack;

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

    if (!s->open_brackets && !(rv->status & lns_quoted)) {
     return lisp_evaluate (rv);
    } else
	 return rv;

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

     if (strmatch (tmp, ".")) {
      rv->type = lnt_dot;
     } else if (strmatch (tmp, "nil")) {
      rv->type = lnt_nil;
     } else {
      uint32_t k = 0, num = 0, alpha = 0, dec = 0, sign;

      for (; tmp[k]; k++) {
       if (isdigit (tmp[k])) {
        num++;
       } else if ((tmp[k] == '+') || (tmp[k] == '-')) {
        sign++;
       } else if (tmp[k] == '.') {
        dec++;
       } else if (isalpha (tmp[k])) {
        alpha++;
       }
      }

      if (alpha) {
       rv->type = lnt_symbol;
       rv->symbol = estrdup (tmp);
      } else if (dec && num) {
       rv->type = lnt_constant;
       memset (&(rv->constant), 0, sizeof (struct lisp_constant));

       rv->constant.type = lct_float;
       rv->constant.number_float = strtod (tmp, (char **)NULL);
      } else if (num) {
       rv->type = lnt_constant;
       memset (&(rv->constant), 0, sizeof (struct lisp_constant));

       rv->constant.type = lct_integer;
       rv->constant.integer = strtol (tmp, (char **)NULL, 10);
      } else {
       rv->type = lnt_symbol;
       rv->symbol = estrdup (tmp);
      }
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
     rv->status |= lns_quoted;

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
 ls.stack = emalloc (sizeof (struct lisp_stack_frame));

 memset (ls.stack, 0, sizeof (struct lisp_stack_frame));

// notice (4, "got this lisp file: %s", data);

 if (data) {
  struct lisp_node *n;

  n = emalloc (sizeof (struct lisp_node));
  memset (n, 0, sizeof (struct lisp_node));
  n->type = lnt_nil;
  n->stack = ls.stack;

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
                         "/lib/einit/modules-lisp-bootstrap/", "(\\.e?lisp)$", NULL, 0);

 if (modules) {
  uint32_t i = 0;

  for (; modules[i]; i++) if (!inset ((const void **)lisp_have_modules, modules[i], SET_TYPE_STRING)) {
   char *filecontents = readfile (modules[i]);

   if (filecontents) {
    struct lisp_node *lisp = lisp_parse (filecontents);

	lisp_free (lisp);
   }

   lisp_have_modules = (char **)setadd ((void **)lisp_have_modules, modules[i], SET_TYPE_STRING);
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
 rnode->stack = node->stack;

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
 rnode->stack = node->stack;

 return rnode;
}

struct lisp_node *lisp_function_car (struct lisp_node *node) {
 if ((node->type == lnt_cons) && (node->primus->type == lnt_cons)) {
  return node->primus->primus;
 } else {
  struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));

  memset (rnode, 0, sizeof (struct lisp_node));
  rnode->type = lnt_nil;
  rnode->stack = node->stack;

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
  rnode->stack = node->stack;

  return rnode;
 }
}

struct lisp_node *lisp_function_add (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct lisp_node *t = node;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_constant;
 rnode->stack = node->stack;

 if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
  if (t->primus->constant.type == lct_integer) {
   rnode->constant.type = lct_integer;
   rnode->constant.integer = t->primus->constant.integer;
  } else if (t->primus->constant.type == lct_float) {
   rnode->constant.type = lct_float;
   rnode->constant.number_float = t->primus->constant.number_float;
  } else {
   rnode->constant.type = lct_integer;
   return rnode;
  }
 } else {
  rnode->constant.type = lct_integer;
  return rnode;
 }
 t = t->secundus;

 while (t->type != lnt_nil) {
  if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
   if (t->primus->constant.type == lct_integer) {
    if (rnode->constant.type == lct_integer)
     rnode->constant.integer += t->primus->constant.integer;
    else if (rnode->constant.type == lct_float)
     rnode->constant.number_float += t->primus->constant.integer;
   } else if (t->primus->constant.type == lct_float) {
    if (rnode->constant.type == lct_integer) {
     rnode->constant.type = lct_float;
     rnode->constant.number_float = rnode->constant.integer + t->primus->constant.number_float;
    } else if (rnode->constant.type == lct_float)
     rnode->constant.number_float += t->primus->constant.number_float;
   }
  } else {
   return rnode;
  }

  t = t->secundus;
 }

 return rnode;
}

struct lisp_node *lisp_function_subtract (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct lisp_node *t = node;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_constant;
 rnode->stack = node->stack;

 if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
  if (t->primus->constant.type == lct_integer) {
   rnode->constant.type = lct_integer;
   rnode->constant.integer = t->primus->constant.integer;
  } else if (t->primus->constant.type == lct_float) {
   rnode->constant.type = lct_float;
   rnode->constant.number_float = t->primus->constant.number_float;
  } else {
   rnode->constant.type = lct_integer;
   return rnode;
  }
 } else {
  rnode->constant.type = lct_integer;
  return rnode;
 }
 t = t->secundus;

 while (t->type != lnt_nil) {
  if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
   if (t->primus->constant.type == lct_integer) {
    if (rnode->constant.type == lct_integer)
     rnode->constant.integer -= t->primus->constant.integer;
    else if (rnode->constant.type == lct_float)
     rnode->constant.number_float -= t->primus->constant.integer;
   } else if (t->primus->constant.type == lct_float) {
    if (rnode->constant.type == lct_integer) {
     rnode->constant.type = lct_float;
     rnode->constant.number_float = rnode->constant.integer - t->primus->constant.number_float;
    } else if (rnode->constant.type == lct_float)
     rnode->constant.number_float -= t->primus->constant.number_float;
   }
  } else {
   return rnode;
  }

  t = t->secundus;
 }

 return rnode;
}

struct lisp_node *lisp_function_multiply (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct lisp_node *t = node;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_constant;
 rnode->stack = node->stack;

 if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
  if (t->primus->constant.type == lct_integer) {
   rnode->constant.type = lct_integer;
   rnode->constant.integer = t->primus->constant.integer;
  } else if (t->primus->constant.type == lct_float) {
   rnode->constant.type = lct_float;
   rnode->constant.number_float = t->primus->constant.number_float;
  } else {
   rnode->constant.type = lct_integer;
   return rnode;
  }
 } else {
  rnode->constant.type = lct_integer;
  return rnode;
 }
 t = t->secundus;

 while (t->type != lnt_nil) {
  if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
   if (t->primus->constant.type == lct_integer) {
    if (rnode->constant.type == lct_integer)
     rnode->constant.integer *= t->primus->constant.integer;
    else if (rnode->constant.type == lct_float)
     rnode->constant.number_float *= t->primus->constant.integer;
   } else if (t->primus->constant.type == lct_float) {
    if (rnode->constant.type == lct_integer) {
     rnode->constant.type = lct_float;
     rnode->constant.number_float = rnode->constant.integer * t->primus->constant.number_float;
    } else if (rnode->constant.type == lct_float)
     rnode->constant.number_float *= t->primus->constant.number_float;
   }
  } else {
   return rnode;
  }

  t = t->secundus;
 }

 return rnode;
}

struct lisp_node *lisp_function_divide (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct lisp_node *t = node;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_constant;
 rnode->stack = node->stack;

 if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
  if (t->primus->constant.type == lct_integer) {
   rnode->constant.type = lct_integer;
   rnode->constant.integer = t->primus->constant.integer;
  } else if (t->primus->constant.type == lct_float) {
   rnode->constant.type = lct_float;
   rnode->constant.number_float = t->primus->constant.number_float;
  } else {
   rnode->constant.type = lct_integer;
   return rnode;
  }
 } else {
  rnode->constant.type = lct_integer;
  return rnode;
 }
 t = t->secundus;

 while (t->type != lnt_nil) {
  if ((t->type == lnt_cons) && (t->primus->type == lnt_constant)) {
   if (t->primus->constant.type == lct_integer) {
    if (rnode->constant.type == lct_integer)
     rnode->constant.integer /= t->primus->constant.integer;
    else if (rnode->constant.type == lct_float)
     rnode->constant.number_float /= t->primus->constant.integer;
   } else if (t->primus->constant.type == lct_float) {
    if (rnode->constant.type == lct_integer) {
     rnode->constant.type = lct_float;
     rnode->constant.number_float = rnode->constant.integer / t->primus->constant.number_float;
    } else if (rnode->constant.type == lct_float)
     rnode->constant.number_float /= t->primus->constant.number_float;
   }
  } else {
   return rnode;
  }

  t = t->secundus;
 }

 return rnode;
}

struct lisp_node *lisp_function_set_configuration (struct lisp_node *node) {
 struct lisp_node *rnode = emalloc (sizeof (struct lisp_node));
 struct cfgnode cfg;

 memset (rnode, 0, sizeof (struct lisp_node));
 rnode->type = lnt_nil;
 rnode->stack = node->stack;

 memset (&cfg, 0, sizeof (struct cfgnode));

 if ((node->type == lnt_cons) &&
     (node->primus->type == lnt_constant) &&
     (node->primus->constant.type == lct_string)) {
  struct lisp_node *t = node->secundus;

  cfg.id = estrdup (node->primus->constant.string);

  while (t->type == lnt_cons) {
   if (t->primus->type == lnt_cons) {
    if ((t->primus->primus->type == lnt_constant) &&
        (t->primus->primus->constant.type == lct_string) &&
		(t->primus->secundus->type == lnt_constant) &&
        (t->primus->secundus->constant.type == lct_string)) {

     if (strmatch ("s", t->primus->primus->constant.string)) {
      cfg.svalue = t->primus->secundus->constant.string;
     }

     cfg.arbattrs = (char **)setadd ((void **)cfg.arbattrs, t->primus->primus->constant.string, SET_TYPE_STRING);
     cfg.arbattrs = (char **)setadd ((void **)cfg.arbattrs, t->primus->secundus->constant.string, SET_TYPE_STRING);
    }
   }

   t = t->secundus;
  }
 } else {
  return rnode;
 }

 cfg_addnode (&cfg);

 return rnode;
}

int module_lisp_cleanup (struct lmodule *pa) {
 function_unregister ("car", 6, lisp_function_car);
 function_unregister ("cdr", 6, lisp_function_cdr);
 function_unregister ("dump", 6, lisp_function_dump);
 function_unregister ("print", 6, lisp_function_print);
 function_unregister ("+", 6, lisp_function_add);
 function_unregister ("-", 6, lisp_function_subtract);
 function_unregister ("*", 6, lisp_function_multiply);
 function_unregister ("/", 6, lisp_function_divide);
 function_unregister ("set-configuration", 6, lisp_function_set_configuration);

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
 function_register ("+", 6, lisp_function_add);
 function_register ("-", 6, lisp_function_subtract);
 function_register ("*", 6, lisp_function_multiply);
 function_register ("/", 6, lisp_function_divide);
 function_register ("set-configuration", 6, lisp_function_set_configuration);

 return 0;
}
