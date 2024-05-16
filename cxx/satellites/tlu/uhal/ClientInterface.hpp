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
	@author Marc Magrans De Abril
	@date 2012
*/

#ifndef _uhal_ClientInterface_hpp_
#define _uhal_ClientInterface_hpp_


#include <deque>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "uhal/grammars/URI.hpp"
#include "uhal/log/exception.hpp"
#include "uhal/definitions.hpp"
#include "uhal/ValMem.hpp"


namespace uhal
{
  // Forward declaration
  class Buffers;
  class ClientInterface;
  class HwInterface;
  class IPbusCore;
  class Node;

  namespace detail
  {
    std::string getAddressDescription(const ClientInterface&, const uint32_t, const size_t&);
  }

  namespace exception
  {
    //! Exception class to handle the case where pinging of a client failed.
    UHAL_DEFINE_EXCEPTION_CLASS ( PingFailed , "Exception class to handle the case where pinging of a client failed." )

    //! Exception class to handle the case where a masked write was attempted with a data source which has non-zero bits outside the bit-mask's bounds.
    UHAL_DEFINE_EXCEPTION_CLASS ( BitsSetWhichAreForbiddenByBitMask , "Exception class to handle the case where a masked write was attempted with a data source which has non-zero bits outside the bit-mask's bounds." )

    //! Exception class to handle the case where we were unable to validate the packet.
    UHAL_DEFINE_EXCEPTION_CLASS ( ValidationError , "Exception class to handle the case where we were unable to validate the packet." )

    //! Exception class to handle a NULL buffer being passed to the transport class.
    UHAL_DEFINE_EXCEPTION_CLASS ( NullBufferException , "Exception class to handle a NULL buffer being passed to the transport class." )

    UHAL_DEFINE_EXCEPTION_CLASS ( PacketLevelError, "Base exception class covering situations in which a packet-level error occurs.")

    UHAL_DEFINE_EXCEPTION_CLASS ( TransactionLevelError, "Base exception class covering situations in which a transaction-level error occurs (i.e. error occurs in individual read/write).")

    UHAL_DEFINE_EXCEPTION_CLASS ( ClientTimeout, "Base exception class covering timeouts when waiting for reply from device")

    UHAL_DEFINE_EXCEPTION_CLASS ( TransportLayerError, "Base exception class covering non-timeout transport-layer-specific errors.")

    UHAL_DEFINE_EXCEPTION_CLASS ( InvalidURI, "Exception class for invalid URIs." )
  }

  //! An abstract base class for defining the interface to the various IPbus clients as well as providing the generalized packing functionality
  class ClientInterface
  {
    protected:
      /**
      	Constructor
      	@param aId the uinique identifier that the client will be given.
      	@param aUri a struct containing the full URI of the target.
        @param aTimeoutPeriod the default timeout period for the protocol
      */
      ClientInterface ( const std::string& aId, const URI& aUri , const boost::posix_time::time_duration& aTimeoutPeriod );

    private:
      //! Default Constructor
      ClientInterface ();

      /**
      	Copy Constructor
      	@param aClientInterface a ClientInterface to copy
      */
      ClientInterface ( const ClientInterface& aClientInterface );

      /**
      	Assignment operator
      	@param aClientInterface a ClientInterface to copy
      	@return reference to this object for chained assignment
      */
      virtual ClientInterface& operator= ( const ClientInterface& aClientInterface );

    public:
      //! Destructor
      virtual ~ClientInterface();

      /**
      	Return the identifier of the target for this client
      	@return the identifier of the target for this client
      */
      const std::string& id() const;

      // /**
      // Ping the target for this client
      // */
      // void ping();

      /**
      	Return the url of the target for this client
      	@return the url of the target for this client
      */
      const std::string& uri() const;

      //! Method to dispatch all queued transactions, and wait until all corresponding responses have been received
      void dispatch ();

