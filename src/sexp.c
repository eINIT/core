/*
 *  sexp.c
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <einit/sexp.h>
#include <einit/einit.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#define MIN_CHUNK_SIZE      1024
#define DEFAULT_BUFFER_SIZE (MIN_CHUNK_SIZE * 4)

#define MAX_SYMBOL_SIZE 128
#define MAX_NUMBER_SIZE 32
#define MAX_STRING_SIZE 4096

enum einit_sexp_read_type { esr_string, esr_number, esr_symbol };

static const struct einit_sexp sexp_nil_v = {.type = es_nil };
static const struct einit_sexp sexp_true_v = {.type = es_boolean_true };
static const struct einit_sexp sexp_false_v = {.type = es_boolean_false };
static const struct einit_sexp sexp_end_of_list_v = {.type = es_list_end };
static const struct einit_sexp sexp_empty_list_v = {.type = es_empty_list
};
static const struct einit_sexp sexp_bad_v = {.type = es_bad };

const struct einit_sexp *const sexp_nil = &sexp_nil_v;
const struct einit_sexp *const sexp_true = &sexp_true_v;
const struct einit_sexp *const sexp_false = &sexp_false_v;
const struct einit_sexp *const sexp_end_of_list = &sexp_end_of_list_v;
const struct einit_sexp *const sexp_empty_list = &sexp_empty_list_v;
const struct einit_sexp *const sexp_bad = &sexp_bad_v;

static struct einit_sexp *einit_parse_string_in_buffer(char *buffer,
                                                       int *index,
                                                       int stop)
{
    unsigned char sc_quote = 0;
    char stbuffer[MAX_STRING_SIZE];
    int sb_pos = 0;


    for (; (*index) < stop; (*index)++) {
        char skip = 0;

        if (sc_quote) {
            if (sb_pos < (MAX_STRING_SIZE - 1)) {
                stbuffer[sb_pos] = buffer[*index];
                sb_pos++;
            }
            sc_quote = 0;

            continue;
        }

        switch (buffer[*index]) {
        case '\\':
            sc_quote = 1;
            skip = 1;
            break;
        case '"':
            stbuffer[sb_pos] = 0;

            struct einit_sexp *rv = einit_sexp_create(es_string);
            rv->string = str_stabilise(stbuffer);

            (*index)++;

            return rv;
            break;

        default:
            break;
        }

        if (skip) continue;

        if (sb_pos < (MAX_STRING_SIZE - 1)) {
            stbuffer[sb_pos] = buffer[*index];
            sb_pos++;
        }
    }

    return NULL;
}

static struct einit_sexp *einit_parse_symbol_in_buffer(char *buffer,
                                                       int *index,
                                                       int stop)
{
    char stbuffer[MAX_SYMBOL_SIZE];
    int sb_pos = 0;

    for (; (*index) < stop; (*index)++) {
        if ((buffer[*index] == '(') || (buffer[*index] == ')')
            || !(buffer[*index]) || isspace(buffer[*index])) {
            stbuffer[sb_pos] = 0;
            struct einit_sexp *rv;

            if (stbuffer[0] == '#') {
                switch (stbuffer[1]) {
                case 't':
                    return (struct einit_sexp *) sexp_true;
                case 'f':
                    return (struct einit_sexp *) sexp_false;
                default:
                    return (struct einit_sexp *) sexp_nil;
                }
            } else {
                rv = einit_sexp_create(es_symbol);
                rv->symbol = str_stabilise(stbuffer);

                return rv;
            }
        }

        if (sb_pos < (MAX_SYMBOL_SIZE - 1)) {
            stbuffer[sb_pos] = buffer[*index];
            sb_pos++;
        }
    }

    return NULL;
}


static struct einit_sexp *einit_parse_number_in_buffer(char *buffer,
                                                       int *index,
                                                       int stop)
{
    char stbuffer[MAX_NUMBER_SIZE];
    int sb_pos = 0;

    for (; (*index) < stop; (*index)++) {
        if ((buffer[*index] == '(') || (buffer[*index] == ')')
            || !(buffer[*index]) || isspace(buffer[*index])) {
            stbuffer[sb_pos] = 0;
            struct einit_sexp *rv;

            rv = einit_sexp_create(es_integer);
            rv->integer = atoi(stbuffer);

            return rv;
        }

        if (sb_pos < (MAX_NUMBER_SIZE - 1)) {
            stbuffer[sb_pos] = buffer[*index];
            sb_pos++;
        }
    }

    return NULL;
}


static struct einit_sexp *einit_parse_sexp_in_buffer(char *buffer,
                                                     int *index, int stop)
{
    struct einit_sexp *rv = NULL;

    // fprintf (stderr, "parsing buffer\n");

    for (; (*index) < stop; (*index)++) {
        if ((buffer[*index]) && !isspace(buffer[*index])) {
            if (isdigit(buffer[*index])) {
                return einit_parse_number_in_buffer(buffer, index, stop);
            }

            switch (buffer[*index]) {
            case '"':
                (*index)++;
                return einit_parse_string_in_buffer(buffer, index, stop);

            case ')':
                (*index)++;

                return (struct einit_sexp *) sexp_end_of_list;
                break;

            case '(':
                // fprintf (stderr, "got start-of-list\n");
                (*index)++;

                struct einit_sexp *tmp =
                    einit_parse_sexp_in_buffer(buffer, index, stop);
                if (!tmp)
                    return NULL;

                if (tmp != sexp_end_of_list) {
                    rv = einit_sexp_create(es_cons);
                    rv->primus = tmp;

                    struct einit_sexp *ccons = rv;

                    do {
                        tmp =
                            einit_parse_sexp_in_buffer(buffer, index,
                                                       stop);

                        if (!tmp) {     /* catch incompletely read sexprs */
                            ccons->secundus =
                                (struct einit_sexp *) sexp_end_of_list;
                            einit_sexp_destroy(rv);
                            return NULL;
                        }

                        if (tmp != sexp_end_of_list) {
                            ccons->secundus = einit_sexp_create(es_cons);
                            ccons = ccons->secundus;

                            if (tmp == sexp_empty_list)
                                tmp =
                                    (struct einit_sexp *) sexp_end_of_list;

                            ccons->primus = tmp;
                        } else {
                            ccons->secundus = tmp;

                            return rv;
                        }
                    } while (1);
                } else {
                    tmp = (struct einit_sexp *) sexp_empty_list;
                }

                return tmp;

                break;
            default:
                return einit_parse_symbol_in_buffer(buffer, index, stop);
            }
        }

        continue;
    }

    return NULL;
}

