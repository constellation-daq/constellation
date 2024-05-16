/*
---------------------------------------------------------------------------

    This file is part of uHAL.

    uHAL is a hardware access library and programming framework
    originally developed for upgrades of the Level-1 trigger of the CMS
    experiment at CERN.

    uHAL is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    uHAL is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with uHAL.  If not, see <http://www.gnu.org/licenses/>.


      Andrew Rose, Imperial College, London
      email: awr01 <AT> imperial.ac.uk

      Marc Magrans de Abril, CERN
      email: marc.magrans.de.abril <AT> cern.ch

---------------------------------------------------------------------------
*/

/**
	@file
	@author Andrew W. Rose
	@date 2012
*/

#ifndef _uhal_DerivedNode_hpp_
#define _uhal_DerivedNode_hpp_


#include <type_traits>

#include "uhal/Node.hpp"


/**
  Macro which adds a Derived Node Class to the factory
  It takes a classname and then creates a registration helper object, with the classname as its template parameter
  and a stringified version of the classname as its constructor argument.
*/
#define UHAL_REGISTER_DERIVED_NODE( classname ) \
  uhal::RegistrationHelper< classname > classname##RegistrationHelper( #classname ); \
  uhal::Node* classname::clone() const \
  { \
    static_assert((std::is_base_of<uhal::Node, classname>::value), "Derived node class must be a descendant of uhal::Node"); \
    return new classname ( static_cast<const classname&> ( *this ) ); \
  }


//! Macro which adds the clone method implementation for derived classes
#define UHAL_DERIVEDNODE(DerivedType) \
protected: \
  virtual uhal::Node* clone() const;

  
namespace uhal
{

  /**
    Experimental!! Helper struct for adding the DerivedNode to the Node Factory
    Declaring an instance of an object at global scope means that it is created *before* the main code is entered
    We can use this to our advantage by using the constructor of this object to add entries to the factory in a distributed fashion
    (for instance, in the file where the derived node is defined), rather than manually having to add entries in one file
    To make things even simpler, the REGISTER macro expands the template argument to a string and passes it to the constructor.
  */
  template< typename T >
  struct RegistrationHelper
  {
    /**
      Constructor
      @param aDerivedClassName The name that will be used to identify this class in the factory
    */
    RegistrationHelper ( const std::string& aDerivedClassName );
  };
  
}

#include "uhal/TemplateDefinitions/DerivedNode.hxx"

#endif
