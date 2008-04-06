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

#include <einit/sexp.h>
#include <einit/einit.h>

struct einit_sexp *einit_read_sexp_from_fd_reader (struct einit_sexp_fd_reader *reader) {
}

struct einit_sexp *einit_read_sexp_from_buffer (char *buffer, int buffer_size) {
}

char *einit_sexp_to_string (struct einit_sexp *sexp) {
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
 return einit_create_sexp_fd_reader_custom (fd, emalloc(4096), 4096);
}

struct einit_sexp_fd_reader *einit_create_sexp_fd_reader_custom (int fd, char *buffer, int buffer_size) {
 struct einit_sexp_fd_reader * reader = emalloc (sizeof (struct einit_sexp_fd_reader));

 reader->fd = fd;
 reader->buffer = buffer;
 reader->buffer_size = buffer_size;
 reader->buffer_position = 0;
}