static int einit_read_sexp_from_fd_reader_fill_buffer(struct
                                                      einit_sexp_fd_reader
                                                      *reader)
{
    if ((reader->size - reader->position) < MIN_CHUNK_SIZE) {
        reader->size += MIN_CHUNK_SIZE;

        reader->buffer = erealloc(reader->buffer, reader->size);
        // fprintf (stderr, "buffer size: %i\n", reader->size);
    }
    // fprintf (stderr, "reading from fd: %i\n", reader->fd);

    int rres = read(reader->fd, (reader->buffer + reader->position),
                    (reader->size - reader->position));

    // fprintf (stderr, "result: %i\n", rres);

    if (rres > 0) {
        reader->position += rres;

        return einit_read_sexp_from_fd_reader_fill_buffer(reader);
    }

    return rres;
}

struct einit_sexp *einit_read_sexp_from_fd_reader(struct
                                                  einit_sexp_fd_reader
                                                  *reader)
{
    int ppos = 0;
    int rres = einit_read_sexp_from_fd_reader_fill_buffer(reader);

    if (((((rres == -1) && ((errno == EAGAIN) || (errno == EINTR)))
          || (rres == 0))) && (reader->position)) {
        ppos = 0;

        struct einit_sexp *rv =
            einit_parse_sexp_in_buffer(reader->buffer, &ppos,
                                       reader->position);

        if (rv) {
            reader->position -= ppos;

            if (reader->position) {
                memmove(reader->buffer, reader->buffer + ppos,
                        reader->position);
            }

            if (rv == sexp_empty_list)
                rv = (struct einit_sexp *) sexp_end_of_list;

            return rv;
        }
    }

    for (ppos = 0;
         (ppos < reader->position) && (!(reader->buffer[ppos])
                                       || isspace(reader->buffer[ppos]));
         ppos++);
    if (ppos == reader->position) {
        reader->position = 0;
    }
    // fprintf (stderr, "ppos: %i, rpos: %i; b[%c, %c, %c]\n", ppos,
    // reader->position, reader->buffer[0], reader->buffer[1],
    // reader->buffer[2]);

    if (((rres == -1) && (errno != EAGAIN) && (errno != EINTR))
        || ((rres == 0) && (reader->position == 0))) {
        close(reader->fd);

        efree(reader->buffer);
        efree(reader);

        return (struct einit_sexp *) sexp_bad;
    }

    return NULL;
}

