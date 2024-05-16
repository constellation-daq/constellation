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

#ifndef _uhal_grammars_HttpResponseGrammar_hpp_
#define _uhal_grammars_HttpResponseGrammar_hpp_


#include <iosfwd>
#include <string>
#include <utility>   // for pair
#include <vector>

#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/struct/adapt_struct.hpp>
#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_grammar.hpp>


namespace uhal
{
  //! Struct to store an http response received from a server when parsed by boost spirit
  struct HttpResponseType
  {
    //! the http transport method
    std::string method;
    //! the http version number
    double version;
    //! the response status
    int status;
    //! the response status string
    std::string status_string;
    //! parsed headers
    std::string headers;
    //! parsed message content
    std::vector< uint8_t > content;
  };

  std::ostream& operator<< ( std::ostream& aStr , const uhal::HttpResponseType& aHttpResponse );
}


// Call to BOOST_FUSION_ADAPT_STRUCT must be at global scope
//! A boost::fusion adaptive struct used by the boost::qi parser
BOOST_FUSION_ADAPT_STRUCT (
  uhal::HttpResponseType,
  ( std::string, method )
  ( double , version )
  ( int, status )
  ( std::string, status_string )
  ( std::string, headers )
  ( std::vector< uint8_t >, content )
)


namespace uhal
{
namespace grammars
{
  //! A struct wrapping a set of rules as a grammar that can parse an HTTP response packet
  struct HttpResponseGrammar : boost::spirit::qi::grammar< std::vector<uint8_t>::iterator , uhal::HttpResponseType() ,	boost::spirit::ascii::space_type >
  {
    //! Default Constructor where we will define the boost::qi rules relating the members
    HttpResponseGrammar();
    //! Boost spirit parsing rule for parsing the incoming http packet
    boost::spirit::qi::rule< std::vector<uint8_t>::iterator,	uhal::HttpResponseType(),								boost::spirit::ascii::space_type > response;
    //! Boost spirit parsing rule for parsing the incoming http packet
    boost::spirit::qi::rule< std::vector<uint8_t>::iterator, std::string(),												boost::spirit::ascii::space_type > nospace_sequence;
    //! Boost spirit parsing rule for parsing the incoming http packet
    boost::spirit::qi::rule< std::vector<uint8_t>::iterator,	std::string(),											boost::spirit::ascii::space_type > nonewline_sequence;
    //! Boost spirit parsing rule for parsing the incoming http packet
    boost::spirit::qi::rule< std::vector<uint8_t>::iterator,	std::string(),											boost::spirit::ascii::space_type > noblankline_sequence;
  };

}
}


#endif
