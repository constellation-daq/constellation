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

#ifndef _uhal_ClientFactory_hpp_
#define _uhal_ClientFactory_hpp_


#include <map>
#include <memory>
#include <unordered_map>

#include "uhal/log/exception.hpp"
#include "uhal/ClientInterface.hpp"
#include "uhal/log/log.hpp"


namespace uhal
{
  namespace exception
  {
    //! Exception class to handle the case where the protocol requested does not exists in the creator map.
    UHAL_DEFINE_EXCEPTION_CLASS ( ProtocolDoesNotExist , "Exception class to handle the case where the protocol requested does not exists in the creator map." )

    //! Exception class to handle the case where the protocol for a user client has not been enabled.
    UHAL_DEFINE_EXCEPTION_CLASS ( ProtocolNotEnabled , "Exception class to handle the case where the protocol for a user client has not been enabled." )

    //! Exception class to handle the case where the factory failed to parse URI.
    UHAL_DEFINE_EXCEPTION_CLASS ( FailedToParseURI , "Exception class to handle the case where the factory failed to parse URI." )
  }

  /**
  A class to construct an IPbus client based on the protocol identifier specified
  NOTE! This is a factory method and must be Mutex protected if it is used in multithreaded environments!
  */

  class ClientFactory
  {

    private:
      /**
      	Default constructor
      	This is private since only a single instance is to be created, using the getInstance method
      */
      ClientFactory();

    public:

      ClientFactory(const ClientFactory&) = delete;
      ClientFactory& operator=(const ClientFactory&) = delete;

      //! Destructor
      virtual ~ClientFactory();

      /**
      	Static method to retrieve the single instance of the class
      	@return the single instance of the class
      */
      static ClientFactory& getInstance();

      /**
        Construct an IPbus client based on the protocol identifier specified
        @param aId the uinique identifier that the client will be given.
        @param aUri a string containing the full URI of the target. This string is parsed to extract the protocol, and it is this which is used to identify the relevent creator which is then used to create the client.
        @return a shared pointer to the newly created client
      */
      std::shared_ptr<ClientInterface> getClient ( const std::string& aId , const std::string& aUri );

      /**
        Construct an IPbus client based on the protocol identifier specified
        @param aId the uinique identifier that the client will be given.
        @param aUri a string containing the full URI of the target. This string is parsed to extract the protocol, and it is this which is used to identify the relevent creator which is then used to create the client.
        @param aUserClientMaps a vector of names of protocols for user clients that should be enabled
        @return a shared pointer to the newly created client
      */
      std::shared_ptr<ClientInterface> getClient ( const std::string& aId , const std::string& aUri, const std::vector<std::string>& aUserClientActivationList );

      /**
        Method to create an associate between a protocol identifier and a Creator of a particular type
        @param aProtocol the protocol identifier
        @param aDescription an optional description of the protocol
      */
      template <class T>
      void registerClient ( const std::string& aProtocol , const std::string& aDescription = "" );

      template <class T>
      class RegistrationHelper {
      public:
        //! Dummy variable required as initialisation target
        static bool sInitialised;

        static bool init(const std::string& aUri, const std::string& aDescription);
      };

    private:

      /**
        Method to create an associate between a protocol identifier and a Creator of a particular type
        @param aProtocol the protocol identifier
        @param aDescription an optional description of the protocol
      */
      template <class T>
      void add ( const std::string& aProtocol , const std::string& aDescription, bool aUserDefined );

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
          	Interface to a function which create a new IPbus client based on the protocol identifier specified
          	@param aId the uinique identifier that the client will be given.
          	@param aUri a string containing the full URI of the target. This string is parsed to extract the protocol, and it is this which is used to identify the relevent creator which is then used to create the client.
          	@return a shared pointer to the newly created client
          */
          virtual std::shared_ptr<ClientInterface> create ( const std::string& aId , const URI& aUri ) = 0;
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
          	@param aId the uinique identifier that the client will be given.
          	@param aUri a string containing the full URI of the target. This string is parsed to extract the protocol, and it is this which is used to identify the relevent creator which is then used to create the client.
          	@return a shared pointer to the newly created client
          */
          std::shared_ptr<ClientInterface> create ( const std::string& aId , const URI& aUri );
      };


    private:

      struct ClientInfo {
        std::shared_ptr<CreatorInterface> creator;
        bool userDefined;
        std::string description;
      };

      //! The single instance of the class
      static std::shared_ptr<ClientFactory> mInstance;
      //! Hash map associating a creator for a particular protocol with a file name
      std::unordered_map< std::string , ClientInfo > mClientMap; //map string name of each protocol to a creator for that protocol
  };

}

#define UHAL_REGISTER_EXTERNAL_CLIENT(clientclass, clienturi, clientdescription) \
  template<> bool uhal::ClientFactory::RegistrationHelper<clientclass>::sInitialised = \
    uhal::ClientFactory::RegistrationHelper<clientclass>::init(clienturi, clientdescription);


#include "uhal/TemplateDefinitions/ClientFactory.hxx"

#endif
