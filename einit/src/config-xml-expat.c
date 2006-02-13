/***************************************************************************
 *            config-xml-expat.c
 *
 *  Mon Feb  6 15:42:42 2006
 *  Copyright  2006  Magnus Deininger
 *  dma05@web.de
 ****************************************************************************/
/*
Copyright (c) 2006, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <expat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"
#include "string.h"

void cfg_xml_handler_tag_start (void *userData, const XML_Char *name, const XML_Char **atts) {
}

void cfg_xml_handler_tag_end (void *userData, const XML_Char *name) {
}

struct sconfiguration *cfg_load () {
 int cfgfd, e, blen;
 char * buf, * data;
 ssize_t rn;
 struct stat *cfgst;
 XML_Parser par;
 if (configfile == NULL) configfile = "/etc/einit/default.xml";
 cfgst = malloc (sizeof (struct stat));
 if (cfgst == NULL) return NULL;
 if (stat (configfile, cfgst) == -1) return NULL;
 cfgfd = open (configfile, O_RDONLY);
// if (cfgfd != -1) {
//  rn = read (cfgfd, cfgst->st_size)
// }
 if (cfgfd != -1) {
  buf = malloc (BUFFERSIZE);
  blen = 0;
  do {
   buf = realloc (buf, blen + BUFFERSIZE);
   if (buf == NULL) return NULL;
   e = read (cfgfd, (char *)(buf + blen), BUFFERSIZE);
   blen = blen + e;
  } while (e > 0);
  close (cfgfd);
 }
 data = realloc (buf, blen);
// *(char *)(data + blen) = 0;
// puts (data);
 par = XML_ParserCreate (NULL);
 if (par != NULL) {
  XML_SetElementHandler (par, cfg_xml_handler_tag_start, cfg_xml_handler_tag_end);
  if (XML_Parse (par, data, blen, 1) == XML_STATUS_ERROR) {
   puts ("cfg_load(): XML_Parse() failed:");
   puts (XML_ErrorString (XML_GetErrorCode (par)));
  }
  XML_ParserFree (par);
 }
 return (void *)1;
}
