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

#ifndef _uhal_HwInterface_hpp_
#define _uhal_HwInterface_hpp_


#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include "uhal/ClientInterface.hpp" // IWYU pragma: keep
#include "uhal/Node.hpp"            // IWYU pragma: keep


namespace uhal
{
  //! A class which bundles a node tree and an IPbus client interface together providing everything you need to navigate and perform hardware access
  class HwInterface
  {
    public:
      /**
      	Constructor
      	@param aClientInterface a shared pointer to a client interface which performs the transport
      	@param aNode a shared pointer to a >>freshly cloned<< node representing the >>full<< endpoint structure
      */
      HwInterface ( const std::shared_ptr<ClientInterface>& aClientInterface , const std::shared_ptr< Node >& aNode );

      /**
      	Copy Constructor
        Calls the copy constructor on the ClientInterface
        Calls the clone method on the Node Tree
        @param hwInterface a Hardware Interface instance to copy
      */
      HwInterface ( const HwInterface& );

      //! Destructor
      virtual ~HwInterface();

      /**
      	Get the underlying IPbus client
      	@return the underlying IPbus client
      */
      ClientInterface& getClient();

      /**
      	Return the url of the target for this client
      	@return the url of the target for this client
      */
      const std::string& uri() const;

      /**
      	Return the identifier of the target for this client
      	@return the identifier of the target for this client
      */
      const std::string& id() const;


      //! Make the IPbus client issue a dispatch
      void dispatch ();

      /**
      	A method to modify the timeout period for any pending or future transactions
      	@param aTimeoutPeriod the desired timeout period in milliseconds
      */
      void setTimeoutPeriod ( const uint32_t& aTimeoutPeriod );

      /**
      	A method to retrieve the timeout period currently being used
      	@return the timeout period currently being used in milliseconds
      */
      uint32_t getTimeoutPeriod();

      // /**
      // Ping the target for this client
      // */
      // void ping();

      /**
      	Retrieve the top-level node
      	@return the top-level node
      */
      const Node& getNode () const;

      /**
      	Retrieve the Node given by a full-stop delimeted name path relative, to the top-level node
      	@param aId a full-stop delimeted name path to a node, relative to the top-level node
      	@return the Node given by the identifier
      */
      const Node& getNode ( const std::string& aId ) const;

      /**
      	Retrieve the Node given by a full-stop delimeted name path relative, to the current node and cast it to a particular node type
      	@param aId a full-stop delimeted name path to a node, relative to the current node
      	@return the Node given by the identifier
      */
      template< typename T>
      const T& getNode ( const std::string& aId ) const;

      /**
      	Return all node IDs known to this HwInterface
      	@return all node IDs known to this HwInterface
      */
      std::vector<std::string> getNodes() const;

      /**
      	Return all node IDs known to this connection manager which match a (boost) regular expression
      	@param aRegex a string expression which is converted to a (boost) regular expression against which the node IDs are tested
      	@return all node IDs known to this connection manager
      */
      std::vector<std::string> getNodes ( const std::string& aRegex ) const;

    private:
      /**
      	A function which sets the HwInterface pointer in the Node to point to this HwInterface
      	@param aNode a Node that is to be claimed
      */
      void claimNode ( Node& aNode );

      //! A shared pointer to the IPbus client through which the transactions will be sent
      std::shared_ptr<ClientInterface> mClientInterface;

      //! A node tree
      std::shared_ptr<Node> mNode;
  };

}

#include "uhal/TemplateDefinitions/HwInterface.hxx"

#endif
