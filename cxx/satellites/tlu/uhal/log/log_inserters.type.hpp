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


#ifndef _uhal_log_inserters_type_hpp_
#define _uhal_log_inserters_type_hpp_


#include <iostream>
#include <stdint.h>

#include <uhal/log/log_inserter_helper.hpp>


namespace uhal
{

  template< typename T > class _Type;

  template< typename T > _Type< T > Type ( );
  template< typename T > _Type< T > Type ( const T& aT );

  template< typename T >
  std::ostream& operator<< ( std::ostream& aStr ,  const _Type< T >& aType );

  template< typename T >
  class _Type
  {
      friend _Type< T > Type<> ();
      friend _Type< T > Type<> ( const T& aT );
      _Type ( ) {}
  };

}


#include "uhal/log/log_inserters.type.hxx"

#endif
