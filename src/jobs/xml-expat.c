/*
 *  xml-expat.c
 *  einit
 *
 *  Created by Magnus Deininger on 27/04/2008.
 *  Most of this code was copied from configuration-xml-expat.c
 *  Copyright 2006-2008 Magnus Deininger. All rights reserved.
 *
 */

/*
 * Copyright (c) 2006-2008, Magnus Deininger All rights reserved.
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

#include <einit/einit.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <expat.h>

#define EINIT_ENTRY_CONFIG EINIT_LIB_BASE "/einit.xml"


#define ECXE_MASTERTAG_EINIT   0x00000001
#define ECXE_MASTERTAG_MODULE  0x00000002
#define ECXE_MASTERTAG_NETWORK 0x00000004

#define MODULE_IMPLIED_PREFIX "services-virtual-module"
#define NETWORK_IMPLIED_PREFIX "configuration-network"

#define ECXE_MASTERTAG ( ECXE_MASTERTAG_EINIT | ECXE_MASTERTAG_MODULE | ECXE_MASTERTAG_NETWORK)

int einit_configuration_xml_expat_configure(struct lmodule *);


char **xml_configuration_files = NULL;
char **xml_configuration_new_files = NULL;

struct einit_xml_expat_user_data {
    uint32_t options;
    char *file, *prefix;
    char *mode;
};

void handler_tag_start(void *userData, const XML_Char * name,
                       const XML_Char ** atts)
{
    struct einit_xml_expat_user_data *ud =
        (struct einit_xml_expat_user_data *) userData;

    int nlen = strlen(name);
    if (!(ud->options & ECXE_MASTERTAG)) {
        if (strmatch(name, "einit")) {
            uint32_t i = 0;
            ud->options |= ECXE_MASTERTAG_EINIT;

            if (atts) {
                for (; atts[i]; i += 2) {
                    if (!strcmp(atts[i], "prefix")) {
                        ud->prefix = emalloc(strlen(atts[i + 1]) + 1);
                        *(ud->prefix) = 0;
                        strcat(((struct einit_xml_expat_user_data *)
                                userData)->prefix, atts[i + 1]);
                    }
                }
            }
        } else if (strmatch(name, "module")) {
            ud->options |= ECXE_MASTERTAG_MODULE;

            ud->prefix = emalloc(strlen(MODULE_IMPLIED_PREFIX) + 1);
            *(ud->prefix) = 0;
            strcat(ud->prefix, MODULE_IMPLIED_PREFIX);
        } else if (strmatch(name, "network")) {
            ud->options |= ECXE_MASTERTAG_NETWORK;

            ud->prefix = emalloc(strlen(NETWORK_IMPLIED_PREFIX) + 1);
            *(ud->prefix) = 0;
            strcat(ud->prefix, NETWORK_IMPLIED_PREFIX);
        }

        return;
    }

    int i = 0;
    if (strmatch(name, "mode")) {
        /*
         * parse the information presented in the element as a
         * mode-definition 
         */
        char *idattr = NULL;

        for (; atts[i] != NULL; i += 2) {
            if (strmatch(atts[i], "id")) {
                idattr = (char *) str_stabilise((char *) atts[i + 1]);
            }
        }

        if (idattr) {
            ud->mode = idattr;

            einit_set_configuration("mode", ud->mode, (char **) atts);
        }
    } else {
        if (!ud->prefix) {
            ud->prefix = emalloc(nlen + 1);
            *(ud->prefix) = 0;
        } else {
            int plen = strlen(ud->prefix);
            ud->prefix = erealloc(ud->prefix, plen + nlen + 2);
            *((ud->prefix) + plen) = '-';
            *((ud->prefix) + plen + 1) = 0;
        }
        strcat(ud->prefix, name);

        if (strmatch(ud->prefix, "core-commands-include-directory")) {
            /*
             * we gotta include some extra dir 
             */
            const char *dir = NULL;
            const char *allow = "\\.xml$";
            const char *disallow = NULL;

            if (atts) {
                for (i = 0; atts[i]; i += 2) {
                    if (strmatch(atts[i], "path")) {
                        dir = atts[i + 1];
                    } else if (strmatch(atts[i], "pattern-allow")) {
                        allow = atts[i + 1];
                    } else if (strmatch(atts[i], "pattern-disallow")) {
                        disallow = atts[i + 1];
                    }
                }
            }

            if (dir) {
                char **files =
                    readdirfilter(NULL, dir, allow, disallow, 0);

                if (files) {
                    setsort((void **) files, set_sort_order_string_lexical,
                            NULL);

                    for (i = 0; files[i]; i++) {
                        xml_configuration_new_files =
                            set_str_add(xml_configuration_new_files,
                                        files[i]);
                    }

                    efree(files);
                }
            }
        } else if (strmatch(ud->prefix, "core-commands-include-file")) {
            /*
             * we gotta include some extra file 
             */
            if (atts) {
                for (i = 0; atts[i]; i += 2) {
                    if (strmatch(atts[i], "s")) {
                        xml_configuration_new_files =
                            set_str_add(xml_configuration_new_files,
                                        (char *) atts[i + 1]);
                    }
                }
            }
        } else {
            einit_set_configuration(ud->prefix, ud->mode, (char **) atts);
        }
    }
}

