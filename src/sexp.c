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

#define MIN_CHUNK_SIZE      1024
#define DEFAULT_BUFFER_SIZE (MIN_CHUNK_SIZE * 4)

struct einit_sexp **einit_sexp_active_readers = NULL;

struct einit_sexp *einit_parse_sexp_in_buffer (char *buffer, int *index, int stop) {
 for (; *index < stop; *index++) {
 }

 return NULL;
}

struct einit_sexp *einit_read_sexp_from_fd_reader (struct einit_sexp_fd_reader *reader) {
 if ((reader->position - reader->size) < MIN_CHUNK_SIZE) {
  reader->size += MIN_CHUNK_SIZE;

  reader->buffer = erealloc (reader->buffer, reader->size);
 }

 int rres = read (reader->fd, (reader->buffer + reader->position), (reader->position - reader->size));

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
 
}

char *einit_sexp_to_string (struct einit_sexp *sexp) {
 char *buffer = NULL;
 int len = 0;
 int pos = 0;

 einit_sexp_to_string_iterator (sexp, &buffer, &len, &pos);

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
