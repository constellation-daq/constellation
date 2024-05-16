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


#ifdef __GNUG__
#include <cxxabi.h>
#endif
#include <ostream>   // for operator<<, ostream, size_t
#include <typeinfo>  // for type_info


namespace uhal
{

  template< typename T >
  std::ostream& operator<< ( std::ostream& aStr ,  const _Type< T >& )
  {
#ifdef __GNUG__
    // this is fugly but necessary due to the way that typeid::name() returns the object type name under g++.
    int lStatus ( 0 );
    static std::size_t lSize ( 1024 );
    static char* lDemangled = new char[lSize];
    aStr << ( abi::__cxa_demangle ( typeid ( T ).name() , lDemangled , &lSize , &lStatus ) );
#else
    aStr << ( typeid ( T ).name() );
#endif
    return aStr;
  }

  template< typename T >
  _Type< T > Type ( )
  {
    return _Type< T > ( );
  }

  template< typename T >
  _Type< T > Type ( const T& )
  {
    return _Type< T > ( );
  }

}

