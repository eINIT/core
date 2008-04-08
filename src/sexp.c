/*
 *  sexp.c
 *  eINIT
 *
 *  Created by Magnus Deininger on 06/04/2008.
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2008, Magnus Deininger
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

enum einit_sexp_read_type { esr_string, esr_number, esr_symbol };

static struct einit_sexp *einit_parse_sexp_in_buffer_with_buffer (char *buffer, int *index, int stop, enum einit_sexp_read_type buffertype) {
 unsigned char sc_quote = 0;
 char *stbuffer = emalloc(stop - (*index));
 int sb_pos = 0;

 for (; (*index) < stop; (*index)++) {
  if (buffertype == esr_string) {
   if (sc_quote) {
    stbuffer [sb_pos] = buffer[*index];
    sb_pos++;
    sc_quote ^= sc_quote;

    continue;
   }

   switch (buffer[*index]) {
    case '\\': sc_quote ^= sc_quote; break;
    case '"':
     stbuffer[sb_pos] = 0;

     struct einit_sexp *rv = einit_sexp_create(es_string);
     rv->string = str_stabilise (stbuffer);

//   fprintf (stderr, "got string: %s\n", rv->string);
     (*index)++;

     efree (stbuffer);
     return rv;
     break;

    default: break;
   }
  } else {
   if ((buffer[*index] == '(') || (buffer[*index] == ')') || isspace (buffer[*index])) {
    stbuffer[sb_pos] = 0;
    struct einit_sexp *rv;

    switch (buffertype) {
     case esr_number:
      rv = einit_sexp_create (es_integer);
      rv->integer = atoi (stbuffer);
      break;

     case esr_symbol:
      rv = einit_sexp_create (es_symbol);
      rv->symbol = str_stabilise (stbuffer);
      break;

      default: break;
    }

    efree (stbuffer);
    return rv;
   }
  }

  stbuffer[sb_pos] = buffer[*index];
  sb_pos++;
 }

 efree (stbuffer);
 return NULL;
}

static struct einit_sexp *einit_parse_sexp_in_buffer (char *buffer, int *index, int stop) {
 struct einit_sexp *rv = NULL;

 for (; (*index) < stop; (*index)++) {
  if (!isspace (buffer[*index])) {
   if (isdigit (buffer[*index])) {
    return einit_parse_sexp_in_buffer_with_buffer(buffer, index, stop, esr_number);
   }

   switch (buffer[*index]) {
    case '"':
     (*index)++;
     return einit_parse_sexp_in_buffer_with_buffer(buffer, index, stop, esr_string);

    case ')':
     rv = einit_sexp_create(es_list_end);
//    fprintf (stderr, "got end-of-list\n");

     (*index)++;

     return rv;
     break;

    case '(':
//    fprintf (stderr, "got start-of-list\n");
     (*index)++;

     struct einit_sexp *tmp = einit_parse_sexp_in_buffer (buffer, index, stop);
     if (!tmp) return NULL;

     if (tmp->type != es_list_end) {
      rv = einit_sexp_create(es_cons);
      rv->primus = tmp;

      struct einit_sexp *ccons = rv;

      do {
       tmp = einit_parse_sexp_in_buffer (buffer, index, stop);

       if (tmp->type != es_list_end) {
        ccons->secundus = einit_sexp_create(es_cons);
        ccons = ccons->secundus;

        ccons->primus = tmp;
       } else {
        ccons->secundus = tmp;

        return rv;
       }
      } while (1);
     }

     return tmp;

     break;
    default:
     return einit_parse_sexp_in_buffer_with_buffer(buffer, index, stop, esr_symbol);
   }
  }

  continue;
 }

 return NULL;
}

static int einit_read_sexp_from_fd_reader_fill_buffer (struct einit_sexp_fd_reader *reader) {
 if ((reader->size - reader->position) < MIN_CHUNK_SIZE) {
  reader->size += MIN_CHUNK_SIZE;

  reader->buffer = erealloc (reader->buffer, reader->size);
//  fprintf (stderr, "increasing buffer: %i (+%i)\n", reader->size, MIN_CHUNK_SIZE);
 }

// fprintf (stderr, "reading from fd: %i\n", reader->fd);

 int rres = read (reader->fd, (reader->buffer + reader->position), (reader->size - reader->position));

// fprintf (stderr, "result: %i\n", rres);

 if (rres > 0) {
  reader->position += rres;

  return einit_read_sexp_from_fd_reader_fill_buffer (reader);
 }

 return rres;
}

struct einit_sexp *einit_read_sexp_from_fd_reader (struct einit_sexp_fd_reader *reader) {
 int ppos = 0;
 int rres = einit_read_sexp_from_fd_reader_fill_buffer (reader);

 if (((((rres == -1) && ((errno == EAGAIN) || (errno == EINTR))) || (rres == 0))) && (reader->position)) {
  ppos = 0;

  struct einit_sexp *rv = einit_parse_sexp_in_buffer (reader->buffer, &ppos, reader->position);

  if (rv) {
   reader->position -= ppos;

   if (reader->position) {
    memmove (reader->buffer, reader->buffer + ppos, reader->position);
   }
  }

  if (rv) return rv;
 }

 for (ppos = 0; (ppos < reader->position) && isspace (reader->buffer[ppos]); ppos++);
 if (ppos == reader->position) {
  reader->position = 0;
 }

// fprintf (stderr, "ppos: %i, rpos: %i; b[%c, %c, %c]\n", ppos, reader->position, reader->buffer[0], reader->buffer[1], reader->buffer[2]);

 if (((rres == -1) && (errno != EAGAIN) && (errno != EINTR)) ||
     ((rres == 0) && (reader->position == 0))) {
  close (reader->fd);

  efree (reader->buffer);
  efree (reader);

  return BAD_SEXP;
 }

 return NULL;
}

void einit_sexp_display (struct einit_sexp *sexp) {
 char *x = einit_sexp_to_string (sexp);
 if (x) {
  fputs (x, stdout);
  efree (x);
 }
}

void einit_sexp_destroy (struct einit_sexp *sexp) {
 switch (sexp->type) {
  case es_cons:
   einit_sexp_destroy (sexp->primus);
   einit_sexp_destroy (sexp->secundus);
  default:
   efree (sexp);
 }
}

struct einit_sexp * einit_sexp_create (enum einit_sexp_type type) {
 struct einit_sexp * sexp = ecalloc (1, sizeof (struct einit_sexp));
 sexp->type = type;

 return sexp;
}

struct einit_sexp_fd_reader *einit_create_sexp_fd_reader (int fd) {
 struct einit_sexp_fd_reader * reader = emalloc (sizeof (struct einit_sexp_fd_reader));

 reader->fd = fd;
 reader->buffer = emalloc(DEFAULT_BUFFER_SIZE);
 reader->size = DEFAULT_BUFFER_SIZE;
 reader->position = 0;
 reader->length = 0;

 fcntl (fd, F_SETFL, O_NONBLOCK);
 fcntl (fd, F_SETFD, FD_CLOEXEC);

 return reader;
}

/* sexp -> string conversion */

