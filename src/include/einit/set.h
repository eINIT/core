/*
 *  set-lowmem.h
 *  einit
 *
 *  Split out from utility.h on 20/01/2007.
 *  Moved from set.h on 20/02/2007
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

#ifdef __cplusplus
extern "C" {
#endif

/*!\file einit/set.h
 * \brief Utility-Functions (sets)
 * \author Magnus Deininger
 *
 * These are functions that should be of use to all modules and eINIT itself.
*/

#ifndef EINIT_SET_H
#define EINIT_SET_H

/*!\defgroup utilityfunctionssets eINIT Utility Functions: Sets
*/
#include <inttypes.h>
#include <sys/types.h>

/*!\ingroup utilityfunctionssets
 * \{
*/
#define SET_TYPE_STRING 0
/*!< Set-type: Set consists of strings */
#define SET_NOALLOC -1
/*!< Set-type: User takes care of (de-)allocating set members */

enum set_sort_order {
 set_sort_order_string_lexical = 0x02,
/*!< Sort string lexically */
 set_sort_order_custom         = 0x01
/*!< Sort string with a custom sorting function */
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
void **setcombine (const void **set1, const void **set2, const int32_t esize);

void **setcombine_nc (void **set1, const void **set2, const int32_t esize);

void **setslice (const void **set1, const void **set2, const int32_t esize);

void **setslice_nc (void **set1, const void **set2, const int32_t esize);

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
void **setadd (void **set, const void *item, int32_t esize);

#define set_str_add(set,item) (char **)setadd((void **)set, item, SET_TYPE_STRING)
#define set_noa_add(set,item) setadd((void **)set, item, SET_NOALLOC)
#define set_fix_add(set,item,esize) setadd((void **)set, item, esize)

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
void **setdup (const void **set, int32_t esize);

#define set_str_dup(set) (char **)setdup((const void **)set, SET_TYPE_STRING)
#define set_noa_dup(set) setdup((const void **)set, SET_NOALLOC)
#define set_fix_dup(set,esize) setdup((const void **)set, esize)

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
void **setdel (void **set, const void *item);

/*!\brief Count elements in \b set.
 * \param[in] set the set
 * \return This function will return the number of elements in the \b set.
 *
 * Counts the number of elements in \b set and returns this number. 
*/
int setcount (const void **set);

/*!\brief Sort elements in \b set.
 * \param[in,out] set          the set
 * \param[in]     task         how to sort the set
 * \param[in]     sortfunction a function that compares set elements. (if you use task=SORT_SET_CUSTOM)
 * \return This function will modify the set you pass to it. It may free() this set. It may return the same
 *         set unmodified or it may return a completely new set altogether. On error, NULL is returned. If
 *         the resulting set is empty, NULL is returned. You will only need to free() the resulting set.
 * \bug broken. Do not use.
 *
 * Sorts the \b set you pass to it, either using the function specified with \b sortfunction, or by what you
 * pass to the function in \b task.
*/
void setsort (void **set, enum set_sort_order task, signed int(*sortfunction)(const void *, const void*));

/*!\brief Find out if element \b needle is in set \b haystack.
 * \param[in] haystack the set
 * \param[in] needle   the element
 * \param[in] esize    Use SET_TYPE_STRING to check if a string is in the set. Use SET_NOALLC to check if a
                       pointer (or any other element) is in the set.
 * \return 1 if the \b needle is in \b haystack, 0 if it is not.
 *
 * Find out if element \b needle is in set \b haystack. Returns 1 if yes, 0 otherwise.
*/
int inset (const void **haystack, const void *needle, int32_t esize);

char inset_pattern (const void **haystack, const void *needle, int32_t esize);

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
char **str2set (const char sep, const char *input);

char *set2str (const char sep, const char **oinput);

/*!\brief Remove \b string from \b set.
 * \bug This should be combined with setdel() to remove code-duplication.
*/
char **strsetdel (char **, char *);

/*!\brief Delete dupes in \b set.
 * \bug Doesn't work properly, methinks.
 * \deprecated This will be removed soon.
*/
char **strsetdeldupes (char **);

/*!\}*/

#endif /* _SET_H */

#ifdef __cplusplus
}
#endif
