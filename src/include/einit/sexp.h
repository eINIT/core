/*
 *  sexp.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 06/04/2008.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2008, Magnus Deininger All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution. *
 * Neither the name of the project nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS 
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#if ! defined(EINIT_SEXP_H)
#define EINIT_SEXP_H

enum einit_sexp_type {
    es_boolean_true,
    es_boolean_false,
    es_symbol,
    es_string,
    es_integer,
    es_list_end,
    es_cons,
    es_nil,
    es_bad,
    es_empty_list
};

struct einit_sexp {
    enum einit_sexp_type type;

    union {
        struct {                /* for conses */
            struct einit_sexp *primus;
            struct einit_sexp *secundus;
        };

        int integer;

        /*
         * these are required to have been stabilised: 
         */
        const char *string;
        const char *symbol;
    };
};

struct einit_sexp_fd_reader {
    int fd;
    char *buffer;
    int size;
    int position;
    int length;
};

struct einit_sexp *einit_read_sexp_from_fd_reader(struct
                                                  einit_sexp_fd_reader
                                                  *reader);
/*
 * if einit_read_sexp_from_fd_reader() returns sexp_bad, then the reader
 * is dead and free()'d as well 
 */

char *einit_sexp_to_string(struct einit_sexp *sexp);
void einit_sexp_display(struct einit_sexp *sexp);
void einit_sexp_destroy(struct einit_sexp *sexp);
struct einit_sexp *einit_sexp_create(enum einit_sexp_type type);

struct einit_sexp_fd_reader *einit_create_sexp_fd_reader(int fd);

const struct einit_sexp *const sexp_nil;
const struct einit_sexp *const sexp_true;
const struct einit_sexp *const sexp_false;
const struct einit_sexp *const sexp_end_of_list;
const struct einit_sexp *const sexp_empty_list;
const struct einit_sexp *const sexp_bad;

#define se_car(sexp)\
 (struct einit_sexp *)(((sexp)->type == es_cons) ? (sexp)->primus : sexp_nil)
#define se_cdr(sexp)\
 (struct einit_sexp *)(((sexp)->type == es_cons) ? (sexp)->secundus : sexp_nil)

struct einit_sexp *se_cons (struct einit_sexp *car, struct einit_sexp *cdr);
struct einit_sexp *se_integer (int);
struct einit_sexp *se_string (const char *s);
struct einit_sexp *se_symbol (const char *s);
struct einit_sexp *se_stringset_to_list (char **s);

#endif