      /**
      	A method to modify the timeout period for any pending or future transactions
        @warning Protected by user mutex, so only for use from user side (not from client code)
      	@param aTimeoutPeriod the desired timeout period in milliseconds
      */
      void setTimeoutPeriod ( const uint32_t& aTimeoutPeriod  = 0 );

      /**
      	A method to retrieve the timeout period currently being used
        @warning Protected by user mutex, so only for use from user side (not from client code)
      	@return the timeout period currently being used in milliseconds
      */
      uint64_t getTimeoutPeriod();

    protected:
      /**
      	A method to retrieve the timeout period currently being used
        @warning NOT protected by user mutex, so only for use from client side (not from user code - hence protected)
      	@return the timeout period currently being used as a boost time_duration
      */    
      const boost::posix_time::time_duration& getBoostTimeoutPeriod();

    public:

      /**
      	Write a single, unmasked word to a register
      	@param aAddr the address of the register to write
      	@param aValue the value to write to the register
      */
      ValHeader write ( const uint32_t& aAddr, const uint32_t& aValue );

      /**
      	Write a single, masked word to a register
      	@param aAddr the address of the register to write
      	@param aValue the value to write to the register
      	@param aMask the mask to apply to the value
      */
      ValHeader write ( const uint32_t& aAddr, const uint32_t& aValue, const uint32_t& aMask );

      /**
      	Write a block of data to a block of registers or a block-write port
      	@param aAddr the address of the register to write
      	@param aValues the values to write to the registers or a block-write port
      	@param aMode whether we are writing to a block of registers (INCREMENTAL) or a block-write port (NON_INCREMENTAL)
      */
      ValHeader writeBlock ( const uint32_t& aAddr, const std::vector< uint32_t >& aValues, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL );

      /**
      	Read a single, unmasked, unsigned word
      	@param aAddr the address of the register to read
      	@return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > read ( const uint32_t& aAddr );

      /**
      	Read a single, masked, unsigned word
      	@param aAddr the address of the register to read
      	@param aMask the mask to apply to the value after reading
      	@return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > read ( const uint32_t& aAddr, const uint32_t& aMask );

      /**
      	Read a block of unsigned data from a block of registers or a block-read port
      	@param aAddr the lowest address in the block of registers or the address of the block-read port
      	@param aSize the number of words to read
      	@param aMode whether we are reading from a block of registers (INCREMENTAL) or a block-read port (NON_INCREMENTAL)
      	@return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValVector< uint32_t > readBlock ( const uint32_t& aAddr, const uint32_t& aSize, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL );

      /**
      	Read the value of a register, apply the AND-term, apply the OR-term, set the register to this new value and return a copy of the original value to the user
      	@param aAddr the address of the register to read, modify, write
      	@param aANDterm the AND-term to apply to existing value in the target register
      	@param aORterm the OR-term to apply to existing value in the target register
      	@return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > rmw_bits ( const uint32_t& aAddr , const uint32_t& aANDterm , const uint32_t& aORterm );

      /**
      	Read the value of a register, add the addend, set the register to this new value and return a copy of the original value to the user
      	@param aAddr the address of the register to read, modify, write
      	@param aAddend the addend to add to the existing value in the target register
      	@return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > rmw_sum ( const uint32_t& aAddr , const int32_t& aAddend );

    protected:
      /**
        Pure virtual function which actually performs the dispatch operation
        @param aBuffers the buffer to be dispatched
      */
      virtual void implementDispatch ( std::shared_ptr< Buffers > aBuffers ) = 0;

      //! Virtual function to dispatch all buffers and block until all replies are received
      virtual void Flush( );


      //! Send a byte order transaction
      virtual ValHeader implementBOT( ) = 0;

      /**
      Write a single, unmasked word to a register
      @param aAddr the address of the register to write
      @param aValue the value to write to the register
      */
      virtual ValHeader implementWrite ( const uint32_t& aAddr, const uint32_t& aValue ) = 0;

