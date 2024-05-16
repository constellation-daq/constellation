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


#include <ostream>                           // for ostream, basic_ostream::put
#include <stdint.h>                          // for uint32_t, uint8_t, uint16_t, int32_t, ...

#include "uhal/log/log_inserter_helper.hpp"  // for RefWrapper


namespace uhal
{
  template< typename T >
  _Integer< T , IntFmt<> > Integer ( const T& aT )
  {
    return _Integer< T , IntFmt<> > ( aT );
  }

  template< typename T , integer_base BASE , integer_format FORMAT , uint32_t WIDTH > _Integer< T , IntFmt<BASE , FORMAT , WIDTH> > Integer ( const T& aT , const IntFmt<BASE , FORMAT , WIDTH>& )
  {
    return _Integer< T , IntFmt<BASE , FORMAT , WIDTH> > ( aT );
  }


  template< typename T >
  void sign_helper ( std::ostream&, const T& ) {}



  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<bin , fixed , WIDTH> >::print ( std::ostream& aStr ) const
  {
    uint32_t lSize ( sizeof ( T ) << 3 ); //number of characters
    aStr.write ( "0b" , 2 );
    int32_t i ( WIDTH-lSize );

    if ( i > 0 )
    {
      for ( ; i!=0 ; --i )
      {
        aStr.put ( '0' );
      }
    }

    T lValue ( RefWrapper<T>::value() );
    T lMask ( 0x1 );
    lMask <<= ( lSize-1 );

    for ( uint32_t i=0 ; i!=lSize ; ++i )
    {
      aStr.put ( ( lValue & lMask ) ?'1':'0' );
      lValue <<= 1;
    }
  }


  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<dec , fixed , WIDTH> >::print ( std::ostream& aStr ) const
  {
    static const char* lCharacterMapping ( "9876543210123456789" );
    static const char* lCharacterMappingCenter ( lCharacterMapping + 9 );
    char lBuffer[24]; //greater than the size of a 64bit decimal number
    char* lPtr = lBuffer;
    T value ( RefWrapper<T>::value() );
    T tmp_value;
    sign_helper ( aStr , value );

    do
    {
      tmp_value = value;
      value /= 10;
      *lPtr++ = * ( lCharacterMappingCenter + tmp_value - ( value * 10 ) );
    }
    while ( value );

    int32_t i ( WIDTH- ( lPtr-lBuffer ) );

    if ( i > 0 )
    {
      for ( ; i!=0 ; --i )
      {
        aStr.put ( '0' );
      }
    }

    do
    {
      aStr.put ( * ( --lPtr ) );
    }
    while ( lPtr!=lBuffer );
  }


  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<hex , fixed , WIDTH> >::print ( std::ostream& aStr ) const
  {
    uint32_t lSize ( sizeof ( T ) << 1 ); //number of characters
    static const char* lCharacterMapping ( "0123456789ABCDEF" );
    aStr.write ( "0x" , 2 );
    int32_t i ( WIDTH-lSize );

    if ( i > 0 )
    {
      for ( ; i!=0 ; --i )
      {
        aStr.put ( '0' );
      }
    }

    uint8_t* lStart ( ( uint8_t* ) ( & RefWrapper<T>::value() ) );
    uint8_t* lPtr ( lStart + sizeof ( T ) );

    do
    {
      --lPtr;
      aStr.put ( * ( lCharacterMapping + ( ( ( *lPtr ) &0xF0 ) >>4 ) ) );
      aStr.put ( * ( lCharacterMapping + ( ( ( *lPtr ) &0x0F ) ) ) );
    }
    while ( lPtr!=lStart );
  }




  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<bin , variable , WIDTH> >::print ( std::ostream& aStr ) const
  {
    if ( RefWrapper<T>::value() == T ( 0 ) )
    {
      aStr.write ( "0b0" , 3 );
    }
    else
    {
      uint32_t lSize ( sizeof ( T ) <<3 );
      aStr.write ( "0b" , 2 );
      T lValue ( RefWrapper<T>::value() );
      T lMask ( 0x1 );
      lMask <<= ( lSize-1 );
      bool lPrint ( false );
      bool lCurrent ( false );

      for ( uint32_t i=0 ; i!=lSize ; ++i )
      {
        lCurrent = lValue & lMask;

        if ( (lPrint |= lCurrent) )
        {
          aStr.put ( lCurrent?'1':'0' );
        }

        lValue <<= 1;
      }
    }
  }



  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<dec , variable , WIDTH> >::print ( std::ostream& aStr ) const
  {
    static const char* lCharacterMapping ( "9876543210123456789" );
    static const char* lCharacterMappingCenter ( lCharacterMapping + 9 );
    char lBuffer[24]; //greater than the size of a 64bit decimal number
    char* lPtr = lBuffer;
    T value ( RefWrapper<T>::value() );
    T tmp_value;
    sign_helper ( aStr , value );

    do
    {
      tmp_value = value;
      value /= 10;
      *lPtr++ = * ( lCharacterMappingCenter + tmp_value - ( value * 10 ) );
    }
    while ( value );

    do
    {
      aStr.put ( * ( --lPtr ) );
    }
    while ( lPtr!=lBuffer );
  }



  template< typename T , uint32_t WIDTH >
  void _Integer< T , IntFmt<hex , variable , WIDTH> >::print ( std::ostream& aStr ) const
  {
    static const char* lCharacterMapping ( "0123456789ABCDEF" );

    if ( RefWrapper<T>::value() == T ( 0 ) )
    {
      aStr.write ( "0x0" , 3 );
    }
    else
    {
      uint32_t lSize ( sizeof ( T ) );
      aStr.write ( "0x" , 2 );
      bool lPrint ( false );
      uint32_t lPos ( 0 );
      uint8_t* lStart ( ( uint8_t* ) ( & RefWrapper<T>::value() ) );
      uint8_t* lPtr ( lStart + lSize );

      do
      {
        --lPtr;
        lPos = ( ( ( *lPtr ) &0xF0 ) >>4 );

        if ( (lPrint |= ( bool ) ( lPos )) )
        {
          aStr.put ( * ( lCharacterMapping + lPos ) );
        }

        lPos = ( ( *lPtr ) &0x0F );

        if ( (lPrint |= ( bool ) ( lPos )) )
        {
          aStr.put ( * ( lCharacterMapping + lPos ) );
        }
      }
      while ( lPtr!=lStart );
    }
  }



  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint8_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int8_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint16_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int16_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint32_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int32_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< uint64_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< int64_t , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }

#if __SIZEOF_LONG__ == 4
  template< typename FORMAT >
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< long int , FORMAT >& aInt )
  {
    aInt.print ( aStr );
    return aStr;
  }
#endif

#ifdef __APPLE__ 
  template< typename FORMAT > 
  std::ostream& operator<< ( std::ostream& aStr , const uhal::_Integer< size_t , FORMAT >& aInt ) 
  { 
    aInt.print ( aStr ); 
    return aStr; 
  }
#endif

}
