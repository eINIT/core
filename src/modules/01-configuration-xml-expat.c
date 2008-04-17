/*
 *  configuration-xml-expat.c
 *  einit
 *
 *  Created by Magnus Deininger on 06/02/2006.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <expat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <einit/bitch.h>
#include <einit/config.h>
#include <einit/utility.h>
#include <einit/tree.h>
#include <einit/event.h>
#include <dirent.h>
#include <sys/stat.h>
#include <einit-modules/configuration.h>
#include <sys/wait.h>

#include <einit-modules/exec.h>

#define ECXE_MASTERTAG_EINIT   0x00000001
#define ECXE_MASTERTAG_MODULE  0x00000002
#define ECXE_MASTERTAG_NETWORK 0x00000004

#define MODULE_IMPLIED_PREFIX "services-virtual-module"
#define NETWORK_IMPLIED_PREFIX "configuration-network"

#define ECXE_MASTERTAG ( ECXE_MASTERTAG_EINIT | ECXE_MASTERTAG_MODULE | ECXE_MASTERTAG_NETWORK)

int einit_configuration_xml_expat_configure(struct lmodule *);

void einit_config_xml_expat_event_handler_core_update_configuration(struct
                                                                    einit_event
                                                                    *);

#if defined(EINIT_MODULE) || defined(EINIT_MODULE_HEADER)
const struct smodule einit_configuration_xml_expat_self = {
    .eiversion = EINIT_VERSION,
    .eibuild = BUILDNUMBER,
    .version = 1,
    .mode = 0,
    .name = "Configuration Parser (XML, Expat)",
    .rid = "einit-configuration-xml-expat",
    .si = {
           .provides = NULL,
           .requires = NULL,
           .after = NULL,
           .before = NULL},
    .configure = einit_configuration_xml_expat_configure
};

module_register(einit_configuration_xml_expat_self);

#endif

struct cfgnode *curmode = NULL;

char **xml_configuration_files = NULL;
time_t xml_configuration_files_highest_mtime = 0;

char **xml_configuration_new_files = NULL;

struct einit_xml_expat_user_data {
    uint32_t options;
    char *file, *prefix;
    uint32_t adds;
};

void cfg_xml_handler_tag_start(void *userData, const XML_Char * name,
                               const XML_Char ** atts)
{
    int nlen = strlen(name);
    if (!
        (((struct einit_xml_expat_user_data *) userData)->
         options & ECXE_MASTERTAG)) {
        if (strmatch(name, "einit")) {
            uint32_t i = 0;
            ((struct einit_xml_expat_user_data *) userData)->options |=
                ECXE_MASTERTAG_EINIT;

            if (atts) {
                for (; atts[i]; i += 2) {
                    if (!strcmp(atts[i], "prefix")) {
                        ((struct einit_xml_expat_user_data *) userData)->
                            prefix = emalloc(strlen(atts[i + 1]) + 1);
                        *(((struct einit_xml_expat_user_data *) userData)->
                          prefix) = 0;
                        strcat(((struct einit_xml_expat_user_data *)
                                userData)->prefix, atts[i + 1]);
                    }
                }
            }
        } else if (strmatch(name, "module")) {
            ((struct einit_xml_expat_user_data *) userData)->options |=
                ECXE_MASTERTAG_MODULE;

            ((struct einit_xml_expat_user_data *) userData)->prefix =
                emalloc(strlen(MODULE_IMPLIED_PREFIX) + 1);
            *(((struct einit_xml_expat_user_data *) userData)->prefix) = 0;
            strcat(((struct einit_xml_expat_user_data *) userData)->prefix,
                   MODULE_IMPLIED_PREFIX);
        } else if (strmatch(name, "network")) {
            ((struct einit_xml_expat_user_data *) userData)->options |=
                ECXE_MASTERTAG_NETWORK;

            ((struct einit_xml_expat_user_data *) userData)->prefix =
                emalloc(strlen(NETWORK_IMPLIED_PREFIX) + 1);
            *(((struct einit_xml_expat_user_data *) userData)->prefix) = 0;
            strcat(((struct einit_xml_expat_user_data *) userData)->prefix,
                   NETWORK_IMPLIED_PREFIX);
        }

        return;
    }

    if (!((struct einit_xml_expat_user_data *) userData)->prefix) {
        ((struct einit_xml_expat_user_data *) userData)->prefix =
            emalloc(nlen + 1);
        *(((struct einit_xml_expat_user_data *) userData)->prefix) = 0;
    } else {
        int plen =
            strlen(((struct einit_xml_expat_user_data *) userData)->
                   prefix);
        ((struct einit_xml_expat_user_data *) userData)->prefix =
            erealloc(((struct einit_xml_expat_user_data *) userData)->
                     prefix, plen + nlen + 2);
        *((((struct einit_xml_expat_user_data *) userData)->prefix) +
          plen) = '-';
        *((((struct einit_xml_expat_user_data *) userData)->prefix) +
          plen + 1) = 0;
    }
    strcat(((struct einit_xml_expat_user_data *) userData)->prefix, name);

    int i = 0;
    if (strmatch(name, "mode")) {
        /*
         * parse the information presented in the element as a
         * mode-definition 
         */
        struct cfgnode *newnode = ecalloc(1, sizeof(struct cfgnode));
        newnode->type = einit_node_mode;
        newnode->arbattrs = set_str_dup_stable((char **) atts);
        for (; newnode->arbattrs[i] != NULL; i += 2) {
            if (strmatch(newnode->arbattrs[i], "id")) {
                newnode->id =
                    (char *) str_stabilise((char *) newnode->
                                           arbattrs[i + 1]);
            }
            /*
             * else if (strmatch (newnode->arbattrs[i], "base")) {
             * newnode->base = str2set (':', (char
             * *)newnode->arbattrs[i+1]); }
             */
        }
        if (newnode->id) {
            char *id = newnode->id;
            if (cfg_addnode(newnode) != -1)
                ((struct einit_xml_expat_user_data *) userData)->adds++;
            curmode = NULL;
            curmode = cfg_findnode(id, einit_node_mode, curmode);
            efree(newnode);
        }
    } else {
        if (strmatch
            (((struct einit_xml_expat_user_data *) userData)->prefix,
             "core-commands-include-directory")) {
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
        } else
            if (strmatch
                (((struct einit_xml_expat_user_data *) userData)->prefix,
                 "core-commands-include-file")) {
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
            /*
             * parse the information presented in the element as a
             * variable 
             */
            struct cfgnode *newnode = ecalloc(1, sizeof(struct cfgnode));
            newnode->type = einit_node_regular;

            newnode->id = (char *)
                str_stabilise(((struct einit_xml_expat_user_data *)
                               userData)->prefix);
            newnode->mode = curmode;
            newnode->arbattrs = set_str_dup_stable((char **) atts);
            if (newnode->arbattrs)
                for (; newnode->arbattrs[i] != NULL; i += 2) {
                    if (strmatch(newnode->arbattrs[i], "s"))
                        newnode->svalue =
                            (char *) newnode->arbattrs[i + 1];
                    else if (strmatch(newnode->arbattrs[i], "i"))
                        newnode->value =
                            parse_integer(newnode->arbattrs[i + 1]);
                    else if (strmatch(newnode->arbattrs[i], "b")) {
                        newnode->flag =
                            parse_boolean(newnode->arbattrs[i + 1]);
                    }
                }
            if (cfg_addnode(newnode) != -1)
                ((struct einit_xml_expat_user_data *) userData)->adds++;
            efree(newnode);
        }
    }
}

