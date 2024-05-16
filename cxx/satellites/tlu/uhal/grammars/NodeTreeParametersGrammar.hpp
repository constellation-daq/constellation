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

#ifndef _uhal_grammars_NodeTreeParametersGrammar_hpp_
#define _uhal_grammars_NodeTreeParametersGrammar_hpp_


#include <string>
#include <unordered_map>
#include <utility>   // for pair

#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_grammar.hpp>


namespace uhal
{
namespace grammars
{
  //! A struct wrapping a set of rules as a grammar that can parse a NodeTreeParametersGrammar of the form "name1=val1;name2=val2;name3=val3"
  struct NodeTreeParametersGrammar : boost::spirit::qi::grammar<std::string::const_iterator, std::unordered_map<std::string, std::string>()>
  {
    //! Default Constructor where we will define the boost::qi rules relating the members
    NodeTreeParametersGrammar();
    //! Boost spirit parsing rule for parsing a NodeTree Parameters attribute    
    boost::spirit::qi::rule<std::string::const_iterator, std::unordered_map<std::string, std::string>()> query;
    //! Boost spirit parsing rule for parsing each of the "name-value pairs" of a NodeTree Parameters attribute
    boost::spirit::qi::rule<std::string::const_iterator, std::pair<std::string, std::string>()> pair;
    //! Boost spirit parsing rule for parsing the "name" part of the "name-value pairs" of a NodeTreeParameters attribute
    boost::spirit::qi::rule<std::string::const_iterator, std::string()> key;
    //! Boost spirit parsing rule for parsing the "value" part of the "name-value pairs" of a NodeTreeParameters attribute
    boost::spirit::qi::rule<std::string::const_iterator, std::string()> value;
  };
}
}

#endif
