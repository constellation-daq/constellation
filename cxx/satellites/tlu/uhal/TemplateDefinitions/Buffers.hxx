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


#include <stdint.h>  // for uint32_t, uint8_t
#include <string.h>  // for memcpy
#include <deque>     // for deque
#include <utility>   // for make_pair, pair
#include <vector>    // for vector


namespace uhal
{
  template< typename T >
  uint8_t* Buffers::send ( const T* aPtr )
  {
    uint32_t lSize ( sizeof ( T ) );
    uint8_t* lStartPtr ( &mSendBuffer[0]+mSendCounter );
    memcpy ( lStartPtr , aPtr , lSize );
    mSendCounter += lSize;
    return lStartPtr;
  }

  template< typename T >
  uint8_t* Buffers::send ( const T& aRef )
  {
    uint32_t lSize ( sizeof ( T ) );
    uint8_t* lStartPtr ( &mSendBuffer[0]+mSendCounter );
    memcpy ( lStartPtr , &aRef , lSize );
    mSendCounter += lSize;
    return lStartPtr;
  }

  template< typename T >
  void Buffers::receive ( T* aPtr )
  {
    uint32_t lSize ( sizeof ( T ) );
    mReplyBuffer.push_back ( std::make_pair ( ( uint8_t* ) ( aPtr ) , lSize ) );
    mReplyCounter += lSize;
  }

  template< typename T >
  void Buffers::receive ( T& aRef )
  {
    uint32_t lSize ( sizeof ( T ) );
    mReplyBuffer.push_back ( std::make_pair ( ( uint8_t* ) ( &aRef ) , lSize ) );
    mReplyCounter += lSize;
  }

}


