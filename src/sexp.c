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

#include <einit/sexp.h>
#include <einit/einit.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#define MIN_CHUNK_SIZE      1024
#define DEFAULT_BUFFER_SIZE (MIN_CHUNK_SIZE * 4)

struct einit_sexp **einit_sexp_active_readers = NULL;

struct einit_sexp *einit_parse_sexp_in_buffer (char *buffer, int *index, int stop) {
 unsigned char sc_quote = 0;

 char *stringbuffer = NULL;
 int sb_pos = 0;

 struct einit_sexp *rv = NULL;

 for (; (*index) < stop; (*index)++) {
  if (stringbuffer) {
   if (sc_quote) {
    stringbuffer [sb_pos] = buffer[*index];
    sb_pos++;
    sc_quote ^= sc_quote;

    continue;
   }

   switch (buffer[*index]) {
    case '\\': sc_quote ^= sc_quote; break;
    case '"':
     stringbuffer[sb_pos] = 0;
     stringbuffer = erealloc (stringbuffer, sb_pos+1);

     rv = einit_sexp_create(es_string);
     rv->string = str_stabilise (stringbuffer);
     efree (stringbuffer);

     fprintf (stderr, "got string: %s\n", rv->string);
     (*index)++;

     return rv;
     break;

    default:
     stringbuffer [sb_pos] = buffer[*index];
     sb_pos++;
   }
  }

  if (isspace (buffer[*index])) {
   continue;
  }

  switch (buffer[*index]) {
   case '"':
    stringbuffer = emalloc (stop);
    break;
  }
 }

 if (stringbuffer) efree (stringbuffer);

 return NULL;
}

struct einit_sexp *einit_read_sexp_from_fd_reader (struct einit_sexp_fd_reader *reader) {
 if ((reader->size - reader->position) < MIN_CHUNK_SIZE) {
  reader->size += MIN_CHUNK_SIZE;

  reader->buffer = erealloc (reader->buffer, reader->size);
 }

 int rres = read (reader->fd, (reader->buffer + reader->position), (reader->size - reader->position));

 if (((rres > 0) && (reader->position += rres)) || (((((rres == -1) && (errno == EAGAIN)) || (rres == 0))) && (reader->position))) {
  int ppos = 0;

  struct einit_sexp *rv = einit_parse_sexp_in_buffer (reader->buffer, &ppos, reader->position);

  if (rv) {
   reader->position -= ppos;

   if (reader->position) {
    memmove (reader->buffer, reader->buffer + ppos, reader->position);
   }
  }

  return rv;
 }

 if ((rres == -1) && (errno != EAGAIN)) {
  if (einit_sexp_active_readers)
   einit_sexp_active_readers = (struct einit_sexp **)setdel ((void **)einit_sexp_active_readers, reader);

  efree (reader->buffer);
  efree (reader);

  return BAD_SEXP;
 }

 return NULL;
}

char *einit_sexp_to_string_iterator (struct einit_sexp *sexp, char **buffer, int *len, int *pos) {
 int i;

 switch (sexp->type) {
  case es_string:
   *len += strlen(sexp->string) + 2;
   *buffer = erealloc (*buffer, *len);

   (*buffer)[(*pos)] = '"';

   for (i = 0, (*pos)++; sexp->string[i]; (*pos)++, i++) {
    fprintf (stderr, "%i, %i, %i, %c\n", *pos, *len, i, sexp->string[i]);

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
 }
}

char *einit_sexp_to_string (struct einit_sexp *sexp) {
 char *buffer = emalloc (1);
 int len = 1;
 int pos = 0;

 einit_sexp_to_string_iterator (sexp, &buffer, &len, &pos);

 buffer[pos] = 0;

 return buffer;
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
 return einit_create_sexp_fd_reader_custom (fd, emalloc(DEFAULT_BUFFER_SIZE), DEFAULT_BUFFER_SIZE);
}

struct einit_sexp_fd_reader *einit_create_sexp_fd_reader_custom (int fd, char *buffer, int buffer_size) {
 struct einit_sexp_fd_reader * reader = emalloc (sizeof (struct einit_sexp_fd_reader));

 reader->fd = fd;
 reader->buffer = buffer;
 reader->size = buffer_size;
 reader->position = 0;
 reader->length = 0;

 fcntl (fd, F_SETFL, O_NONBLOCK);
 fcntl (fd, F_SETFD, FD_CLOEXEC);

 return reader;
}
