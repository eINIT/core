/*
 *  einit.h
 *  eINIT
 *
 *  Created by Magnus Deininger on 01/08/2007.
 *  Modifications by nikolavp
 *  Copyright 2006, 2007 Magnus Deininger. All rights reserved.
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
class EinitService;
class EinitModule;
class EinitMode;
 
/*!\You can use the following typedefs
 *\to make your code more readable
 */
typedef std::map<string, EinitService *> servicesMap;
typedef std::map<string, EinitModule *> modulesMap;
typedef std::map<string, EinitMode *> modesMap;
 
/*!\brief The Main eINIT Object
 *
 * This is the main object to manipulate eINIT with. You should only have one instance of this in your program.
 */
class Einit {
 public:
  Einit(); /*!<\brief Regular constructor, as a side-effect this'll initate the connection to eINIT. */
  ~Einit(); /*!<\brief Regular destructor, as a side-effect this'll terminate the connection to eINIT. */
 
/*!\brief Listen in on remote events
 * \param[in] type     A subsystem-ID to listen in on.
 * \param[in] callback The function you wish called
 *
 * Tell the core you wish to receive a specific <type> of events. If an appropriate event is received, your <callback> is called. Note that you can't change any elements of an event using this callback, as opposed to the core, where you can do this. Changes are not propagated back to the core.
 */
  void listen (enum einit_event_subsystems type, void (*callback)(struct einit_remote_event *));
 
/*!\brief Stop listening in on remote events
 * \param[in] type     A subsystem-ID that the callback is registered with.
 * \param[in] callback The function
 *
 * Tell the core you you no longer wish to receive a specific <type> of events using <callback>.
 */
  void ignore (enum einit_event_subsystems type, void (*callback)(struct einit_remote_event *));
 
 
/*!\brief Get an Object to manipulate a Module with
 * \param[in] id The module to look up.
 *
 * Returns an object that can be used to manipulate a specific module.
 */
  EinitModule *getModule (const string& id) const;
 
/*!\brief Get an Object to manipulate a Service with
 * \param[in] id The service to look up.
 *
 * Returns an object that can be used to manipulate a specific service.
 */
  EinitService *getService (const string& id) const;
 
/*!\brief Get an Object to manipulate a mode with
 * \param[in] id The mode to look up.
 *
 * Returns an object that can be used to manipulate a specific mode.
 */
  EinitMode *getMode (const string& id)const;
 
/*!\brief Get all current Modules
 *
 * Returns a map with pointers to all of eINIT's current modules.
 */
  modulesMap getAllModules() const;
 
/*!\brief Get all current Services
 *
 * Returns a map with pointers to all of eINIT's current services.
 */
  servicesMap getAllServices() const;
 
/*!\brief Get all current Modes
 *
 * Returns a map with pointers to all of eINIT's current modes.
 */
  modesMap getAllModes() const;
 
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
 
 private:
  bool listening;
 
  servicesMap services;
  modulesMap modules;
  modesMap modes;
 
};
 
/*!\brief Base-class for Einit's "Offspring"-classes
 *
 * You won't really need to deal with this.
 */
class EinitOffspring {
 friend class Einit;
 
 public:
 
 protected:
  EinitOffspring (Einit *); /*!<\brief Constructor. The argument is the main instance of eINIT to connect this to */
  ~EinitOffspring (); /*!<\brief Destructor. Nothing fancy... move along... */
 
  Einit *main; /*!<\brief The main instance of Einit that this is connected with */
 
 private:
};
 
/*!\brief An eINIT Service
 *
 * This object is used to manipulate the state of a Service in eINIT's core.
 */
class EinitService : public EinitOffspring {
 friend class Einit;
 