void handler_tag_end(void *userData, const XML_Char * name)
{
    struct einit_xml_expat_user_data *ud =
        (struct einit_xml_expat_user_data *) userData;

    if (!(ud->options & ECXE_MASTERTAG))
        return;

    if (strmatch(name, "einit")
        && (ud->options & ECXE_MASTERTAG_EINIT)) {
        ud->options ^= ECXE_MASTERTAG_EINIT;
        return;
    } else if (strmatch(name, "module")
               && (ud->options & ECXE_MASTERTAG_MODULE)
               && ud->prefix
               && strmatch(ud->prefix, MODULE_IMPLIED_PREFIX)) {
        ud->options ^= ECXE_MASTERTAG_MODULE;
        return;
    } else if (strmatch(name, "network")
               && (ud->options & ECXE_MASTERTAG_NETWORK)
               && ud->prefix
               && strmatch(ud->prefix, NETWORK_IMPLIED_PREFIX)) {
        ud->options ^= ECXE_MASTERTAG_NETWORK;
        return;
    }


    if (ud->prefix) {
        int tlen = strlen(name) + 1;
        char *last = strrchr(ud->prefix, 0);
        if ((last - tlen) > ud->prefix)
            *(last - tlen) = 0;
        else {
            efree(ud->prefix);
            ud->prefix = NULL;
        }
    }

    if (strmatch(name, "mode"))
        ud->mode = NULL;
}

int expat_parse_configuration_file(char *configfile)
{
    static char recursion = 0;
    int blen;
    char *data;
    char *confpath = NULL;
    XML_Parser par;

    if (!configfile)
        return 0;

    struct einit_xml_expat_user_data expatuserdata = {
        .options = 0,
        .prefix = NULL,
        .mode = NULL
    };

    if ((data = readfile(configfile))) {
        fprintf(stdout, "parsing \"%s\".\n", configfile);

        blen = strlen(data) + 1;
        par = XML_ParserCreate(NULL);
        if (par != NULL) {
            XML_SetUserData(par, (void *) &expatuserdata);
            XML_SetElementHandler(par, handler_tag_start, handler_tag_end);
            if (XML_Parse(par, data, blen - 1, 1) == XML_STATUS_ERROR) {
                uint32_t line = XML_GetCurrentLineNumber(par);
                char **tx = str2set('\n', data);

                fprintf(stdout,
                        "expat_parse_configuration_file(): XML_Parse():\n * in %s, line %i, character %i\n",
                        configfile, line,
                        (int) XML_GetCurrentColumnNumber(par));

                if (tx) {
                    if (setcount((const void **) tx) >= line) {
                        fprintf(stdout, " * offending line:\n%s\n",
                                tx[line - 1]);
                    }
                    efree(tx);
                }

                fputs(XML_ErrorString(XML_GetErrorCode(par)), stdout);
            }
            if (!inset
                ((const void **) xml_configuration_files,
                 (void *) configfile, SET_TYPE_STRING))
                xml_configuration_files =
                    set_str_add(xml_configuration_files,
                                (void *) configfile);
            XML_ParserFree(par);
        } else {
            perror("XML Parser could not be created");
        }
        efree(data);

        if (!recursion) {
            confpath =
                einit_get_configuration_string
                ("core-settings-configuration-path", NULL);

            if (!confpath)
                confpath = "/etc/einit/";
            if (coremode & einit_mode_sandbox) {
                if (confpath[0] == '/')
                    confpath++;
            }

            char *file = NULL;

            while (xml_configuration_new_files) {
                if ((file = estrdup(xml_configuration_new_files[0]))) {
                    xml_configuration_new_files =
                        strsetdel(xml_configuration_new_files, file);

                    struct stat st;

                    if ((file[0] == '/') || !stat(file, &st)) {
                        recursion++;
                        expat_parse_configuration_file(file);
                        recursion--;
                    } else {
                        char *includefile = joinpath(confpath, file);

                        recursion++;
                        expat_parse_configuration_file(includefile);
                        recursion--;

                        efree(includefile);
                    }

                    efree(file);
                }
            }
        }

        if (expatuserdata.prefix)
            efree(expatuserdata.prefix);

        return 1;
    } else if (errno) {
        fprintf(stdout, "could not read file \"%s\": %s\n", configfile,
                strerror(errno));

        if (expatuserdata.prefix)
            efree(expatuserdata.prefix);

        return errno;
    }

    if (expatuserdata.prefix)
        efree(expatuserdata.prefix);

    return 1;
}

void config_updated()
{
    struct einit_event se = evstaticinit(einit_core_configuration_update);
    event_emit(&se, einit_event_flag_remote);
}

void update_modules()
{
    struct einit_event se = evstaticinit(einit_core_update_modules);
    event_emit(&se, einit_event_flag_remote);
}

int main(int argc, char **argv)
{
    if (!einit_connect(&argc, argv)) {
        fprintf(stdout, "einit_connect() failed\n");
        return 0;
    }

    expat_parse_configuration_file(EINIT_ENTRY_CONFIG);

    config_updated();
    update_modules();

    return 0;
}