void einit_sexp_display(struct einit_sexp *sexp)
{
    char *x = einit_sexp_to_string(sexp);
    if (x) {
        fprintf(stdout, "%s\n", x);
        efree(x);
    }
}

void einit_sexp_destroy(struct einit_sexp *sexp)
{
    if ((sexp != sexp_nil)
        && (sexp != sexp_true)
        && (sexp != sexp_false)
        && (sexp != sexp_end_of_list)
        && (sexp != sexp_empty_list)
        && (sexp != sexp_bad)) {
        switch (sexp->type) {
        case es_cons:
            einit_sexp_destroy(sexp->primus);
            einit_sexp_destroy(sexp->secundus);
        default:
            efree(sexp);
        }
    }
}

struct einit_sexp *einit_sexp_create(enum einit_sexp_type type)
{
    switch (type) {
    case es_nil:
        return (struct einit_sexp *) sexp_nil;
    case es_boolean_true:
        return (struct einit_sexp *) sexp_true;
    case es_boolean_false:
        return (struct einit_sexp *) sexp_false;
    case es_list_end:
        return (struct einit_sexp *) sexp_end_of_list;
    case es_empty_list:
        return (struct einit_sexp *) sexp_empty_list;
    default:
        {
            struct einit_sexp *sexp =
                ecalloc(1, sizeof(struct einit_sexp));
            sexp->type = type;

            return sexp;
        }
    }
}

struct einit_sexp_fd_reader *einit_create_sexp_fd_reader(int fd)
{
    struct einit_sexp_fd_reader *reader =
        emalloc(sizeof(struct einit_sexp_fd_reader));

    reader->fd = fd;
    reader->buffer = emalloc(DEFAULT_BUFFER_SIZE);
    reader->size = DEFAULT_BUFFER_SIZE;
    reader->position = 0;
    reader->length = 0;

