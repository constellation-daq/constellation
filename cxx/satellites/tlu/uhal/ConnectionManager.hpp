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

#ifndef _uhal_ConnectionManager_hpp_
#define _uhal_ConnectionManager_hpp_


#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "uhal/log/exception.hpp"
#include "uhal/HwInterface.hpp"


// Forward declarations
namespace pugi
{
  class xml_node;
}

namespace boost
{
  class mutex;
}


namespace uhal
{
  namespace exception
  {
    //! Exception class to handle the case where the supposedly unique ID is duplicated.
    UHAL_DEFINE_EXCEPTION_CLASS ( DuplicatedUID , "Exception class to handle the case where the supposedly unique ID is duplicated." )
    //! Exception class to handle the case where the UID requested does not exists in the map of connections.
    UHAL_DEFINE_EXCEPTION_CLASS ( ConnectionUIDDoesNotExist , "Exception class to handle the case where the UID requested does not exists in the map of connections." )
  }

  //! A class to open and manage XML connection files and wrap up the interfaces to the NodeTreeBuilder and the ClientFactory
  class ConnectionManager
  {
    public:
      //! A struct to hold the fields of each entry in the XML connections file
      struct ConnectionDescriptor
      {
        /**
        	Constructor
        	@param aNode a PugiXML node from which to extract the details of the connection
        	@param aConnectionFile the connection file which this entry is contained in
        	@param aSuccess return whether all the necessary values were extracted from the PugiXML node
        */
        ConnectionDescriptor ( const pugi::xml_node& aNode , const boost::filesystem::path& aConnectionFile , bool& aSuccess );

        /**
        	Comparison operation
        	@param aConnectionDescriptor another ConnectionDescriptor to compare with
        	@return whether the two ConnectionDescriptors are identical
        */
        bool operator== ( const ConnectionDescriptor& aConnectionDescriptor ) const;

        //! An identifier for an individual
        std::string id;
        //! The full uri for making the connection
        std::string uri;
        //! The address table for building the node tree
        std::string address_table;
        //! The connection file which provided this entry
        boost::filesystem::path connection_file;
      };

    public:

      ConnectionManager(const ConnectionManager&) = delete;
      ConnectionManager& operator=(const ConnectionManager&) = delete;

      /**
      	Default constructor
      	Given a semi-colon delimeted list of glob expressions, parse all the files matching it (e.g. $BUILD/config/c*.xml). If one parsing fails throw an exception and return filename and line number
      	@param aFilenameExpr a semi-colon delimeted list of glob expressions specifying one or more XML connection files to load
      */
      ConnectionManager ( const std::string& aFilenameExpr );

      ConnectionManager ( const std::string& aFilenameExpr , const std::vector<std::string>& aUserClientActivationList );

      //! Destructor
      virtual ~ConnectionManager ();

      /**
      	Retrieves protocol, host, and port from the connection file to create an IPbus Client
      	Retrieves the address table file from the connection file to create the Node tree
      	Puts the two together to create a full HwInterface
      	@param aId the unique identifier for the connection
      	@return a HwInterface which encapsulates the Node tree and the IPbus Client
       */
      HwInterface getDevice ( const std::string& aId );

      /**
      	Return all device IDs known to this connection manager
      	@return all device IDs known to this connection manager
      */
      std::vector<std::string> getDevices ( ) const;

      /**
      	Return all device IDs known to this connection manager which match a (boost) regular expression
      	@param aRegex a string expression which is converted to a (boost) regular expression against which the device IDs are tested
      	@return all device IDs known to this connection manager
      */
      std::vector<std::string> getDevices ( const std::string& aRegex ) const;

      /**
      	Use the specified protocol, host, and port to create an IPbus Client
      	Use the specified address table to create the Node tree
      	Puts the two together to create a full HwInterface
      	@param aId the unique identifier for the connection
      	@param aUri the URI string detailing how the connection is made
      	@param aAddressFileExpr a file expression pointing to exactly one local or remote address file (note. this assumes a semi-colon delimited list which can contain glob file expressions, etc. This expression  is parsed and evaluated, and the file count checked at runtime.)
      	@return a HwInterface which encapsulates the Node tree and the IPbus Client
       */
      static HwInterface getDevice ( const std::string& aId , const std::string& aUri , const std::string& aAddressFileExpr );

      static HwInterface getDevice ( const std::string& aId , const std::string& aUri , const std::string& aAddressFileExpr , const std::vector<std::string>& aUserClientActivationList );

      //! Clears cache of Node tree structure for previously-opened address files (thread safe)
      static void clearAddressFileCache();

    private:
      //! A mutex lock to protect access to the factory methods in multithreaded environments
      static std::mutex mMutex;

      /**
      	Method called once the file specified in the constructor has been opened
      	@param aProtocol The protocol by which the file was loaded
      	@param aPath The fully qualified path to the file which has been opened
      	@param aFile A byte vector containing the content of the opened file. Done like this since the routine handles local and http files identically
      */
      void CallBack ( const std::string& aProtocol , const boost::filesystem::path& aPath , std::vector<uint8_t>& aFile );

      const std::vector<std::string> mUserClientActivationList;

      //! A map of connection identifiers to stucts containing details of the parsed XML node
      std::map< std::string, ConnectionDescriptor >  mConnectionDescriptors;	//connection identifier, parsed descriptor (also contains the connection identifier)

      //! A set of previously opened filenames, so that the same file is not parsed multiple times
      std::set< std::string > mPreviouslyOpenedFiles;
  };

}

#endif

