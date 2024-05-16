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
	@author Marc Magrans De Abril
	@date 2012
*/

#ifndef _uhal_DerivedNodeFactory_hpp_
#define _uhal_DerivedNodeFactory_hpp_


#include <memory>
#include <string>
#include <unordered_map>


namespace uhal
{
  class Node;
  class NodeTreeBuilder;   // IWYU pragma: keep

  //! Helper class to add Nodes derived from DerivedNode to the class factory
  template< typename T > struct RegistrationHelper;   // IWYU pragma: keep


  /**
    A singleton class to register derived nodes, and create instances of them later
    NOTE! This is a factory method and must be Mutex protected if it is used in multithreaded environments!
  */
  class DerivedNodeFactory
  {
    friend class NodeTreeBuilder;

    public:

      //! EXPERIMENTAL! Give the RegistrationHelper access to the private factory
      template< typename T > friend struct RegistrationHelper;

    private:
      /**
      	Default constructor
      	This is private since only a single instance is to be created, using the getInstance method
      */
      DerivedNodeFactory ();

    public:

      DerivedNodeFactory(const DerivedNodeFactory&) = delete;
      DerivedNodeFactory& operator=(const DerivedNodeFactory&) = delete;

      //! Destructor
      virtual ~DerivedNodeFactory ();

      /**
      	Static method to retrieve the single instance of the class
      	@return the single instance of the class
      */
      static DerivedNodeFactory& getInstance();


    private:

      Node* convertToClassType( Node* aNode );

      /**
        Method to create an associate between a node type identifier and a Creator of that particular node type
        @param aNodeClassName the node type identifier
      */
      template <class T>
      void add ( const std::string& aNodeClassName );


      //! An abstract base class for defining the interface to the creators
      class CreatorInterface
      {
        public:
          //! Default constructor
          CreatorInterface()
          {
          }

          //! Destructor
          virtual ~CreatorInterface()
          {
          }

          /**
          Interface to a function which create a new derived node class based on the class name
          @param aAttributes a vector containing a set of name value pairs which were passed as arguments
          @return a new node tree
          */
          virtual Node* create ( const Node& aNode ) = 0;
      };


      //! Templated concrete implementation with a CreatorInterface interface
      template <class T>
      class Creator: public CreatorInterface
      {
        public:

          //! Default constructor
          Creator()
          {
          }

          //! Destructor
          virtual ~Creator()
          {
          }

          /**
          Concrete function which creates a new IPbus client based on the protocol identifier specified
          @param aAttributes a vector containing a set of name value pairs which were passed as arguments
          @return a new node tree
          */
          Node* create ( const Node& aNode );
      };


    private:
      //! The single instance of the class
      static std::shared_ptr<DerivedNodeFactory> mInstance;

      //! Hash map associating a creator for a particular node type with a string identifier for that node type
      std::unordered_map< std::string , std::shared_ptr< CreatorInterface > > mCreators;
  };
}

#include "uhal/TemplateDefinitions/DerivedNodeFactory.hxx"

#endif
