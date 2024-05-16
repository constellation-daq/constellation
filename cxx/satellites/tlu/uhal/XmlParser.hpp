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

#ifndef _uhal_XmlParser_hpp_
#define _uhal_XmlParser_hpp_

#include <string>
#include <deque>
#include <set>

#include "pugixml.hpp"
#include "uhal/log/log.hpp"
#include "uhal/log/exception.hpp"

namespace uhal
{

  namespace exception
  {
    //! Exception class to handle the case where an attribute is both required and forbidden.
    UHAL_DEFINE_EXCEPTION_CLASS ( ContradictoryParserRule , "Exception class to handle the case where an attribute is both required and forbidden." )
    //! Exception class to handle the case where a callback is requested without it being specified.
    UHAL_DEFINE_EXCEPTION_CLASS ( NoActionSpecified , "Exception class to handle the case where a callback is requested without it being specified." )
    //! Exception class to handle the case where the parser is asked to handle more than 64 attributes.
    UHAL_DEFINE_EXCEPTION_CLASS ( TooManyAttributes , "Exception class to handle the case where the parser is asked to handle more than 64 attributes." )
    //! Exception class to handle the case where an unknown attribute is parsed.
    UHAL_DEFINE_EXCEPTION_CLASS ( UnknownAttribute , "Exception class to handle the case where an unknown attribute is parsed." )
    //! Exception class to handle the case where two or more equally strict rules are passed.
    UHAL_DEFINE_EXCEPTION_CLASS ( AmbiguousParserRules , "Exception class to handle the case where two or more equally strict rules are passed." )
    //! Exception class to handle the case where no rules were parsed.
    UHAL_DEFINE_EXCEPTION_CLASS ( NoRulesPassed , "Exception class to handle the case where no rules were parsed." )
  }

  //! Forward declaration of the Parser so we can declare it friend
  template < typename R >
  class Parser;

  //! Abstract base class for wrapping function pointers, function objects and bound functions as objects
  template < typename R >
  class BaseFunctionObject
  {
    public:
      //! Default constructor
      BaseFunctionObject();
      //! Destructor
      virtual ~BaseFunctionObject();
      /**
        Functor which converts an XML node to an object of template type R
        @param aNode an XML node for converting to an object
        @return an object of template type R
      */
      virtual R operator() ( const pugi::xml_node& aNode ) = 0;
  };

  //! Class for wrapping bound functions and function objects as an object
  template < typename R , typename T >
  class FunctionObject : public BaseFunctionObject<R>
  {
    public:
      /**
        Constructor
        @param aT a reference to a bound object or function object which will be evaluated when the object is evaluated (bracket operator)
      */
      FunctionObject ( T& aT );

      /**
        Functor which converts an XML node to an object of template type R
        @param aNode an XML node for converting to an object
        @return an object of template type R
      */
      R operator() ( const pugi::xml_node& aNode );

    private:
      /**
        The function object or bound function which will be called when the object is evaluated (bracket operator)
      */
      T mT;
  };

  //! Class for wrapping function pointer as an object
  template < typename R , typename T >
  class FunctionObject<R,T*> : public BaseFunctionObject<R>
  {
    public:
      /**
        Constructor
        @param aT a function pointer which will be evaluated when the object is evaluated (bracket operator)
      */
      FunctionObject ( T* aT );

      /**
        Functor which converts an XML node to an object of template type R
        @param aNode an XML node for converting to an object
        @return an object of template type R
      */
      R operator() ( const pugi::xml_node& aNode );

    private:
      /**
        Store the function pointer which will be called when the object is evaluated (bracket operator)
      */
      T* mT;
  };



  //! Rule for matching XML attributes
  template < typename R >
  class Rule
  {
      //! Make the Parser a friend of the Rule
      friend class Parser< R >;
    public:
      //! Default constructor
      Rule( );

      //! Destructor
      virtual ~Rule();

      /**
        Add a required attribute to the rule
        @param aStr a required attribute to the rule
        @return this rule object for chaining instruction calls
      */
      Rule<R>& require ( const std::string& aStr );

      /**
        Add an forbidden attribute to the rule
        @param aStr an forbidden attribute to the rule
        @return this rule object for chaining instruction calls
      */
      Rule<R>& forbid ( const std::string& aStr );

      /**
        Add an optional attribute to the rule
        @param aStr an optional attribute to the rule
        @return this rule object for chaining instruction calls
      */
      Rule<R>& optional ( const std::string& aStr );

      /**
        A function to return a string description of the rule
        @return a string description of the rule 
      */
      std::string description() const;

    private:

      /**
        Functor which converts an XML node to an object of template type R
        (Calls the function pointer, if it is not NULL)
        @param aNode an XML node for converting to an object
        @return an object of template type R
      */
      R operator() ( const pugi::xml_node& aNode );

    private:
      //! The required attributes for this rule
      std::set<std::string> mRequired;
      //! The forbidden attributes for this rule
      std::set<std::string> mForbidden;
      //! The optional attributes for this rule
      std::set<std::string> mOptional;

      //! The ID of the rule
      uint32_t mRuleId;
      //! The hash for required attributes
      uint64_t mRequiredHash;
      //! The hash for forbidden attributes
      uint64_t mForbiddenHash;

      //! An object wrapping the function pointer for the function to be called when the rule conditions are met
      BaseFunctionObject<R>* mFuncPtr;
  };

  //! Parser class which converts an XML node to an object of type R
  template < typename R >
  class Parser
  {
    public:
      //! Default constructor
      Parser();

      //! Destructor
      ~Parser();

      /**
        Method to add the rules to the parser
        @param aRule a rule object specifying what to match
        @param aCallbackHandler a function callback to be performed when the rule conditions are met
      */
      template < typename T >
      void addRule ( const Rule<R> & aRule , T aCallbackHandler );

      /**
        Functor which converts an XML node to an object of template type R
        @param aNode an XML node for converting to an object
        @return an object of template type R
      */
      R operator() ( const pugi::xml_node& aNode );

    private:
      //! One-hot encoded hash for rules
      uint64_t mNextHash;
      //! Map of the tags to the one-hot encoded hash
      std::unordered_map< std::string , uint64_t > mHashes;
      //! Container for storing rule objects
      std::deque< Rule<R> > mRules;
      //! Member to track rule numbers for giving each rule a unique ID
      uint32_t mRuleCounter;
  };

}

#include "uhal/TemplateDefinitions/XmlParser.hxx"

#endif
