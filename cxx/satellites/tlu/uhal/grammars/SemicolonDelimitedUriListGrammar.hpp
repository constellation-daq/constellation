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

#ifndef _uhal_grammars_SemicolonDelimitedUriListGrammar_hpp_
#define _uhal_grammars_SemicolonDelimitedUriListGrammar_hpp_


#include <string>
#include <utility>   // for pair
#include <vector>

#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_grammar.hpp>


namespace uhal
{
namespace grammars
{
  //! The BOOST::SPIRIT grammar for parsing the Semicolon delimited URI list into a vector of protocol-URI pairs
  struct SemicolonDelimitedUriListGrammar : boost::spirit::qi::grammar< std::string::const_iterator , std::vector< std::pair<std::string, std::string> >() , boost::spirit::ascii::space_type >
  {
    SemicolonDelimitedUriListGrammar();
    //! Boost spirit parsing rule for parsing a Semicolon delimited URI list
    boost::spirit::qi::rule< std::string::const_iterator , std::vector< std::pair<std::string, std::string> > () ,	boost::spirit::ascii::space_type > data_pairs_vector;
    //! Boost spirit parsing rule for parsing each entry within a Semicolon delimited URI list
    boost::spirit::qi::rule< std::string::const_iterator , std::pair<std::string, std::string>() ,					boost::spirit::ascii::space_type > data_pairs;
    //! Boost spirit parsing rule for parsing the "protocol" part of each entry in a Semicolon delimited URI list
    boost::spirit::qi::rule< std::string::const_iterator , std::string() ,											boost::spirit::ascii::space_type > data_pairs_1;
    //! Boost spirit parsing rule for parsing the "URI" part of each entry in a Semicolon delimited URI list
    boost::spirit::qi::rule< std::string::const_iterator , std::string() ,											boost::spirit::ascii::space_type > data_pairs_2;
  };
}
}


#endif