      /**
      Write a block of data to a block of registers or a block-write port
      @param aAddr the address of the register to write
      @param aValues the values to write to the registers or a block-write port
      @param aMode whether we are writing to a block of registers (INCREMENTAL) or a block-write port (NON_INCREMENTAL)
      */
      virtual ValHeader implementWriteBlock ( const uint32_t& aAddr, const std::vector< uint32_t >& aValues, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL ) = 0;

      /**
      Read a single, masked, unsigned word
      @param aAddr the address of the register to read
      @param aMask the mask to apply to the value after reading
      @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRead ( const uint32_t& aAddr, const uint32_t& aMask = defs::NOMASK ) = 0;

      /**
      Read a block of unsigned data from a block of registers or a block-read port
      @param aAddr the lowest address in the block of registers or the address of the block-read port
      @param aSize the number of words to read
      @param aMode whether we are reading from a block of registers (INCREMENTAL) or a block-read port (NON_INCREMENTAL)
      @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValVector< uint32_t > implementReadBlock ( const uint32_t& aAddr, const uint32_t& aSize, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL ) = 0;


      /**
      Read the value of a register, apply the AND-term, apply the OR-term, set the register to this new value and return a copy of the new value to the user
      @param aAddr the address of the register to read, modify, write
      @param aANDterm the AND-term to apply to existing value in the target register
      @param aORterm the OR-term to apply to existing value in the target register
      @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRMWbits ( const uint32_t& aAddr , const uint32_t& aANDterm , const uint32_t& aORterm ) = 0;

      /**
      Read the value of a register, add the addend, set the register to this new value and return a copy of the new value to the user
      @param aAddr the address of the register to read, modify, write
      @param aAddend the addend to add to the existing value in the target register
      @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRMWsum ( const uint32_t& aAddr , const int32_t& aAddend ) = 0;

      //! Add a preamble to an IPbus buffer
      virtual void preamble ( std::shared_ptr< Buffers > aBuffers );

      //! Return the size of the preamble
      virtual uint32_t getPreambleSize();

      /**
      	Finalize the buffer before it is transmitted
        @param aBuffers the buffer to finalize before dispatch
      */
      virtual void predispatch ( std::shared_ptr< Buffers > aBuffers );

      /**
        Helper function to create a ValHeader object
        @return a std::pair containing the ValHeader object and a pointer to the underlying memory object
      */
      std::pair < ValHeader , _ValHeader_* > CreateValHeader();

      /**
        Helper function to create a ValWord object
        @param aValue an initial value
        @param aMask a bit-mask for selecting a subset of bits in the word
        @return a std::pair containing the ValWord object and a pointer to the underlying memory object
      */      
      std::pair < ValWord<uint32_t> , _ValWord_<uint32_t>* > CreateValWord ( const uint32_t& aValue , const uint32_t& aMask = defs::NOMASK );
      
      /**
        Helper function to create a ValVector object
        @param aSize the size of the ValVector
        @return a std::pair containing the ValVector object and a pointer to the underlying memory object
      */          
      std::pair < ValVector<uint32_t> , _ValVector_<uint32_t>* > CreateValVector ( const uint32_t& aSize );

      /**
        	Function which dispatch calls when the reply is received to check that the headers are as expected
          @param aBuffers the buffer to validate when it is propagated with replies
        	@return whether the returned packet is valid
        */
      virtual  exception::exception* validate ( std::shared_ptr< Buffers > aBuffers );


    protected:
      /**
      	Function which the dispatch calls when the reply is received to check that the headers are as expected
      	@param aSendBufferStart a pointer to the start of the first word of IPbus data which was sent (i.e. with no preamble)
      	@param aSendBufferEnd a pointer to the end of the last word of IPbus data which was sent
      	@param aReplyStartIt an iterator to the start of the list of memory locations in to which the reply was written
      	@param aReplyEndIt an iterator to the end (one past last valid entry) of the list of memory locations in to which the reply was written
      	@return whether the returned IPbus packet is valid
      */
      virtual  exception::exception* validate ( uint8_t* aSendBufferStart ,
          uint8_t* aSendBufferEnd ,
          std::deque< std::pair< uint8_t* , uint32_t > >::iterator aReplyStartIt ,
          std::deque< std::pair< uint8_t* , uint32_t > >::iterator aReplyEndIt ) = 0;

