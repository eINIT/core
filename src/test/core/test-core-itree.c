/*
 *  test-core-itree.c
 *  einit
 *
 *  Created by Magnus Deininger on 05/12/2007.
 *  Copyright 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2007, Magnus Deininger
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <einit/itree.h>

struct itree *tree = NULL;

int basic_test () {
 struct itree *t;

 tree = itreeadd (tree, 23, (void *)65, tree_value_noalloc);
 tree = itreeadd (tree, 42, (void *)32, tree_value_noalloc);
 tree = itreeadd (tree, 19, (void *)16, tree_value_noalloc);

 if (!(t = itreefind (tree, 23, tree_find_first)) || (t->value != (void *)65)) {
  return 1;
 }

 if (!(t = itreefind (tree, 42, tree_find_first)) || (t->value != (void *)32)) {
  return 1;
 }

 if (!(t = itreefind (tree, 19, tree_find_first)) || (t->value != (void *)16)) {
  return 1;
 }

 return 0;
}

int multikey_test () {
 struct itree *t;

 tree = itreeadd (tree, 23, (void *)14, tree_value_noalloc);
 tree = itreeadd (tree, 23, (void *)1, tree_value_noalloc);

 if (!(t = itreefind (tree, 23, tree_find_first)) || (t->value != (void *)1)) {
  fprintf (stderr, "expected value: 1, got: %i\n", (int)t->value);
  return 1;
 }
 if (!(t = itreefind (t, 23, tree_find_next)) || (t->value != (void *)14)) {
  fprintf (stderr, "expected value: 14, got: %i\n", (int)t->value);
  return 1;
 }
 if (!(t = itreefind (t, 23, tree_find_next)) || (t->value != (void *)65)) {
  fprintf (stderr, "expected value: 65, got: %i\n", (int)t->value);
  return 1;
 }

 return 0;
}

int multikey_secondary_test () {
 struct itree *t;

 tree = itreeadd (tree, 19, (void *)42, tree_value_noalloc);

 if (!(t = itreefind (tree, 19, tree_find_first)) || (t->value != (void *)42)) {
  return 1;
 }

 return 0;
}

int random_int (int max) {
 long n = random();
 double d = ((double)n) / ((double)RAND_MAX);

 return (int)(d*max);
}

#define RANDOM_SEED 9016376
#define RANDOM_MAX 0x7fffffff
#define RANDOM_TEST_PASSES 0x10000

int random_test () {
 srandom(RANDOM_SEED);
 struct itree *t;
 int passes = RANDOM_TEST_PASSES;
 char completion = 0, nc;

 while (passes) {
  int key = random_int (RANDOM_MAX);
  int value = random_int (RANDOM_MAX);

  tree = itreeadd (tree, key, (void *)value, tree_value_noalloc);

  if (!(t = itreefind (tree, key, tree_find_first)) || (t->value != (void *)value)) {
   return 1;
  }

  passes--;
 }

 return 0;
}

int main () {
 if (basic_test ()) {
  fprintf (stderr, "basic lookup/insertion test failed!");
  return EXIT_FAILURE;
 }

#ifdef DEBUG
 fprintf (stderr, "tree after basic_test():\n");
 itreedump (tree);
#endif

 if (multikey_test ()) {
  fprintf (stderr, "multikey lookup/insertion test failed!");
  return EXIT_FAILURE;
 }

#ifdef DEBUG
 fprintf (stderr, "tree after multikey_test():\n");
 itreedump (tree);
#endif

 if (basic_test ()) {
  fprintf (stderr, "basic lookup/insertion test (second pass) failed!");
  return EXIT_FAILURE;
 }

#ifdef DEBUG
 fprintf (stderr, "tree after basic_test() (second pass):\n");
 itreedump (tree);
#endif

 if (multikey_secondary_test ()) {
  fprintf (stderr, "multikey lookup/insertion test (second pass) failed!");
  return EXIT_FAILURE;
 }

#ifdef DEBUG
 fprintf (stderr, "tree after multikey_secondary_test():\n");
 itreedump (tree);
#endif

 fprintf (stderr, "random lookup/insertion test: (%i passes): ", RANDOM_TEST_PASSES);
 if (random_test ()) {
  fprintf (stderr, "failed!\n");
  return EXIT_FAILURE;
 }
 fprintf (stderr, "alright\n");

// itreefree_all (tree, NULL);

 return EXIT_SUCCESS;
}