 public:
 
/*!\brief Enable this Service
 *
 * This function makes eINIT try to enable this specific service. It's like calling 'einit-control rc <service> enable'.
 */
  bool enable();
 
/*!\brief Disable this Service
 *
 * This function makes eINIT try to disable this specific service. It's like calling 'einit-control rc <service> disable'.
 */
  bool disable();
 
/*!\brief Invoke a custom action on this Service
 *
 * This function makes eINIT try to do <action> on this specific service. It's like calling 'einit-control rc <service> <action>'.
 */
  bool call(string action);
 
/*!\brief Get all the Common Functions of this Service
 *
 * This function tries to find all the common functions that can be used for the call() method on this service. "Common functions" are functions that work with all the modules in this service. This function may return all functions, if it can't find out which of them are in the common subset.
 */
  vector<string> getCommonFunctions();
 
/*!\brief Get all the Functions of this Service
 *
 * This function tries to find all the functions that can be used for the call() method on this service. "All functions" means all functions, regardless of whether they're implemented by all modules that provide this service.
 */
  vector<string> getAllFunctions();
 
/*!\brief The ID of this service, for reference and the like. */
  string id;
/*!\brief Is this service provided? */
  bool provided;
 
/*!\brief A vector with all the modules that make up this service. */
  vector<string> modules;
/*!\brief A map with all the services that make up this service, if this service is a group or may be seen as one. */
  vector<string> group;
/*!\brief The type of this group, if it is one. */
  string groupType;
/*!\brief A map with all the modes that this service is known to be used in. */
  vector<string> modes;
 
 private:
  EinitService (Einit *, struct einit_service *);
  ~EinitService ();
 
  bool update(struct einit_service *);
};
 
/*!\brief An eINIT Module
 *
 * This object is used to manipulate the state of a Module in eINIT's core.
 */
class EinitModule : public EinitOffspring {
 friend class Einit;
 friend class EinitService;
 
 public:
/*!\brief Enable this Module
 *
 * This function makes eINIT try to enable this specific module. It's like calling 'einit-control rc <module> enable'.
 */
  bool enable();
 
/*!\brief Disable this Module
 *
 * This function makes eINIT try to disable this specific module. It's like calling 'einit-control rc <module> disable'.
 */
  bool disable();
 
/*!\brief Invoke a custom action on this Module
 *
 * This function makes eINIT try to do <action> on this specific module. It's like calling 'einit-control rc <module> <action>'.
 */
  bool call(string);
 
/*!\brief Get all the Functions of this Module
 *
 * This function tries to find all the functions that can be used for the call() method on this module. This needs some cooperation from the module.
 */
  vector<string> getAllFunctions() const;
 
/*!\brief The ID of this module, just in case */
  string id;
/*!\brief A name for this module, so you got something to display to users */
  string name;
 
/*!\brief Is this module enabled? */
  bool enabled;
/*!\brief Is this module idle (i.e. zapped or has never been called)? */
  bool idle;
/*!\brief Has there been an error? */
  bool error;
 
/*!\brief A vector with the services provided by this module. */
  vector<string> provides;
/*!\brief A vector with the services required by this module. */
  vector<string> requires;
 
/*!\brief Regular expresions that indicate before what other services or modules this module will be loaded. */
  vector<string> before;
/*!\brief Regular expresions that indicate after what other services or modules this module will be loaded. */
  vector<string> after;
 
 private:
  EinitModule (Einit *, struct einit_module *);
  ~EinitModule ();
 
  bool update(struct einit_module *);
 
  vector<string> functions;
};
 
/*!\brief An eINIT Mode
 *
 * This object is used to manipulate the state of a Mode in eINIT's core.
 */
class EinitMode : public EinitOffspring {
 friend class Einit;
 
 public:
 
/*!\brief Switch to this Mode
 *
 * Tell eINIT it should switch to this mode.
 */
  bool switchTo();
 
/*!\brief The ID of this mode, just for cakes. */
  string id;
 
/*!\brief A map with the services to enable in this mode. */
  servicesMap services;
/*!\brief A map with the services that are crucial in this mode. */
  servicesMap critical;
/*!\brief A map with the services to disable in this mode. */
  servicesMap disable;
/*!\brief A map with the base-modes of this mode. */
  modesMap base;
 
 private:
  EinitMode (Einit *, struct einit_mode_summary *);
  ~EinitMode ();
 
  bool update(struct einit_mode_summary *);
};