      //! Function which is called when an exception is thrown
      virtual void dispatchExceptionHandler();

      /**
        Function to return a buffer to the buffer pool
        @param aBuffers a shared-pointer to a buffer to be returned to the buffer pool
      */
      void returnBufferToPool ( std::shared_ptr< Buffers >& aBuffers );
      /**
        Function to return a collection of buffers to the buffer pool
        @param aBuffers a collection of shared-pointers to a buffer to be returned to the buffer pool
      */
      void returnBufferToPool ( std::deque< std::shared_ptr< Buffers > >& aBuffers );
      /**
        Function to return a collection of buffers to the buffer pool
        @param aBuffers a collection of shared-pointers to a buffer to be returned to the buffer pool
      */
      void returnBufferToPool ( std::vector< std::shared_ptr< Buffers > >& aBuffers );
      /**
        Function to return a collection of buffers to the buffer pool
        @param aBuffers a collection of shared-pointers to a buffer to be returned to the buffer pool
      */
      void returnBufferToPool ( std::deque< std::vector< std::shared_ptr< Buffers > > >& aBuffers );

    private:
      /**
        If the current buffer is null, allocate a buffer from the buffer pool for it
        If the buffer pool is empty, create 10 new buffers
      */
      void updateCurrentBuffers();
      void deleteBuffers();


    private:
      //! A MutEx lock used to make sure the access functions are thread safe
      std::mutex mUserSideMutex;
      
      //! A MutEx lock used to make sure the access to the buffers is thread safe
      std::mutex mBufferMutex;

      //! A memory pool of buffers which will be dispatched
      std::deque < std::shared_ptr< Buffers > > mBuffers;

#ifdef NO_PREEMPTIVE_DISPATCH
      //! A deque to store buffers pending dispatch for the case where pre-emptive dispatch is disabled
      std::deque < std::shared_ptr< Buffers > > mNoPreemptiveDispatchBuffers;
#endif

      //! A pointer to a buffer-wrapper object
      std::shared_ptr< Buffers > mCurrentBuffers;

      //! the identifier of the target for this client
      std::string mId;

      //! Timeout period for transactions
      boost::posix_time::time_duration mTimeoutPeriod;

      std::weak_ptr<Node> mNode;

      friend class IPbusCore;
      friend class HwInterface;
      friend std::string detail::getAddressDescription(const ClientInterface&, const uint32_t, const size_t&);

    protected:

      //! a struct containing the full URI of the target for this client
      URI mUri;

      std::string mUriString;

      /**
        Function which checks the available space in the currently filling buffer against requested send and receive sizes and, if there is insufficient space in the currently filling buffer, then dispatch it and create a new buffer
        @param aSendSize the amount of data that the current instruction wishes to send
        @param aReplySize the amount of data that the current instruction expects to receive
        @param aAvailableSendSize return the amount of space available for outgoing IPbus packets
        @param aAvailableReplySize return the amount of space available for incoming IPbus packets
      */
      virtual std::shared_ptr< Buffers > checkBufferSpace ( const uint32_t& aSendSize , const uint32_t& aReplySize , uint32_t& aAvailableSendSize , uint32_t& aAvailableReplySize );

      /**
        Return the maximum number of packets in flight
        @return the maximum number of packets in flight
      */
      virtual uint32_t getMaxNumberOfBuffers() = 0;
      /**
        Return the maximum size to be sent based on the buffer size in the target
        @return the maximum size to be sent
      */
      virtual uint32_t getMaxSendSize() = 0;
      /**
        Return the maximum size of reply packet based on the buffer size in the target
        @return the maximum size of reply packet
      */
      virtual uint32_t getMaxReplySize() = 0;
  };

}

#endif

