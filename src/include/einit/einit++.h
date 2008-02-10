/*
 *  einit++.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Modifications by nikolavp
 *  Copyright 2007-2008 Magnus Deininger. All rights reserved.
 *
 */
 
/*
Copyright (c) 2007-2008, Magnus Deininger
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
 
/*!\file einit/einit++.h
 * \brief eINIT C++ Client Library
 * \author Magnus Deininger
 *
 * Bindings for libeinit in C++ (libeinit++). You'll very likely want to use this and not libeinit (einit/einit.h) for your client applications. It's also a lot cleaner and you don't need to worry about nearly as much.
 */
 
#include <einit/einit.h>
#include <string>
#include <map>
#include <vector>

using std::string;
using std::vector;

/*!\brief The Main eINIT Object
 *
 * This is the main object to manipulate eINIT with. You should only have one instance of this in your program.
 */
class Einit {
 public:
  Einit(); /*!<\brief Regular constructor, as a side-effect this'll initate the connection to eINIT. */
  ~Einit(); /*!<\brief Regular destructor, as a side-effect this'll terminate the connection to eINIT. */

/*!\brief Power Down the System
 *
 * Tell eINIT to initiate a system shutdown. You're likely to die soon after this, so better start cleaning up ASAP.
 */
  bool powerDown();

/*!\brief Reset the System
 *
 * Tell eINIT to initiate a system reboot. You're likely to die soon after this, so better start cleaning up ASAP.
 */
  bool powerReset();

/*!\brief Update current Information from the core.
 *
 * Update all the information we have from eINIT.
 */
  void update();
};
