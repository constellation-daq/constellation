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

#include <sstream>

namespace uhal
{

  template < typename R >
  BaseFunctionObject<R>::BaseFunctionObject()
  {
  }


  template < typename R >
  BaseFunctionObject<R>::~BaseFunctionObject()
  {
  }




  template < typename R , typename T >
  FunctionObject<R,T>::FunctionObject ( T& aT ) :
    mT ( aT )
  {
  }


  template < typename R , typename T >
  R FunctionObject<R,T>::operator() ( const pugi::xml_node& aNode )
  {
    return ( mT ) ( aNode );
  }


  template < typename R , typename T >
  FunctionObject<R,T*>::FunctionObject ( T* aT ) :
    mT ( aT )
  {
  }


  template < typename R , typename T >
  R FunctionObject<R,T*>::operator() ( const pugi::xml_node& aNode )
  {
    return ( *mT ) ( aNode );
  }




  template < typename R >
  Rule<R>::Rule( ) :
    mRuleId ( 0 ),
    mRequiredHash ( 0x0000000000000000 ),
    mForbiddenHash ( 0x0000000000000000 ),
    mFuncPtr ( NULL )
  {
  }


  template < typename R >
  Rule<R>::~Rule()
  {
    if ( mFuncPtr )
    {
      delete mFuncPtr;
      mFuncPtr = NULL;
    }
  }


  template < typename R >
  Rule<R>& Rule<R>::require ( const std::string& aStr )
  {
    if ( mForbidden.find ( aStr ) != mForbidden.end() )
    {
      exception::ContradictoryParserRule lExc;
      log ( lExc , "Contradictory rule for attribute ", Quote ( aStr ) );
      throw lExc;
    }

    mRequired.insert ( aStr );
    return *this;
  }


  template < typename R >
  Rule<R>& Rule<R>::forbid ( const std::string& aStr )
  {
    if ( mRequired.find ( aStr ) != mRequired.end() )
    {
      exception::ContradictoryParserRule lExc;
      log ( lExc , "Contradictory rule for attribute ", Quote ( aStr ) );
      throw lExc;
    }

    mForbidden.insert ( aStr );
    return *this;
  }


  template < typename R >
  Rule<R>& Rule<R>::optional ( const std::string& aStr )
  {
    mOptional.insert ( aStr );
    return *this;
  }


  template < typename R >
  std::string Rule<R>::description() const
  {
    std::set<std::string>::const_iterator lIt;
    std::stringstream lStr;
    lStr << "Rule " << mRuleId ;
    lIt = mRequired.begin();

    if ( lIt != mRequired.end() )
    {
      lStr << " {Require : ";

      while ( true )
      {
        lStr << *lIt ;

        if ( ( ++lIt ) == mRequired.end() )
        {
          break;
        }

        lStr<< ", ";
      }

      lStr << "}";
    }

    lIt = mForbidden.begin();

    if ( lIt != mForbidden.end() )
    {
      lStr << " {Forbid : ";

      while ( true )
      {
        lStr << *lIt ;

        if ( ( ++lIt ) == mForbidden.end() )
        {
          break;
        }

        lStr<< ", ";
      }

      lStr << "}";
    }

    lIt = mOptional.begin();

    if ( lIt != mOptional.end() )
    {
      lStr << " {Optional : ";

      while ( true )
      {
        lStr << *lIt ;

        if ( ( ++lIt ) == mOptional.end() )
        {
          break;
        }

        lStr<< ", ";
      }

      lStr << "}";
    }

    return lStr.str();
  }


  template < typename R >
  R Rule<R>::operator() ( const pugi::xml_node& aNode )
  {
    if ( mFuncPtr )
    {
      return ( *mFuncPtr ) ( aNode );
    }
    else
    {
      exception::NoActionSpecified lExc;
      log ( lExc , "No action specified!" );
      throw lExc;
    }
  }




  template < typename R >
  Parser<R>::Parser() :
    mNextHash ( 0x0000000000000001 ),
    mRuleCounter ( 0 )
  {
    mHashes.clear();
  }


  template < typename R >
  Parser<R>::~Parser()
  {
  }


  template < typename R >
  template < typename T >
  void Parser<R>::addRule ( const Rule<R> & aRule , T aCallbackHandler )
  {
    mRules.push_back ( aRule );
    Rule<R>& lRule ( mRules.back() );

    for ( std::set<std::string>::iterator lIt = lRule.mRequired.begin() ; lIt != lRule.mRequired.end() ; ++lIt )
    {
      std::unordered_map< std::string , uint64_t >::iterator lIt2 ( mHashes.find ( *lIt ) );

      if ( lIt2 == mHashes.end() )
      {
        if ( mNextHash == 0x0000000000000000 )
        {
          exception::TooManyAttributes lExc;
          log ( lExc , "Too many attributes" );
          throw lExc;
        }

        lRule.mRequiredHash |= mNextHash;
        mHashes.insert ( std::make_pair ( *lIt , mNextHash ) );
        mNextHash <<= 1;
      }
      else
      {
        lRule.mRequiredHash |= lIt2->second;
      }
    }

    for ( std::set<std::string>::iterator lIt = lRule.mForbidden.begin() ; lIt != lRule.mForbidden.end() ; ++lIt )
    {
      std::unordered_map< std::string , uint64_t >::iterator lIt2 ( mHashes.find ( *lIt ) );

      if ( lIt2 == mHashes.end() )
      {
        if ( mNextHash == 0x0000000000000000 )
        {
          exception::TooManyAttributes lExc;
          log ( lExc , "Too many attributes" );
          throw lExc;
        }

        lRule.mForbiddenHash |= mNextHash;
        mHashes.insert ( std::make_pair ( *lIt , mNextHash ) );
        mNextHash <<= 1;
      }
      else
      {
        lRule.mForbiddenHash |= lIt2->second;
      }
    }

    for ( std::set<std::string>::iterator lIt = lRule.mOptional.begin() ; lIt != lRule.mOptional.end() ; ++lIt )
    {
      std::unordered_map< std::string , uint64_t >::iterator lIt2 ( mHashes.find ( *lIt ) );

      if ( lIt2 == mHashes.end() )
      {
        if ( mNextHash == 0x0000000000000000 )
        {
          exception::TooManyAttributes lExc;
          log ( lExc , "Too many attributes" );
          throw lExc;
        }

        mHashes.insert ( std::make_pair ( *lIt , mNextHash ) );
        mNextHash <<= 1;
      }
    }

    lRule.mRuleId = ++mRuleCounter;
    lRule.mFuncPtr = new FunctionObject<R,T> ( aCallbackHandler );
  }


