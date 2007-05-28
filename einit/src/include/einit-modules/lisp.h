/*
 *  lisp.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 27/05/2006.
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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum lisp_node_type {
 lnt_cons,
 lnt_symbol,
 lnt_constant,
 lnt_nil,
 lnt_dot,
};

enum lisp_node_status {
 lns_clear = 0x0000,
 lns_quoted = 0x0001
};

enum lisp_constant_type {
 lct_string,
 lct_float,
 lct_integer,
 lct_true
};

struct lisp_constant {
 enum lisp_constant_type type;

 union {
  char *string;
  double number_float;
  int integer;
 };
};

struct lisp_node {
 enum lisp_node_type type;
 enum lisp_node_status status;

 union {
  struct { /* cons */
   struct lisp_node *primus;
   struct lisp_node *secundus;
  };

  char *symbol;
  
  struct lisp_constant constant;
 };
};

enum lisp_parser_bits {
 lpb_clear = 0x0000,
 lpb_open_single_quotes = 0x0001,
 lpb_open_double_quotes = 0x0002,
 lpb_last_char_was_backspace = 0x0004,
 lpb_cons_dot = 0x0008,
 lpb_comment = 0x0010,
 lpb_next_item_to_cdr = 0x0020,
 lpb_ignorefurther = 0x0040,
 lpb_noeval = 0x0080,
};

struct lisp_parser_state {
 enum lisp_parser_bits status;

 uint32_t open_brackets;
 uint32_t position;
};

typedef struct lisp_node *(*lisp_function) (struct lisp_node *);

#ifdef __cplusplus
}
#endif