void cfg_xml_handler_tag_end(void *userData, const XML_Char * name)
{
    if (!
        (((struct einit_xml_expat_user_data *) userData)->
         options & ECXE_MASTERTAG))
        return;

    if (strmatch(name, "einit")
        && (((struct einit_xml_expat_user_data *) userData)->
            options & ECXE_MASTERTAG_EINIT)) {
        ((struct einit_xml_expat_user_data *) userData)->options ^=
            ECXE_MASTERTAG_EINIT;
        return;
    } else if (strmatch(name, "module")
               && (((struct einit_xml_expat_user_data *) userData)->
                   options & ECXE_MASTERTAG_MODULE)
               && ((struct einit_xml_expat_user_data *) userData)->prefix
               &&
               strmatch(((struct einit_xml_expat_user_data *) userData)->
                        prefix, MODULE_IMPLIED_PREFIX)) {
        ((struct einit_xml_expat_user_data *) userData)->options ^=
            ECXE_MASTERTAG_MODULE;
        return;
    } else if (strmatch(name, "network")
               && (((struct einit_xml_expat_user_data *) userData)->
                   options & ECXE_MASTERTAG_NETWORK)
               && ((struct einit_xml_expat_user_data *) userData)->prefix
               &&
               strmatch(((struct einit_xml_expat_user_data *) userData)->
                        prefix, NETWORK_IMPLIED_PREFIX)) {
        ((struct einit_xml_expat_user_data *) userData)->options ^=
            ECXE_MASTERTAG_NETWORK;
        return;
    }


    if (((struct einit_xml_expat_user_data *) userData)->prefix) {
        int tlen = strlen(name) + 1;
        char *last =
            strrchr(((struct einit_xml_expat_user_data *) userData)->
                    prefix, 0);
        if ((last - tlen) >
            ((struct einit_xml_expat_user_data *) userData)->prefix)
            *(last - tlen) = 0;
        else {
            efree(((struct einit_xml_expat_user_data *) userData)->prefix);
            ((struct einit_xml_expat_user_data *) userData)->prefix = NULL;
        }
    }

    if (strmatch(name, "mode"))
        curmode = NULL;
}

