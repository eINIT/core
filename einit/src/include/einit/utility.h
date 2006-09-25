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
 * \defgroup utilityfunctionshashes eINIT Utility Functions: Hashes
 * \defgroup utilityfunctionsevents eINIT Utility Functions: Events
 * \defgroup utilityfunctionsstrings eINIT Utility Functions: String-manipulation
 * \defgroup utilityfunctionsmem eINIT Utility Functions: Memory-management wrappers
*/
#include <inttypes.h>
#include <einit/event.h>

/*!\ingroup utilityfunctionssets
 * \{
*/
#define	SET_TYPE_STRING         0    /*!< Set-type: Set consists of strings */
#define	SET_NOALLOC            -1   /*!< Set-type: User takes care of (de-)allocating set members */

#define SORT_SET_STRING_LEXICAL 0x01 /*!< Sort string lexically */
#define SORT_SET_CUSTOM         0xFF /*!< Sort string with a custom sorting function */

/*!\ingroup utilityfunctionshashes
 * \brief Hash-Element
 *
 * This struct represents a hash-element. Note that, for this matter, hashes are merely key/value pairs. The
 * key is always a string, the value can be any pointer.
*/
struct uhash {
 char *key;          /*!< the key (perl-style; think of it as a variable-name) */
 void *value;        /*!< the value associated with the key */
 void *luggage;      /*!< a pointer to an area of memory that is references by the value (will be free()d) */
 struct uhash *next; /*!< next element */
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
/*!\brief Add the \b key with \b value to \b hash.
 * \param[in,out] hash    the hash to be manipulated
 * \param[in]     key     the name of the variable
 * \param[in]     value   the variable's value
 * \param[in]     vlen    the size of the element; use either sizeof(), SET_TYPE_STRING or SET_NOALLOC
 * \param[in]     luggage a pointer to an area of memory that was malloc()d for the value
 *                        (will be freed if element is free()d)
 * \return This will manipulate \b hash by adding \b key = \b value to it. It may free( \b hash ).
 *         You will need to hashfree() the returned hash once you're done with it.
 *
 * This is used to add hash elements to a hash, or to initialise it. You may point the luggage variable to
 * an area of memory that is to be free()d if the corresponding element is free()d.
*/
struct uhash *hashadd (struct uhash *hash, char *key, void *value, int32_t vlen, void *luggage);

/*!\brief Find the \b key in \b hash.
 * \param[in] hash    the hash to be manipulated
 * \param[in] key     the name of the variable
 * \return This will return a pointer to the hash element that is identified by the \b key. You should not
 *         modify anything but the value and luggage fields in the returned element.
 *
 * This is used to find hash elements in a hash.
*/
struct uhash *hashfind (struct uhash *hash, char *key);

/*!\brief Delete the \b subject from the hash \b cur.
 * \param[in,out] cur     the hash to be manipulated
 * \param[in]     subject the hash element to be deleted
 * \return This will return a pointer to the first hash element you passed to it, or to the first element after
 *         \b subject has been erased.
 *
 * This function will delete an element from a hash. It will also free() any resources allocated by other hash
 * functions for the element in question, and it will erase any \b luggage if it has been defined.
*/
struct uhash *hashdel (struct uhash *cur, struct uhash *subject);

/*!\brief Free ( \b hash ).
 * \param[in] hash the hash to be free()d
 * \return This function does not return any value.
 *
 * This function will deallocate all resources used by the specified hash, including all luggage-areas. After
 * the function returns, using the free()d hash with any of the hash functions will result in undefined
 * behaviour (usually a SIGSEGV or a SIGBUS, though, so don't do it).
*/
void hashfree (struct uhash *hash);

/*!\brief Return next hash element
 * \param[in] h the hash
 * \return This function will return the next hash element.
 *
 * This macro can be used to sequentially step through a hash, instead of by keys.
*/
#define hashnext(h) h->next
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
struct einit_event *evinit (uint16_t type);

/*!\brief Destroy event structure \b ev
 * \param[in,out] ev the event to be destroyed.
 * \return This function does not return any value.
 *
 * This function will deallocate all resources that had been used to create the event structure \b ev. After this
 * function has run, \b ev will be invalid and attempting to use it will result in undefined behaviour.
*/
void evdestroy (struct einit_event *ev);
/*!\}*/

#endif /* _UTILITY_H */
