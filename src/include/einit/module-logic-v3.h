/*
 *  module-logic-v3.h
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

#ifndef EINIT_MODULE_LOGIC_H
#define EINIT_MODULE_LOGIC_H

#define MODULE_LOGIC_V3

#include <pthread.h>
#include <unistd.h>
#include <einit/module.h>
#include <einit/config.h>
#include <einit/tree.h>
#include <einit/event.h>

struct module_taskblock {
 char **enable;
 char **disable;
 char **critical;
};

enum einit_plan_options {
 plan_option_shutdown = 0x1
};

struct mloadplan {
 struct module_taskblock changes;
 struct cfgnode *mode;
 char **used_modes;

 enum einit_plan_options options;
};

#define get_plan_progress(plan)\
 function_call_by_name(double, "module-logic-get-plan-progress", 1, plan)

#define module_logic_configure(x)

#endif

#ifdef __cplusplus
}
#endif
