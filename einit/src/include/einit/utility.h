/*
 *  utility.h
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
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

/*!\file einit/utility.h
 * \brief Utility-Functions
 * \author Magnus Deininger
 *
 * These are functions that should be of use to all modules and eINIT itself.
*/

#ifndef _UTILITY_H
#define _UTILITY_H

/*!\defgroup utilityfunctionsevents eINIT Utility Functions: Events
 * \defgroup utilityfunctionsstrings eINIT Utility Functions: String-manipulation
 * \defgroup utilityfunctionsmem eINIT Utility Functions: Memory-management wrappers
*/
#include <inttypes.h>
#include <einit/event.h>
#include <sys/types.h>
#include <einit/set.h>

/*!\brief Add the variable \b key with \b value to \b environment.
 * \param[in,out] environment the environment to be manipulated
 * \param[in]     key         the name of the variable
 * \param[in]     value       the variable's value
 * \return This will manipulate \b environment by adding \b key = \b value to it. It may free( \b environment ).
 *         You will need to free() the returned set once you're done with it.
 *
 * This is used to add environment variables to an environment. This is basically a wrapper to some set-handling
 * functions, since the environments in posix-ish systems are the same as these string-sets.
*/
char **straddtoenviron (char **environment, char *key, char *value);

char *readfile (char *filename);

/*!\}*/

/*!\ingroup utilityfunctionsmem
 * \{
*/

/*!\brief malloc()-wrapper
 *
 * This is a wrapper around malloc(). Usage and return conditions are exactly the same as for malloc(), except
 * that this function will not fail.
*/
void *emalloc (size_t);

/*!\brief calloc()-wrapper
 *
 * This is a wrapper around calloc(). Usage and return conditions are exactly the same as for calloc(), except
 * that this function will not fail.
*/
void *ecalloc (size_t, size_t);

/*!\brief realloc()-wrapper
 *
 * This is a wrapper around realloc(). Usage and return conditions are exactly the same as for realloc(), except
 * that this function will not fail.
*/
void *erealloc (void *, size_t);

/*!\brief strdup()-wrapper
 *
 * This is a wrapper around strdup(). Usage and return conditions are exactly the same as for strdup(), except
 * that this function will not fail.
*/
char *estrdup (char *);
/*!\}*/

/*!\ingroup utilityfunctionsstrings
 * \{
*/
/*!\brief Remove leading and trailing whitespace from string \b s
 * \param[in,out] s the string to be modified
 * \return This function does not return any value.
 *
 * This function will remove leading and trailing whitespace (spaces, tabs, newlines, carriage returns, etc.)
 * from the string \b s that is given to it. The string will be modified if necessary.
*/
void strtrim (char *s);
/*!\}*/

/*!\ingroup utilityfunctionsevents
 * \{
*/
/*!\brief Submit textual notice
 * \param[in] severity a number that is used to indicate the severity of the message
 * \param[in] message  a string describing something that just happened
 * \return This function does not return any value.
 *
 * This function can be used to log messages, provide debug information or to warn the user if anything weird
 * just happened. \b severity should be a positive integer between 1 and 20, indicating how important the
 * \b message is. a \b severity of 10+ indicates DEBUG information, <=5 indicates that the information should
 * be pointed out to system users, if possible/appropriate.
 * (The aural feedback module will, by default, vocalise all messages with a severity of <=5 and play them on
 * the system's default speakers.)
*/
void notice (unsigned char severity, char *message);

#define fdputs(string,fd) write (fd, string, strlen(string))

/*!\brief Duplicate event structure \b ev
 * \param[in] ev the event structure to be modified
 * \return This function will return a duplicate of the event structure that it was passed, or NULL on error.
 *         Any returned event structure should be dealloced with evdestroy once it becomes obsolete.
 *
 * This function will duplicate and return the event structure \b ev that is passed to it.
*/
struct einit_event *evdup (struct einit_event *ev);

/*!\brief Initialise event structure of type \b type
 * \param[in] type the type of the structure that is to be initalised.
 * \return This function will return a new event structure, or NULL on error.
 *         Any returned event structure should be dealloced with evdestroy once it becomes obsolete.
 *
 * This function will create and return a new event structure of type \b type.
*/
struct einit_event *evinit (uint32_t type);

/*!\brief Destroy event structure \b ev
 * \param[in,out] ev the event to be destroyed.
 * \return This function does not return any value.
 *
 * This function will deallocate all resources that had been used to create the event structure \b ev. After this
 * function has run, \b ev will be invalid and attempting to use it will result in undefined behaviour.
*/
void evdestroy (struct einit_event *ev);
/*!\}*/

/* user/group functions */
int lookupuidgid (uid_t *uid, gid_t *gid, char *user, char *group);

/* parser functions */
signed int parse_integer (char *);
char parse_boolean (char *);

char *apply_variables (char *string, char **env);

char *escape_xml (char *input);

#endif /* _UTILITY_H */
