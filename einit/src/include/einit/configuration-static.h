/*
 *  monitor.h
 *  einit
 *
 *  Created by Magnus Deininger on 23/11/2007.
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EINIT_MONITOR_H
#define EINIT_MONITOR_H

#define BSDLICENSE "All rights reserved.\n"\
 "\n"\
 "Redistribution and use in source and binary forms, with or without modification,\n"\
 "are permitted provided that the following conditions are met:\n"\
 "\n"\
 "    * Redistributions of source code must retain the above copyright notice,\n"\
 "      this list of conditions and the following disclaimer.\n"\
 "    * Redistributions in binary form must reproduce the above copyright notice,\n"\
 "      this list of conditions and the following disclaimer in the documentation\n"\
 "      and/or other materials provided with the distribution.\n"\
 "    * Neither the name of the project nor the names of its contributors may be\n"\
 "      used to endorse or promote products derived from this software without\n"\
 "      specific prior written permission.\n"\
 "\n"\
 "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n"\
 "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n"\
 "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n"\
 "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR\n"\
 "ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n"\
 "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n"\
 "LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON\n"\
 "ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"\
 "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n"\
 "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"

#define NODE_MODE 1

#define EINIT_VERSION 1

#if defined (ISSVN) && (ISSVN > 0)
#define EINIT_VERSION_LITERAL_NUMBER "live"
#else
#define EINIT_VERSION_LITERAL_NUMBER "0.25.1"
#endif

#define EINIT_VERSION_LITERAL EINIT_VERSION_LITERAL_NUMBER EINIT_VERSION_LITERAL_SUFFIX

#define einit_exit_status_last_rites_halt 42
#define einit_exit_status_last_rites_reboot 43
#define einit_exit_status_last_rites_kexec 44

#define einit_exit_status_die_respawn 50

#endif

#ifdef __cplusplus
}
#endif
