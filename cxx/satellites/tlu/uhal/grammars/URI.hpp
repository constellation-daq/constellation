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

#ifndef _uhal_grammars_URI_hpp_
#define _uhal_grammars_URI_hpp_


#include <iosfwd>
#include <string>
#include <utility>   // for pair
#include <vector>


namespace uhal
{
  //! boost::fusion requires us to typedef our template types, so typedef a container which can hold key/value pairs
  typedef std::vector< std::pair<std::string, std::string> > NameValuePairVectorType;

  //! Struct to store a URI when parsed by boost spirit
  struct URI
  {
    //! The "protocol" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"
    std::string mProtocol;
    //! The "host" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"
    std::string mHostname;
    //! The "port" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"
    std::string mPort;
    //! The "patha/pathb/blah" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"
    std::string mPath;
    //! The "ext" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"
    std::string mExtension;
    //! The "key1=val1&key2=val2&key3=val3" part of a URI of the form "protocol://host:port/patha/pathb/blah.ext?key1=val1&key2=val2&key3=val3"	stored as a vector of key/val pairs
    NameValuePairVectorType mArguments;
  };

  std::ostream& operator<< ( std::ostream& aStr , const uhal::URI& aURI );


  std::string toString( const URI& aURI );


}


#endif
