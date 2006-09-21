/*
 *  utility.h
 *  einit
 *
 *  Created by Magnus Deininger on 25/03/2006.
 *  Copyright 2006 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, Magnus Deininger
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
 * These are functions should be of use to all modules and eINIT itself.
*/

#ifndef _UTILITY_H
#define _UTILITY_H

/*!\defgroup utilityfunctionssets eINIT Utility Functions: Sets
 *!\defgroup utilityfunctionshashes eINIT Utility Functions: Hashes
 *!\defgroup utilityfunctionsevents eINIT Utility Functions: Events
 *!\defgroup utilityfunctionsstrings eINIT Utility Functions: String-manipulation
 *!\defgroup utilityfunctionsmem eINIT Utility Functions: Memory-management wrappers
*/
#include <inttypes.h>
#include <einit/event.h>

/*!\ingroup utilityfunctionssets
 * \{
*/
#define	SET_TYPE_STRING		0    /*!< Set-type: Set consists of strings */
#define	SET_NOALLOC		-1   /*!< Set-type: User takes care of (de-)allocating set members */

#define SORT_SET_STRING_LEXICAL	0x01 /*!< Sort string lexically */
#define SORT_SET_CUSTOM		0xFF /*!< Sort string with a custom sorting function */

/*!\brief Hash-Element
 * \ingroup utilityfunctionshashes
 *
 * This struct represent a hash-element. Note that, for this matter, hashes are merely key/value pairs. The key is always a string, the value can be any pointer.
*/
struct uhash {
 char *key;
 void *value;
 void *luggage;
 struct uhash *next;
};

/*!\brief Combine \b set1 and \b set2.
 * \param[in] set1  the first set
 * \param[in] set2  the second set
 * \param[in] esize Element size; depending on the element-type this should be SET_TYPE_STRING or the
 *                  sizeof() a regular element. You may also specify SET_NOALLOC, which means that you
 *                  will take care of (de-)allocating elements yourself.
 * \return This function returns a new set. It will not free the original sets.
 *
 * Combine \b set1 and \b set2 into a new set which is the returned.
*/
void **setcombine (void **set1, void **set2, int32_t esize);

/*!\brief Add \b item to \b set.
 * \param[in,out] set   the set
 * \param[in]     item  the item to be added
 * \param[in]     esize Element size; depending on the element-type this should be SET_TYPE_STRING or the
 *                      sizeof() a regular element. You may also specify SET_NOALLOC, which means that you
 *                      will take care of (de-)allocating elements yourself.
 * \return This function will modify the set you pass to it. It may free() this set. It may return the same
 *         set unmodified or it may return a completely new set altogether. On error, NULL is returned. You
 *         will only need to free() the resulting set.
 *
 * Add \b item to \b set and return the resulting new set. 
*/
void **setadd (void **set, void *item, int32_t esize);

/*!\brief Duplicate \b set.
 * \param[in] set   the set
 * \param[in] esize Element size; depending on the element-type this should be SET_TYPE_STRING or the
 *                  sizeof() a regular element. You may also specify SET_NOALLOC, which means that you
 *                  will take care of (de-)allocating elements yourself.
 * \return This function will return a duplicate of the set you passed to it, or it will return NULL if
 *         an error occured. You will need to free() this set.
 *
 * Duplicate \b set and return the resulting new set. 
*/
void **setdup (void **set, int32_t esize);

/*!\brief Remove \b item from \b set.
 * \param[in,out] set   the set
 * \param[in]     item  the item to be removed
 * \return This function will modify the set you pass to it. It may free() this set. It may return the same
 *         set unmodified or it may return a completely new set altogether. On error, NULL is returned. If
 *         the resulting set is empty, NULL is returned. You will only need to free() the resulting set.
 * \bug This should be combined with strsetdel() to remove code-duplication.
 *
 * Remove \b item from \b set and return the resulting new set. 
*/
void **setdel (void **set, void *item);

/*!\brief Count elements in \b set.
 * \param[in] set the set
 * \return This function will return the number of elements in the \b set.
 *
 * Counts the number of elements in \b set and returns this number. 
*/
int setcount (void **set);

/*!\brief Sort elements in \b set.
 * \param[in,out] set          the set
 * \param[in]     task         how to sort the set (use the constant SORT_SET_STRING_LEXICAL or SORT_SET_CUSTOM)
 * \param[in]     sortfunction a function that compares set elements. (if you use task=SORT_SET_CUSTOM)
 * \return This function will modify the set you pass to it. It may free() this set. It may return the same
 *         set unmodified or it may return a completely new set altogether. On error, NULL is returned. If
 *         the resulting set is empty, NULL is returned. You will only need to free() the resulting set.
 * \bug broken. Do not use.
 *
 * Sorts the \b set you pass to it, either using the function specified with \b sortfunction, or by what you
 * pass to the function in \b task.
*/
void setsort (void **set, char task, signed int(*sortfunction)(void *, void*));

/*!\brief Find out if element \b needle is in set \b haystack.
 * \param[in] haystack the set
 * \param[in] needle   the element
 * \param[in] esize    Use SET_TYPE_STRING to check if a string is in the set. Use SET_NOALLC to check if a
                       pointer (or any other element) is in the set.
 * \return 1 if the \b needle is in \b haystack, 0 if it is not.
 *
 * Find out if element \b needle is in set \b haystack. Returns 1 if yes, 0 otherwise.
*/
int inset (void **haystack, const void *needle, int32_t esize);

/*!\brief Convert \b input into a set of strings, using the separator \b sep.
 * \param[in] sep   the separator
 * \param[in] input the string that is to be converted into a set
 * \return If (string!=NULL) this will always return a new set with either one entry if the separator is not
 *         found in the string, or with multiple members if the separator is present in the string. You need
 *         only free the resulting set.
 *
 * Convert \b input into a set of strings, using the separator \b sep. You need not copy the input string before
 * passing it to this function.
*/
char **str2set (const char sep, char *input);

/*!\brief Remove \b string from \b set.
 * \bug This should be combined with setdel() to remove code-duplication.
*/
char **strsetdel (char **, char *);

/*!\brief Delete dupes in \b set.
 * \bug Doesn't work properly, methinks.
 * \deprecated This will be removed soon.
*/
char **strsetdeldupes (char **);

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

/*!\}*/

/*!\ingroup utilityfunctionshashes
 * \{
*/
struct uhash *hashadd (struct uhash *, char *, void *, int32_t, void *);
struct uhash *hashfind (struct uhash *, char *);
struct uhash *hashdel (struct uhash *, struct uhash *);
void hashfree (struct uhash *);
#define hashnext(h) h->next
/*!\}*/

/*!\ingroup utilityfunctionsmem
 * \{
*/
void *emalloc (size_t);
void *ecalloc (size_t, size_t);
void *erealloc (void *, size_t);
char *estrdup (char *);
/*!\}*/

/*!\ingroup utilityfunctionsstrings
 * \{
*/
void strtrim (char *);
/*!\}*/

/*!\ingroup utilityfunctionsevents
 * \{
*/
void notice (unsigned char, char *);
struct einit_event *evdup (struct einit_event *);
struct einit_event *evinit (uint16_t);
void evdestroy (struct einit_event *);
/*!\}*/

#endif /* _UTILITY_H */
