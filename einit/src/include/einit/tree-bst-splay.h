/*
 *  tree-bst-splay.h
 *  einit
 *
 *  Created by Magnus Deininger on 20/02/2007.
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

/*!\file einit/tree.h
 * \brief Utility-Functions
 * \author Magnus Deininger
 *
 * search-trees, etc 
*/

#ifndef _TREE_H
#define _TREE_H

/*!\defgroup trees eINIT Utility Functions: Trees
*/
#include <inttypes.h>
#include <einit/event.h>
#include <sys/types.h>

#define TREE_FIND_FIRST         0x01
#define TREE_FIND_NEXT          0x02

/*!\ingroup trees
 * \brief Hash-Element
 *
 * This struct represents a stree-element. Note that, for this matter, streees are merely key/value pairs. The
 * key is always a string, the value can be any pointer.
*/
struct stree {
 char *key;           /*!< the key (perl-style; think of it as a variable-name) */
 void *value;         /*!< the value associated with the key */
 void *luggage;       /*!< a pointer to an area of memory that is references by the value (will be free()d) */
 struct stree *next;  /*!< next element (for sequential tree traversal) */

 struct stree **root;  /*!< double-pointer to the root-element */
 struct stree **lbase; /*!< double-pointer to the linear base-node */
 struct stree *left;   /*!< left element */
 struct stree *right;  /*!< right element */
 struct stree *parent; /*!< ancestor element */
};

/*!\brief Add the \b key with \b value to \b stree.
 * \param[in,out] stree    the stree to be manipulated
 * \param[in]     key     the name of the variable
 * \param[in]     value   the variable's value
 * \param[in]     vlen    the size of the element; use either sizeof(), SET_TYPE_STRING or SET_NOALLOC
 * \param[in]     luggage a pointer to an area of memory that was malloc()d for the value
 *                        (will be freed if element is free()d)
 * \return This will manipulate \b stree by adding \b key = \b value to it. It may free( \b stree ).
 *         You will need to streefree() the returned stree once you're done with it.
 *
 * This is used to add stree elements to a stree, or to initialise it. You may point the luggage variable to
 * an area of memory that is to be free()d if the corresponding element is free()d.
*/
struct stree *streeadd (struct stree *stree, char *key, void *value, int32_t vlen, void *luggage);

/*!\brief Find the \b key in \b stree.
 * \param[in] stree    the stree to be manipulated
 * \param[in] key     the name of the variable
 * \return This will return a pointer to the stree element that is identified by the \b key. You should not
 *         modify anything but the value and luggage fields in the returned element.
 *
 * This is used to find stree elements in a stree.
*/
struct stree *streefind (struct stree *stree, char *key, char options);

/*!\brief Delete the \b subject from its stree.
 * \param[in]     subject the stree element to be deleted
 * \return This will return a pointer to the first stree element you passed to it, or to the first element after
 *         \b subject has been erased.
 *
 * This function will delete an element from a stree. It will also free() any resources allocated by other stree
 * functions for the element in question, and it will erase any \b luggage if it has been defined.
*/
struct stree *streedel (struct stree *subject);

/*!\brief Free ( \b stree ).
 * \param[in] stree the stree to be free()d
 * \return This function does not return any value.
 *
 * This function will deallocate all resources used by the specified stree, including all luggage-areas. After
 * the function returns, using the free()d stree with any of the stree functions will result in undefined
 * behaviour (usually a SIGSEGV or a SIGBUS, though, so don't do it).
*/
void streefree (struct stree *stree);

/*!\brief Return next stree element
 * \param[in] h the stree
 * \return This function will return the next stree element.
 *
 * This macro can be used to sequentially step through a stree, instead of by keys.
*/
#define streenext(h) h->next
/*!\}*/

#endif /* _TREE_H */

#ifdef __cplusplus
}
#endif
