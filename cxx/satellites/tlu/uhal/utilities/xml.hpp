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


#ifndef _uhal_utilities_xml_hpp_
#define _uhal_utilities_xml_hpp_


#include <stdint.h>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "uhal/log/exception.hpp"


// Forward declarations
namespace pugi
{
  class xml_node;
  struct xml_parse_result;
}


namespace uhal
{
  namespace exception
  {
    //! Exception class to handle the case where the string will not fit into a 32-bit number.
    UHAL_DEFINE_EXCEPTION_CLASS ( StringNumberWillNotFitInto32BitNumber , "Exception class to handle the case where the string will not fit into a 32-bit number." )

    //! Exception class to handle the case when a node attribute has the incorrect value.
    UHAL_DEFINE_EXCEPTION_CLASS ( NodeAttributeIncorrectValue , "Exception class to handle the case when a node attribute has the incorrect value." )
  }

  namespace utilities
  {
    /**
      Helper function to make debugging failures when parsing XML files easier
      @param aLoadResult the result of the parsing
      @param aPath the full filename of file whose parsing failed
      @param aFile a byte vector containing the contents of the file (stored like this because the file could be either local or retrieved by HTTP)
    */
    void PugiXMLParseResultPrettifier ( const pugi::xml_parse_result& aLoadResult , const boost::filesystem::path& aPath , const std::vector<uint8_t>& aFile );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , std::string& aTarget );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , int32_t& aTarget );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , uint32_t& aTarget );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , double& aTarget );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , float& aTarget );


    /**
      Helper function to retrieve a named attribute from a PugiXML node and cast it to the correct type
      @param aNode a node from which the attribute is to be extracted
      @param aAttrName the name of the attribute to be extracted
      @param aTarget a variable into which the attribute's value id to be written
      @return success/failure status
    */
    template < bool DebugInfo >
    bool GetXMLattribute ( const pugi::xml_node& aNode , const std::string& aAttrName , bool& aTarget );

  }
}


#endif
