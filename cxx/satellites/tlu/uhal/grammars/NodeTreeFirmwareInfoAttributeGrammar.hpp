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

#ifndef _uhal_grammars_NodeTreeFirmwareInfoAttributeGrammar_hpp_
#define _uhal_grammars_NodeTreeFirmwareInfoAttributeGrammar_hpp_


#include <string>
#include <utility>   // for pair
#include <vector>

#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_grammar.hpp>


namespace uhal
{
  //! boost::fusion requires us to typedef our template types, so typedef a container which can hold key/value pairs
  typedef std::vector< std::pair<std::string, std::string> > NameValuePairVectorType;

  /**
    Struct to store the name and member variables within a node endpoint attribute when parsed by boost spirit
    The endpoint attribute has the form "endpoint;name1=val1;name2=val2;name3=val3;" where the name-value pairs are optional, 
    as is the trailing semicolon
  */
  struct NodeTreeFirmwareInfoAttribute
  {
    //! The endpoint type
    std::string mType;
    //! The member variable of the endpoint stored as "name1=val1;name2=val2;name3=val3"
    NameValuePairVectorType mArguments;
  };
}


// Call to BOOST_FUSION_ADAPT_STRUCT must be at global scope
//! A boost::fusion adaptive struct used by the boost::qi parser
BOOST_FUSION_ADAPT_STRUCT (
  uhal::NodeTreeFirmwareInfoAttribute,
  ( std::string , mType )
  ( uhal::NameValuePairVectorType, mArguments )
)


namespace uhal
{
namespace grammars
{
  //! A struct wrapping a set of rules as a grammar that can parse a NodeTreeFirmwareInfoAttribute of the form "endpoint;name1=val1;name2=val2;name3=val3"
  struct NodeTreeFirmwareinfoAttributeGrammar : boost::spirit::qi::grammar<std::string::const_iterator, uhal::NodeTreeFirmwareInfoAttribute(), boost::spirit::ascii::space_type>
  {
    //! Default Constructor where we will define the boost::qi rules relating the members
    NodeTreeFirmwareinfoAttributeGrammar();
    //! Boost spirit parsing rule for parsing a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	uhal::NodeTreeFirmwareInfoAttribute(), boost::spirit::ascii::space_type > start;
    //! Boost spirit parsing rule for parsing the "endpoint" part of a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	std::string(),											boost::spirit::ascii::space_type > endpoint;
    //! Boost spirit parsing rule for parsing all the "name-value pairs" of a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	std::vector< std::pair<std::string, std::string> > (),	boost::spirit::ascii::space_type > data_pairs_vector; //NameValuePairVectorType
    //! Boost spirit parsing rule for parsing each of the "name-value pairs" of a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	std::pair<std::string, std::string>(),					boost::spirit::ascii::space_type > data_pairs;
    //! Boost spirit parsing rule for parsing the "name" part of the "name-value pairs" of a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	std::string(),											boost::spirit::ascii::space_type > data_pairs_1;
    //! Boost spirit parsing rule for parsing the "value" part of the "name-value pairs" of a NodeTreeFirmwareInfoAttribute
    boost::spirit::qi::rule< std::string::const_iterator,	std::string(),											boost::spirit::ascii::space_type > data_pairs_2;
  };
}
}


#endif