int einit_config_xml_expat_parse_configuration_file(char *configfile)
{
    static char recursion = 0;
    int blen;
    char *data;
    char *confpath = NULL;
    XML_Parser par;
    struct stat st;

    if (!configfile || stat(configfile, &st))
        return 0;

    struct einit_xml_expat_user_data expatuserdata = {
        .options = 0,
        .prefix = NULL,
        .adds = 0
    };

    if ((data = readfile(configfile))) {
        notice(9, "parsing \"%s\".\n", configfile);

        if (st.st_mtime > xml_configuration_files_highest_mtime)
            xml_configuration_files_highest_mtime = st.st_mtime;

        blen = strlen(data) + 1;
        par = XML_ParserCreate(NULL);
        if (par != NULL) {
            XML_SetUserData(par, (void *) &expatuserdata);
            XML_SetElementHandler(par, cfg_xml_handler_tag_start,
                                  cfg_xml_handler_tag_end);
            if (XML_Parse(par, data, blen - 1, 1) == XML_STATUS_ERROR) {
                uint32_t line = XML_GetCurrentLineNumber(par);
                char **tx = str2set('\n', data);

                notice(2,
                       "einit_config_xml_expat_parse_configuration_file(): XML_Parse():\n * in %s, line %i, character %i\n",
                       configfile, line,
                       (int) XML_GetCurrentColumnNumber(par));
                fprintf(stderr,
                        "einit_config_xml_expat_parse_configuration_file(): XML_Parse():\n * in %s, line %i, character %i\n",
                        configfile, line,
                        (int) XML_GetCurrentColumnNumber(par));

                if (tx) {
                    if (setcount((const void **) tx) >= line) {
                        notice(2, " * offending line:\n%s\n",
                               tx[line - 1]);
                        fprintf(stderr, " * offending line:\n%s\n",
                                tx[line - 1]);
                    }
                    efree(tx);
                }

                bitch(bitch_expat, 0,
                      XML_ErrorString(XML_GetErrorCode(par)));
            }
            if (!inset
                ((const void **) xml_configuration_files,
                 (void *) configfile, SET_TYPE_STRING))
                xml_configuration_files =
                    set_str_add(xml_configuration_files,
                                (void *) configfile);
            XML_ParserFree(par);
        } else {
            bitch(bitch_expat, 0, "XML Parser could not be created");
        }
        efree(data);

        if (!recursion) {
            confpath = cfg_getpath("core-settings-configuration-path");
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
                        einit_config_xml_expat_parse_configuration_file
                            (file);
                        recursion--;
                    } else {
                        char *includefile = joinpath(confpath, file);

                        recursion++;
                        einit_config_xml_expat_parse_configuration_file
                            (includefile);
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
        notice(3, "could not read file \"%s\": %s\n", configfile,
               strerror(errno));

        if (expatuserdata.prefix)
            efree(expatuserdata.prefix);

        return errno;
    }

    if (expatuserdata.prefix)
        efree(expatuserdata.prefix);

    return 1;
}

void einit_config_xml_expat_event_handler_core_update_configuration(struct
                                                                    einit_event
                                                                    *ev)
{
    if (!ev->string && xml_configuration_files) {
        struct stat st;
        uint32_t i = 0;

        for (; xml_configuration_files && xml_configuration_files[i]; i++) {
            stat(xml_configuration_files[i], &st);
            /*
             * see if any files were modified 
             */
            if (st.st_mtime > xml_configuration_files_highest_mtime) {
                /*
                 * need to update configuration 
                 */
                for (i = 0;
                     xml_configuration_files && xml_configuration_files[i];
                     i++) {
                    einit_config_xml_expat_parse_configuration_file
                        (xml_configuration_files[i]);
                }

                goto update;
            }
        }
    }

    if (ev->string) {
        einit_config_xml_expat_parse_configuration_file(ev->string);

        goto update;
    }

    return;

    update:
    {
        struct einit_event se = evstaticinit (einit_event_subsystem_any);
        event_emit (&se, 0);
    }

}

extern char **einit_startup_configuration_files;

int einit_configuration_xml_expat_configure(struct lmodule *this)
{
    module_init(this);
    exec_configure(this);

    event_listen(einit_core_update_configuration,
                 einit_config_xml_expat_event_handler_core_update_configuration);

    /*
     * emit events to read configuration files 
     */
    struct einit_event cev = evstaticinit(einit_core_update_configuration);

    if (einit_startup_configuration_files) {
        uint32_t rx = 0;
        for (; einit_startup_configuration_files[rx]; rx++) {
            cev.string = einit_startup_configuration_files[rx];
            event_emit(&cev, 0);
        }
    }

    evstaticdestroy(cev);

    return 0;
}
