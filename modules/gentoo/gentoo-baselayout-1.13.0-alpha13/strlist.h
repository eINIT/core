/*
   strlist.h 
   String list macros for making char ** arrays
   Copyright 2007 Gentoo Foundation
   Written by Roy Marples <uberlord@gentoo.org>
   Based on a previous implementation by Martin Schlemmer
   Released under the GPLv2
   */

#ifndef __STRLIST_H__
#define __STRLIST_H__

#include <stdlib.h>
#include <string.h>

#include "rc-misc.h"

/* Add a new item to a string list.  If the pointer to the list is NULL,
   allocate enough memory for the amount of entries needed.  Ditto for
   when it already exists, but we add one more entry than it can
   contain.  The list is NULL terminated. */
#define STRLIST_ADD(_list, _item) \
 do { \
   char **_tmp; \
   int _i = 0; \
   if (_item) \
     { \
       while ((_list) && (_list[_i])) \
         _i++; \
       _tmp = realloc (_list, sizeof (char *) * (_i + 2)); \
       if (_tmp) \
         { \
           _list = _tmp; \
	   _list[_i] = xstrdup (_item); \
	   _list[_i + 1] = NULL; \
	 } \
     } \
 } while (0)

/* Same as above, but insert the item into the list dependant on locale */
#define STRLIST_ADDSORT(_list, _item) \
 do { \
   char **_tmp; \
   char *_tmp1; \
   char *_tmp2; \
   int _i = 0; \
   if (_item) \
     { \
       while ((_list) && (_list[_i])) \
         _i++; \
       _tmp = realloc (_list, sizeof (char *) * (_i + 2)); \
       if (_tmp) \
         { \
	   _list = _tmp; \
	   if (_i == 0) \
	     _list[_i] = NULL; \
	   _list[_i + 1] = NULL; \
	   _i = 0; \
	   while (_list[_i] && strcoll (_list[_i], _item) < 0) \
	     _i++; \
	   _tmp1 = _list[_i]; \
	   _list[_i] = xstrdup (_item); \
	   do \
	     { \
	       _i++;\
	       _tmp2 = _list[_i]; \
	       _list[_i] = _tmp1; \
	       _tmp1 = _tmp2; \
	     } while (_tmp1); \
	 } \
     } \
 } while (0)

/* Delete one entry from the string list, and shift the rest down if the entry
 * was not at the end.  For now we do not resize the amount of entries the
 * string list can contain, and free the memory for the matching item */
#define STRLIST_DEL(_list, _item) \
 do { \
   int _i = 0; \
   if (_list && _item) \
     { \
       while (_list[_i]) \
	 { \
	   if (strcmp (list[_i], _item)) \
	     { \
	       free (_list[_i]); \
	       do \
		 { \
		   _list[_i] = _list[_i + 1]; \
		   _i++; \
		 } while (_list[_i]); \
	       break; \
	     } \
	 } \
     } \
 } while (0)

/* Step through each entry in the string list, setting '_pos' to the
   beginning of the entry.  '_counter' is used by the macro as index,
   but should not be used by code as index (or if really needed, then
   it should usually by +1 from what you expect, and should only be
   used in the scope of the macro) */
#define STRLIST_FOREACH(_list, _pos, _counter) \
 if ((_list) && _list[0] && ((_counter = 0) == 0)) \
   while ((_pos = _list[_counter++]))

/* Just free the whole string list */
#define STRLIST_FREE(_list) \
 do { \
   if (_list) \
     { \
       int _i = 0; \
       while (_list[_i]) \
	 { \
	   free (_list[_i]); \
	   _list[_i++] = NULL; \
	 } \
       free (_list); \
       _list = NULL; \
     } \
 } while (0)

#endif /* __STRLIST_H__ */
