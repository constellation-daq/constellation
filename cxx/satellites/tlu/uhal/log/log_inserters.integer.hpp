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

#ifndef _uhal_log_inserters_integer_hpp_
#define _uhal_log_inserters_integer_hpp_


#include <stdint.h>
#include <iostream>

#include <uhal/log/log_inserter_helper.hpp>


namespace uhal
{

  //! Enumerated type specifying the formatting option
  enum integer_base
  {
    bin, ///< Binary
    dec, ///< Decimal
    hex  ///< Hexadecimal
  };

  //! Enumerated type specifying the formatting option
  enum integer_format
  {
    fixed,   ///< Fixed width
    variable ///< Variable width
  };

  //! Constant which is the default formatting option
  static const integer_base DefaultIntegerBase ( dec );

  //! Constant which is the default formatting option
  static const integer_format DefaultIntegerFormat ( variable );

  //! Empty struct which acts as a dummy variable for passing the formatting information around
  template< integer_base BASE = DefaultIntegerBase , integer_format FORMAT = DefaultIntegerFormat , uint32_t WIDTH = 0 > struct IntFmt {};

  //! Forward declare an ultra-lightweight wrapper which does formatting of numbers only on demand
  template< typename T , typename FORMAT >
  class _Integer;

  //! Forward declare a function which creates an instance of the ultra-lightweight wrapper from an integer
  template< typename T > _Integer< T , IntFmt<> > Integer ( const T& aT );

  //! Forward declare a function which creates an instance of the ultra-lightweight wrapper from an integer
  template< typename T , integer_base BASE , integer_format FORMAT , uint32_t WIDTH > _Integer< T , IntFmt<BASE , FORMAT , WIDTH> > Integer ( const T& aT , const IntFmt<BASE , FORMAT , WIDTH>& aFmt );

  //! Helper function for adding the '+'/'-' sign
  template< typename T >
  void sign_helper ( std::ostream& aStr, const T& aInt );

  //! Helper function for adding the '+'/'-' sign to int8_ts
  template<>
  void sign_helper ( std::ostream& aStr, const int8_t& aInt );

  //! Helper function for adding the '+'/'-' sign to int16_ts
  template<>
  void sign_helper ( std::ostream& aStr, const int16_t& aInt );

  //! Helper function for adding the '+'/'-' sign to int32_ts
  template<>
  void sign_helper ( std::ostream& aStr, const int32_t& aInt );

  //! Helper function for adding the '+'/'-' sign to int64_ts
  template<>
  void sign_helper ( std::ostream& aStr, const int64_t& aInt );


  //! Declare an ultra-lightweight wrapper which does formatting of numbers only on demand
  template< typename T , typename FORMAT >
  class _Integer : public RefWrapper< T >
  {
    public:
      /**
      	Constructor
      	@param aT an integer type for which a reference is stored
      */
      _Integer ( const T& aT );

      /**
      The function which formats the integer and appends it into the given stream
      @param aStr a stream to which to append formatted data
      */
      void print ( std::ostream& aStr ) const;
  };


  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<bin , fixed , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };

  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<bin , variable , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };


  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<dec , fixed , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };

  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<dec , variable , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };

  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<hex , fixed , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };

  template< typename T , uint32_t WIDTH >
  class _Integer< T , IntFmt<hex , variable , WIDTH> > : public RefWrapper< T >
  {
    public:

      _Integer ( const T& aT ) : RefWrapper< T > ( aT ) {}
      void print ( std::ostream& aStr ) const;
  };



  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint8_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int8_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint16_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int16_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint32_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int32_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint64_t , FORMAT >& aInt );

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int64_t , FORMAT >& aInt );

#if __SIZEOF_LONG__ == 4
  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< long int , FORMAT >& aInt );
#endif

#ifdef __APPLE__ 
  template< typename FORMAT > 
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< size_t , FORMAT >& aInt ); 
#endif 

}


#include "uhal/log/log_inserters.integer.hxx"

#endif