    fcntl(fd, F_SETFL, O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    return reader;
}

static void *sexp_chunk_realloc(void *p, size_t s, size_t * cs)
{
    size_t ns = ((s / MIN_CHUNK_SIZE) + 1) * MIN_CHUNK_SIZE;
    if (ns != (*cs)) {
        *cs = ns;
        return erealloc(p, ns);
    } else {
        return p;
    }
}

/*
 * sexp -> string conversion 
 */

static void einit_sexp_to_string_iterator(struct einit_sexp *sexp,
                                          char **buffer, int *len,
                                          int *pos, char in_list,
                                          size_t * cs)
{
    int i;
    char *symbuffer = NULL;
    char numbuffer[33];

    switch (sexp->type) {
    case es_string:
        *len += strlen(sexp->string) + 2;
        *buffer = sexp_chunk_realloc(*buffer, *len, cs);

        (*buffer)[(*pos)] = '"';

        for (i = 0, (*pos)++; sexp->string[i]; (*pos)++, i++) {
            // fprintf (stderr, "%i, %i, %i, %c\n", *pos, *len, i,
            // sexp->string[i]);

            if ((sexp->string[i] == '"') || (sexp->string[i] == '\\')) {
                (*len)++;
                *buffer = sexp_chunk_realloc(*buffer, *len, cs);

                (*buffer)[(*pos)] = '\\';
                (*pos)++;
            }

            (*buffer)[(*pos)] = sexp->string[i];
        }
        (*buffer)[(*pos)] = '"';
        (*pos)++;

        break;

    case es_cons:
        if (!in_list) {
            *len += 1;
            *buffer = sexp_chunk_realloc(*buffer, *len, cs);

            (*buffer)[(*pos)] = '(';
            (*pos)++;
        }

        einit_sexp_to_string_iterator(sexp->primus, buffer, len, pos, 0,
                                      cs);

        if (sexp->secundus->type != es_list_end) {
            *len += 1;
            *buffer = sexp_chunk_realloc(*buffer, *len, cs);

            (*buffer)[(*pos)] = ' ';
            (*pos)++;
        }

        einit_sexp_to_string_iterator(sexp->secundus, buffer, len, pos, 1,
                                      cs);

        break;

    case es_list_end:
        if (in_list) {
            *len += 1;
            *buffer = sexp_chunk_realloc(*buffer, *len, cs);
            (*buffer)[(*pos)] = ')';
            (*pos)++;
        } else {
            *len += 2;
            *buffer = sexp_chunk_realloc(*buffer, *len, cs);

            (*buffer)[(*pos)] = '(';
            (*pos)++;
            (*buffer)[(*pos)] = ')';
            (*pos)++;
        }

        break;

    case es_boolean_true:
        *len += 2;
        *buffer = sexp_chunk_realloc(*buffer, *len, cs);

        (*buffer)[(*pos)] = '#';
        (*pos)++;
        (*buffer)[(*pos)] = 't';
        (*pos)++;

        break;

    case es_boolean_false:
        *len += 2;
        *buffer = sexp_chunk_realloc(*buffer, *len, cs);

        (*buffer)[(*pos)] = '#';
        (*pos)++;
        (*buffer)[(*pos)] = 'f';
        (*pos)++;

        break;

    case es_integer:
        snprintf(numbuffer, 33, "%d", sexp->integer);
        symbuffer = numbuffer;
        goto print_symbol;

    case es_symbol:
        symbuffer = (char *) sexp->symbol;

      print_symbol:

        *len += strlen(symbuffer);
        *buffer = sexp_chunk_realloc(*buffer, *len, cs);

        for (i = 0; symbuffer[i]; (*pos)++, i++) {
            // fprintf (stderr, "%i, %i, %i, %c\n", *pos, *len, i,
            // symbuffer[i]);

            if (symbuffer[i] == '"') {
                (*len)++;
                *buffer = sexp_chunk_realloc(*buffer, *len, cs);

                (*buffer)[(*pos)] = '\\';
                (*pos)++;
            }

            (*buffer)[(*pos)] = symbuffer[i];
        }

        break;

    case es_nil:
        *len += 4;
        *buffer = sexp_chunk_realloc(*buffer, *len, cs);

        (*buffer)[(*pos)] = '#';
        (*pos)++;
        (*buffer)[(*pos)] = 'n';
        (*pos)++;
        (*buffer)[(*pos)] = 'i';
        (*pos)++;
        (*buffer)[(*pos)] = 'l';
        (*pos)++;

        break;

    case es_empty_list:
        fputs("BAD SEXPR (es_empty_list)\n", stderr);
        break;

    case es_bad:
        fputs("BAD SEXPR!\n", stderr);
        break;
    }
}

char *einit_sexp_to_string(struct einit_sexp *sexp)
{
    char *buffer = emalloc(MIN_CHUNK_SIZE);
    int len = 1;
    int pos = 0;
    size_t cs = MIN_CHUNK_SIZE;

    einit_sexp_to_string_iterator(sexp, &buffer, &len, &pos, 0, &cs);

    buffer[pos] = 0;

    return buffer;
}

struct einit_sexp *se_cons(struct einit_sexp *car, struct einit_sexp *cdr)
{
    struct einit_sexp *r = einit_sexp_create(es_cons);
    r->primus = car;
    r->secundus = cdr;
    return r;
}

struct einit_sexp *se_integer(int i)
{
    struct einit_sexp *r = einit_sexp_create(es_integer);
    r->integer = i;
    return r;
}


struct einit_sexp *se_string(const char *s)
{
    struct einit_sexp *r = einit_sexp_create(es_string);
    if (s)
        r->string = s;
    else
        r->string = "";
    return r;
}

struct einit_sexp *se_symbol(const char *s)
{
    struct einit_sexp *r = einit_sexp_create(es_symbol);
    if (s)
        r->symbol = s;
    else
        r->symbol = "einit-core";
    return r;
}

struct einit_sexp *se_stringset_to_list(char **s)
{
    if (!s)
        return (struct einit_sexp *) sexp_end_of_list;
    int i = 1;
    struct einit_sexp *r = se_cons(se_string(str_stabilise(s[0])),
                                   (struct einit_sexp *) sexp_end_of_list);
    struct einit_sexp *v = r;

    for (; s[i]; i++) {
        v->secundus =
            se_cons(se_string(str_stabilise(s[i])),
                    (struct einit_sexp *) sexp_end_of_list);
        v = v->secundus;
    }

    return r;
}

struct einit_sexp *se_symbolset_to_list(char **s)
{
    if (!s)
        return (struct einit_sexp *) sexp_end_of_list;
    int i = 1;
    struct einit_sexp *r = se_cons(se_symbol(str_stabilise(s[0])),
                                   (struct einit_sexp *) sexp_end_of_list);
    struct einit_sexp *v = r;

    for (; s[i]; i++) {
        v->secundus =
            se_cons(se_symbol(str_stabilise(s[i])),
                    (struct einit_sexp *) sexp_end_of_list);
        v = v->secundus;
    }

    return r;
}