static void einit_sexp_to_string_iterator (struct einit_sexp *sexp, char **buffer, int *len, int *pos, char in_list) {
 int i;
 char *symbuffer = NULL;
 char numbuffer[33];

 switch (sexp->type) {
  case es_string:
   *len += strlen(sexp->string) + 2;
   *buffer = erealloc (*buffer, *len);

   (*buffer)[(*pos)] = '"';

   for (i = 0, (*pos)++; sexp->string[i]; (*pos)++, i++) {
//    fprintf (stderr, "%i, %i, %i, %c\n", *pos, *len, i, sexp->string[i]);

    if (sexp->string[i] == '"') {
     (*len)++;
     *buffer = erealloc (*buffer, *len);

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
    *buffer = erealloc (*buffer, *len);

    (*buffer)[(*pos)] = '(';
    (*pos)++;
   }

   einit_sexp_to_string_iterator (sexp->primus, buffer, len, pos, 0);

   if (sexp->secundus->type != es_list_end) {
    *len += 1;
    *buffer = erealloc (*buffer, *len);

    (*buffer)[(*pos)] = ' ';
    (*pos)++;
   }

   einit_sexp_to_string_iterator (sexp->secundus, buffer, len, pos, 1);

   break;

  case es_list_end:
   if (in_list) {
    *len += 1;
    *buffer = erealloc (*buffer, *len);
    (*buffer)[(*pos)] = ')';
    (*pos)++;
   } else {
    *len += 2;
    *buffer = erealloc (*buffer, *len);

    (*buffer)[(*pos)] = '(';
    (*pos)++;
    (*buffer)[(*pos)] = ')';
    (*pos)++;
   }

   break;

  case es_boolean_true:
   *len += 2;
   *buffer = erealloc (*buffer, *len);

   (*buffer)[(*pos)] = '#';
   (*pos)++;
   (*buffer)[(*pos)] = 't';
   (*pos)++;

   break;

  case es_boolean_false:
   *len += 2;
   *buffer = erealloc (*buffer, *len);

   (*buffer)[(*pos)] = '#';
   (*pos)++;
   (*buffer)[(*pos)] = 'f';
   (*pos)++;

   break;

  case es_integer:
   snprintf (numbuffer, 33, "%d", sexp->integer);
   symbuffer = numbuffer;
   goto print_symbol;

  case es_symbol:
   symbuffer = (char *)sexp->symbol;

   print_symbol:

     *len += strlen(symbuffer);
   *buffer = erealloc (*buffer, *len);

   for (i = 0; symbuffer[i]; (*pos)++, i++) {
//    fprintf (stderr, "%i, %i, %i, %c\n", *pos, *len, i, symbuffer[i]);

    if (symbuffer[i] == '"') {
     (*len)++;
     *buffer = erealloc (*buffer, *len);

     (*buffer)[(*pos)] = '\\';
     (*pos)++;
    }

    (*buffer)[(*pos)] = symbuffer[i];
   }

   break;
 }
}

char *einit_sexp_to_string (struct einit_sexp *sexp) {
 char *buffer = emalloc (1);
 int len = 1;
 int pos = 0;

 einit_sexp_to_string_iterator (sexp, &buffer, &len, &pos, 0);

 buffer[pos] = 0;

 return buffer;
}
