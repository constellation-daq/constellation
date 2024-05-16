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

#ifndef _uhal_Buffers_hpp_
#define _uhal_Buffers_hpp_


#include <deque>
#include <stdint.h>         // for uint32_t, uint8_t
#include <utility>          // for pair
#include <vector>           // for vector

#include "uhal/ValMem.hpp"


namespace uhal
{

  //! A class wrapping the send and recieve buffers that are to be filled and transported and the validated memories associated with them
  class Buffers
  {
    public:

      /**
      	Constructor
      	@param aMaxSendSize The size of the buffer (in bytes) in the target device for receiving IPbus data packets from uhal.
      	@warning Used to set internal buffer size, not for checking
      */
      Buffers ( const uint32_t& aMaxSendSize = 65536 );

      //! Destructor
      virtual ~Buffers();

      /**
      	Get the number of bytes that are currently in the send buffer
      	@return the number of bytes that are currently in the send buffer
      */
      const uint32_t& sendCounter();

      /**
      	Get the number of bytes that are currently expected by the reply buffer
      	@return the number of bytes that are currently expected by the reply buffer
      */
      const uint32_t& replyCounter();

      /**
      	Helper function to copy an object to the send buffer
      	@param aPtr a pointer to an object to be copied
      	@return a pointer to the copy of the object in the send buffer
      */
      template< typename T >
      uint8_t* send ( const T* aPtr );

      /**
      	Helper function to copy an object to the send buffer
      	@param aRef a reference to an object to be copied
      	@return a pointer to the copy of the object in the send buffer
      */
      template< typename T >
      uint8_t* send ( const T& aRef );

      /**
      	Helper function to copy a block of memory to the send buffer
      	@param aPtr a pointer to the start of the memory to be copied
      	@param aSize the number of bytes to be copied
      	@return a pointer to the copy of the memory block in the send buffer
      */
      uint8_t* send ( const uint8_t* aPtr , const uint32_t& aSize );

      /**
      	Helper function to add a destination object to the reply queue
      	@param aPtr a pointer to some persistent object which can be written to when the transaction is performed
      */
      template< typename T >
      void receive ( T* aPtr );

      /**
      	Helper function to add a destination object to the reply queue
      	@param aRef a reference to some persistent object which can be written to when the transaction is performed
      */
      template< typename T >
      void receive ( T& aRef );

      /**
      	Helper function to add a destination memory location to the reply queue
      	@param aPtr a pointer to some persistent memory which can be written to when the transaction is performed
      	@param aSize the number of bytes which can be safely written
      */
      void receive ( uint8_t* aPtr , const uint32_t& aSize );

      /**
      	Helper function to associate a validated memory with this buffer so that it is guaranteed to exist when the transaction is performed
      	@param aValMem a validated memory to be associated with this buffer
      */
      void add ( const ValHeader& aValMem );

      /**
      	Helper function to associate a validated memory with this buffer so that it is guaranteed to exist when the transaction is performed
      	@param aValMem a validated memory to be associated with this buffer
      */
      void add ( const ValWord< uint32_t >& aValMem );

      /**
      	Helper function to associate a validated memory with this buffer so that it is guaranteed to exist when the transaction is performed
      	@param aValMem a validated memory to be associated with this buffer
      */
      void add ( const ValVector< uint32_t >& aValMem );

      /**
      	Get a pointer to the start of the send buffer
      	@return a pointer to the start of the send buffer
      */
      uint8_t* getSendBuffer();

      /**
      	Get a reference to the reply queue
      	@return a reference to the reply queue
      */
      std::deque< std::pair< uint8_t* , uint32_t > >& getReplyBuffer();

      //! Helper function to mark all validated memories associated with this buffer as valid
      void validate ();

      //! Clear the counters and the reply buffers
      void clear();

    private:
      //! The number of bytes that are currently in the send buffer
      uint32_t mSendCounter;
      //! The number of bytes that are currently expected by the reply buffer
      uint32_t mReplyCounter;

      //! The start location of the memory buffer
      std::vector<uint8_t> mSendBuffer;
      //! The queue of reply destinations
      std::deque< std::pair< uint8_t* , uint32_t > > mReplyBuffer;

      //! Deque holding validated memories so that they are guaranteed to exist when the transaction is performed
      std::deque< ValHeader > mValHeaders;
      //! Deque holding validated memories so that they are guaranteed to exist when the transaction is performed
      std::deque< ValWord< uint32_t > > mUnsignedValWords;
      //! Deque holding validated memories so that they are guaranteed to exist when the transaction is performed
      std::deque< ValVector< uint32_t > > mUnsignedValVectors;
  };

}

#include "uhal/TemplateDefinitions/Buffers.hxx"

#endif
