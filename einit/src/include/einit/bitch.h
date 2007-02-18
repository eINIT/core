/*
 *  bitch.h
 *  einit
 *
 *  Created by Magnus Deininger on 14/02/2006.
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
 *
 */

/*
Copyright (c) 2006, 2007, Magnus Deininger
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the name of the project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*!\file einit/bitch.h
 * \brief Error-reporting functions
 * \author Magnus Deininger
 *
 * Error reporting (a.k.a. "bitching") is fairly important...
*/

#ifndef _BITCH_H
#define _BITCH_H

#define BTCH_ERRNO 1 /*!< report error from the errno variable */
#define BTCH_DL 2    /*!< report dynamic linker error */

#define BITCH_BAD_SAUCE 0x00
#define BITCH_EMALLOC   0x01
#define BITCH_STDIO     0x02
#define BITCH_REGEX     0x03
#define BITCH_EXPAT     0x04
#define BITCH_DL        0x05
#define BITCH_LOOKUP    0x06

#define BITCH_SAUCES (BITCH_LOOKUP + 1)

unsigned char mortality[BITCH_SAUCES];

/*!\brief Bitch about whatever happened just now
 * \param[in] opt bitwise OR of BTCH_ERRNO and BTCH_DL
 * \return (int)-1. Don't ask.
 *
 * Bitch about whatever happened just now, i.e. report the last error.
*/
int bitch (unsigned int opt);

/*!\brief Bitch about whatever happened just now
 * \param[in] sauce a source for whatever happened
 * \param[in] location a string indicating where the error happened (like perror)
 * \param[in] error the value of errno at the time of the error
 * \param[in] reason a string to print as an alternative to the error number
 * \return error. Don't ask. May also not return at all.
 *
 * Bitch about whatever happened just now, i.e. report the last error.
 * Depending on the sauce you pass, it may or may not make the program
 * terminate. (Configurable this is)
 */
int bitch2 (unsigned char sauce, char *location, int error, char *reason);

#endif /* _BITCH_H */
