/*
 *  test-core-apply-variables.c
 *  einit
 *
 *  Created by Magnus Deininger on 19/10/2007.
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

#include <einit/utility.h>
#include <unistd.h>
#include <stdlib.h>
#include <einit/set.h>

int example_setfont () {
 const char **vars = (const char **)set_str_add (set_str_add((void **)NULL, "configuration_services_setfont_font"), "lat2-12");
 char *res = apply_variables ("tty_max=$(cat /etc/einit/subsystems.d/tty.xml | egrep -c 'tty[0-9]-regular'); tty_list=$(seq 1 ${tty_max}); if [ -d /dev/vc ]; then tty_dev=/dev/vc/; else tty_dev=/dev/tty; fi; for tty_num in ${tty_list}; do setfont ${configuration_services_setfont_font} -C ${tty_dev}${tty_num}; done", vars);

 const char *expected_result = "tty_max=$(cat /etc/einit/subsystems.d/tty.xml | egrep -c 'tty[0-9]-regular'); tty_list=$(seq 1 ${tty_max}); if [ -d /dev/vc ]; then tty_dev=/dev/vc/; else tty_dev=/dev/tty; fi; for tty_num in ${tty_list}; do setfont lat2-12 -C ${tty_dev}${tty_num}; done";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int example_simple_start () {
 const char **vars = (const char **)set_str_add(set_str_add((void **)NULL, "simple"), "start");
 char *res = apply_variables ("${simple}", vars);

 const char *expected_result = "start";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int example_simple_start_half () {
 const char **vars = (const char **)set_str_add(set_str_add((void **)NULL, "simple"), "start");
 char *res = apply_variables ("${simple}${half}", vars);

 const char *expected_result = "start${half}";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int example_half_simple_start () {
 const char **vars = (const char **)set_str_add(set_str_add((void **)NULL, "simple"), "start");
 char *res = apply_variables ("${simple}${half}", vars);

 const char *expected_result = "start${half}";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}


int example_double () {
 const char **vars = (const char **)set_str_add(set_str_add(set_str_add(set_str_add((void **)NULL, "double"), "DD"), "simple"), "start");
 char *res = apply_variables ("${simple${double}${half}}", vars);

 const char *expected_result = "${simpleDD${half}}";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int example_broken () {
 const char **vars = (const char **)set_str_add(set_str_add(set_str_add(set_str_add((void **)NULL, "double"), "DD"), "simple"), "start");
 char *res = apply_variables ("${simple${double}${half}", vars);

 const char *expected_result = "${simpleDD${half}";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int example_middle () {
 const char **vars = (const char **)set_str_add(set_str_add(set_str_add(set_str_add((void **)NULL, "double"), "DD"), "simple"), "start");
 char *res = apply_variables ("${simple}${double}${half}", vars);

 const char *expected_result = "startDD${half}";

 char test_failed = 0;

 if (!strmatch (res, expected_result)) {
  test_failed = 1;
  fprintf (stdout, "setfont example failed:\n expected result:\n\"%s\"\n actual result:\n\"%s\"\n", expected_result, res);
 }

 efree (vars);
 efree (res);

 return test_failed;
}

int main () {
 if (example_setfont() || example_simple_start() || example_simple_start_half() || example_half_simple_start() ||
     example_double() || example_broken() || example_middle()) {
  return EXIT_FAILURE;
 }

 return EXIT_SUCCESS;
}