  template < typename R >
  R Parser<R>::operator() ( const pugi::xml_node& aNode )
  {
    uint64_t lHash ( 0x0000000000000000 );

    for ( pugi::xml_attribute lAttr = aNode.first_attribute(); lAttr; lAttr = lAttr.next_attribute() )
    {
      std::unordered_map< std::string , uint64_t >::iterator lIt2 ( mHashes.find ( lAttr.name() ) );

      if ( lIt2 == mHashes.end() )
      {
        exception::UnknownAttribute lExc;
        log ( lExc , "Parser failed because of unknown attribute ", Quote ( lAttr.name() ) );
        throw lExc;
      }

      lHash |= lIt2->second;
    }

    std::deque< Rule<R>* > lPassed;
    std::deque< Rule<R>* > lFailedRequired;
    std::deque< Rule<R>* > lFailedForbidden;
    int i ( 0 );

    for ( typename std::deque< Rule<R> >::iterator lIt = mRules.begin() ; lIt != mRules.end() ; ++lIt, ++i )
    {
      if ( lIt->mForbiddenHash & lHash )
      {
        lFailedForbidden.push_back ( & ( *lIt ) );
      }
      else if ( lIt->mRequiredHash & ~lHash )
      {
        lFailedRequired.push_back ( & ( *lIt ) );
        continue;
      }
      else
      {
        lPassed.push_back ( & ( *lIt ) );
      }
    }

    if ( lPassed.size() == 1 )
    {
      return ( *lPassed.front() ) ( aNode );
    }

    if ( lPassed.size() > 1 )
    {
      log ( Warning() , "Ambiguity! " , Integer ( lPassed.size() ) ," rules passed. Attempting to find the rule with the most stringent requirements." );
      Rule<R>* lMostStringent ( NULL );
      uint32_t lCounter ( 0 );

      for ( typename std::deque< Rule<R>* >::iterator lIt = lPassed.begin(); lIt != lPassed.end(); ++lIt )
      {
        if ( lCounter < ( **lIt ).mRequired.size() )
        {
          lMostStringent = ( *lIt );
          lCounter = ( **lIt ).mRequired.size();
        }
        else if ( lCounter == ( **lIt ).mRequired.size() )
        {
          lMostStringent = NULL;
        }
      }

      if ( !lMostStringent )
      {
        exception::AmbiguousParserRules lExc;
        log ( lExc , "Ambiguity remains! Multiple rules passed " , Integer ( lCounter ) , " requirements." );
        throw lExc;
      }

      log ( Warning() , "In ambiguous case, selected " , lMostStringent->description() );
      return ( *lMostStringent ) ( aNode );
    }

    std::stringstream lStr;

    for ( pugi::xml_attribute lAttr = aNode.first_attribute(); lAttr; lAttr = lAttr.next_attribute() )
    {
      lStr << lAttr.name() << "=\"" << lAttr.value() << "\", ";
    }

    std::string lString ( lStr.str() );
    lString.resize ( lString.size() - 2 );
    exception::NoRulesPassed lExc;
    log ( lExc , "Node with attributes : " , lString , " failed all parser rules because : " );

    for ( typename std::deque< Rule<R>* >::iterator lIt = lFailedRequired.begin() ; lIt != lFailedRequired.end(); ++lIt )
    {
      std::stringstream lStr;
      uint64_t lTemp ( ( **lIt ).mRequiredHash & ~lHash );

      for ( std::unordered_map< std::string , uint64_t >::iterator lIt2 = mHashes.begin() ; lIt2 != mHashes.end() ; ++lIt2 )
      {
        if ( ( lIt2->second ) & lTemp )
        {
          lStr << "\"" << lIt2->first << "\", ";
        }
      }

      std::string lString ( lStr.str() );
      lString.resize ( lString.size() - 2 );
      log ( lExc , "Rule " ,  Integer ( ( **lIt ).mRuleId ) , " requires attributes : " , lString );
    }

    for ( typename std::deque< Rule<R>* >::iterator lIt = lFailedForbidden.begin() ; lIt != lFailedForbidden.end(); ++lIt )
    {
      std::stringstream lStr;
      uint64_t lTemp ( ( **lIt ).mForbiddenHash & lHash );

      for ( std::unordered_map< std::string , uint64_t >::iterator lIt2 = mHashes.begin() ; lIt2 != mHashes.end() ; ++lIt2 )
      {
        if ( ( lIt2->second ) & lTemp )
        {
          lStr << "\"" << lIt2->first << "\", ";
        }
      }

      std::string lString ( lStr.str() );
      lString.resize ( lString.size() - 2 );
      log ( lExc , "Rule " ,  Integer ( ( **lIt ).mRuleId ) , " forbids attributes : " , lString );
    }

    throw lExc;
  }

}

