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

      Tom Williams, Rutherford Appleton Laboratory, Oxfordshire
      email: tom.williams <AT> cern.ch

---------------------------------------------------------------------------
*/

#ifndef _uhal_utilities_files_hpp_
#define _uhal_utilities_files_hpp_


#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "uhal/grammars/SemicolonDelimitedUriListGrammar.hpp"
#include "uhal/grammars/HttpResponseGrammar.hpp"
#include "uhal/grammars/URIGrammar.hpp"

#include "pugixml.hpp"

#include "uhal/log/log.hpp"
#include "uhal/log/exception.hpp"


namespace uhal
{
  namespace exception
  {
    //! Exception class to handle the case where the string is not a comma-delimiter list of URIs.
    UHAL_DEFINE_EXCEPTION_CLASS ( UriListParsingError , "Exception class to handle the case where the string is not a comma-delimiter list of URIs." )
    //! Exception class to handle the case where a URI contains a non-supported protocol.
    UHAL_DEFINE_EXCEPTION_CLASS ( NonSupportedUriProtocol , "Exception class to handle the case where a URI contains a non-supported protocol." )
    //! Exception class to handle the case where a URI can not be opened.
    UHAL_DEFINE_EXCEPTION_CLASS ( CannotOpenFile , "Exception class to handle the case where a URI can not be opened." )
    //! Exception class to handle the case where a URI using the 'file://' protocol can not be expanded.
    UHAL_DEFINE_EXCEPTION_CLASS ( FileNotFound , "Exception class to handle the case where a URI using the 'file://' protocol can not be expanded." )
    //!Exception class to handle the case where expanding a shell expression failed.
    UHAL_DEFINE_EXCEPTION_CLASS ( ExpandingShellExpressionFailed , "Exception class to handle the case where expanding a shell expression failed." )
  }

  namespace utilities
  {
    /**
    	Parse a semicolon delimited list of URIs into a vector of protocol/address pairs
    	@param aSemicolonDelimitedUriList a string containing a semicolon delimited list of URIs
    	@param aUriList a vector to which the extracted protocol/address pairs are appended
    */
    void ParseSemicolonDelimitedUriList ( const std::string& aSemicolonDelimitedUriList , std::vector< std::pair<std::string, std::string> >& aUriList );


    /**
    	Perform shell expansion of a linux shell expression ( e.g. "~/c*.xml" -> "/usr/home/awr/connections.xml" ) and convert into boost::filesystem::paths
    	@param aFilenameExpr a c-style string containing a linux shell expression to be expanded
    	@param aParentPath a path which will be prepended to relative file names
    	@param aFiles a pointer to a vector of boost::filesystem::paths onto which the returned file names are appended
    */
    void ShellExpandFilenameExpr ( const std::string& aFilenameExpr , const boost::filesystem::path& aParentPath , std::vector< boost::filesystem::path >& aFiles );


    /**
    	Retrieve a file by HTTP
    	@param aURL a URL to retrieve
    	@param aResponse a structure into which the returned HTTP packet is parsed
    	@return success/failure status
    */
    template < bool DebugInfo >
    bool HttpGet ( const std::string& aURL , HttpResponseType& aResponse );


    namespace detail 
    {
      typedef std::function<void (const std::string&, const boost::filesystem::path&, std::vector<uint8_t>& aCallback)> FileCallback_t;
    }


    /**
    	Given a linux shell expression, open all files which match it and call the callback function on each of them
    	@param aFilenameExpr a linux shell expression to be expanded
    	@param aParentPath a path which will be prepended to relative file names
    	@param aCallback a callback function to be called on each file matching the linux shell expression
    */
    void OpenFileLocal ( const std::string& aFilenameExpr , const boost::filesystem::path& aParentPath , const detail::FileCallback_t& aCallback);


    /**
    	Given a URL, retrieve the file and call the callback function on each of them
    	@param aURL a URL to retrieve
    	@param aCallback a callback function to be called on the retrieved URL
    	@return success/failure status
    */
    void OpenFileHttp ( const std::string& aURL , const detail::FileCallback_t& aCallback );

    /**
    	Given a protocol and either a URL or a linux shell expression, open the file and call the callback function on each of them
    	@param aProtocol the protocol to be used to retrieve the file
    	@param aFilenameExpr a linux shell expression to be expanded or a URL to be retrieved
    	@param aParentPath a path which will be prepended to relative file names for local files. No meaning for http files.
    	@param aCallback a callback function to be called on the files
    	@return success/failure status
    */
    void OpenFile ( const std::string& aProtocol , const std::string& aFilenameExpr , const boost::filesystem::path& aParentPath , const detail::FileCallback_t& aCallback );

  }
}


#endif
