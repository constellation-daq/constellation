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

#ifndef _uhal_ProtocolTCP_hpp_
#define _uhal_ProtocolTCP_hpp_


#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/deadline_timer.hpp>

#include "uhal/ClientInterface.hpp"
#include "uhal/log/exception.hpp"
#include "uhal/utilities/TimeIntervalStats.hpp"

namespace uhal
{
  // Forward declarations
  class Buffers;
  struct URI;


  namespace exception
  {
    //! Exception class to handle the case where the TCP connection timed out.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( TcpTimeout , ClientTimeout , "Exception class to handle the case where the TCP connection timed out." )
    //! Exception class to handle a failure to create a TCP socket.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( ErrorAtTcpSocketCreation , TransportLayerError , "Exception class to handle a failure to create a TCP socket." )
    //! Exception class to handle the case where the error flag was raised in the asynchronous callback system.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( TcpConnectionFailure , TransportLayerError , "Exception class to handle the case where the TCP connection was refused or aborted." )
    //! Exception class to handle the case where ASIO returned an error.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( ASIOTcpError , TransportLayerError , "Exception class to handle the case where ASIO returned an error." )

  }

  //! Transport protocol to transfer an IPbus buffer via TCP
  template < typename InnerProtocol , std::size_t nr_buffers_per_send >
  class TCP : public InnerProtocol
  {

    private:
      /**
        Copy Constructor
        This creates a new socket, dispatch queue, dispatch thread, etc. which connects to the same target ip/port
        @param aTCP a TCP-protocol object to copy
      */
      TCP ( const TCP& aTCP ); // non-copyable

      /**
        Assignment operator
        This reassigns the endpoint, closes the existing socket and cleans up the buffers, etc. On the next call which requires the socket, it will be reopened with the new endpoint.
        @param aTCP a TCP-protocol object to copy
        @return reference to the current object to allow chaining of assignments
      */
      TCP& operator= ( const TCP& aTCP ); // non-assignable

    public:
      /**
      	Constructor
      	@param aId the unique identifier that the client will be given.
      	@param aUri a struct containing the full URI of the target.
      */
      TCP ( const std::string& aId, const URI& aUri );

      //! Destructor
      virtual ~TCP();

    private:
      /**
      	Send the IPbus buffer to the target, read back the response and call the packing-protocol's validate function
      	@param aBuffers the buffer object wrapping the send and receive buffers that are to be transported
      	If multithreaded, adds buffer to the dispatch queue and returns. If single-threaded, calls the dispatch-worker dispatch function directly and blocks until the response is validated.
      */
      void implementDispatch ( std::shared_ptr< Buffers > aBuffers );

      //! Concrete implementation of the synchronization function to block until all buffers have been sent, all replies received and all data validated
      virtual void Flush( );

      //! Function which tidies up this protocol layer in the event of an exception
      virtual void dispatchExceptionHandler();

      /**
        Return the maximum size to be sent based on the buffer size in the target
        @return the maximum size to be sent
      */
      uint32_t getMaxSendSize();

      /**
        Return the maximum size of reply packet based on the buffer size in the target
        @return the maximum size of reply packet
      */
      uint32_t getMaxReplySize();

      //! Make the TCP connection
      void connect();

      /**
        Initialize performing the next TCP write operation
        In multi-threaded mode, this runs the ASIO async write and exits
        In single-threaded mode, this runs the ASIO async write and blocks
      */
      void write ( );

      /**
        Callback function which is called upon completion of the ASIO async write
        This, then, makes a call to read to read back the reply to what has just been sent
        @param aErrorCode the error code with which the ASIO operation completed
      */
      void write_callback ( const boost::system::error_code& aErrorCode , std::size_t aBytesTransferred );

      /**
        Initialize performing the next TCP read operation
        In multi-threaded mode, this runs the ASIO async read and exits
        In single-threaded mode, this runs the ASIO async read and blocks
      */
      void read ( );

      /**
        Callback function which is called upon completion of the ASIO async read
        This, then, checks the queue to see if there are more packets to be sent and if so, calls write
        @param aErrorCode the error code with which the ASIO operation completed
      */
      void read_callback ( const boost::system::error_code& aErrorCode , std::size_t aBytesTransferred );

      //! Function called by the ASIO deadline timer
      void CheckDeadline();

      /**
        Function to set the value of a variable associated with a BOOST conditional-variable and then notify that conditional variable
        @param aValue a value to which to update the variable associated with a BOOST conditional-variable
      */
      void NotifyConditionalVariable ( const bool& aValue );

      //! Function to block a thread pending a BOOST conditional-variable and its associated regular variable
      void WaitOnConditionalVariable();


    private:
      typedef std::chrono::steady_clock SteadyClock_t;

      //! The maximum UDP payload size (in bytes)
      size_t mMaxPayloadSize;

      //! The boost::asio::io_service used to create the connections
      boost::asio::io_service mIOservice;

      //! A shared pointer to a boost::asio tcp socket through which the operation will be performed
      boost::asio::ip::tcp::socket mSocket;

      //! A shared pointer to a boost::asio tcp endpoint - used by the delayed (open-on-first-use) connect
      boost::asio::ip::tcp::resolver::iterator mEndpoint;

      //! The mechanism for providing the time-out
      boost::asio::deadline_timer mDeadlineTimer;

      /// Needed when multi-threading to stop the boost::asio::io_service thinking it has nothing to do and so close the socket
      boost::asio::io_service::work mIOserviceWork;

      //! The Worker thread in Multi-threaded mode
      std::thread mDispatchThread;

      //! A MutEx lock used to make sure the access functions are thread safe
      std::mutex mTransportLayerMutex;

      //! The list of buffers still waiting to be sent
      std::deque < std::shared_ptr< Buffers > > mDispatchQueue;
      //! The list of buffers still awaiting a reply
      std::deque < std::pair<std::vector< std::shared_ptr< Buffers > >, SteadyClock_t::time_point> > mReplyQueue;

      //! Counter of how many writes have been sent, for which no reply has yet been received
      uint32_t mPacketsInFlight;

      //! Boolean specifying whether or not the main thread is within TCP::Flush method. Its value checked by the worker thread to know whether it should wait for more packets before sending onto the TCP socket.
      bool mFlushStarted;

      //! A mutex for use by the conditional variable
      std::mutex mConditionalVariableMutex;
      //! A conditional variable for blocking the main thread until the variable with which it is associated is set correctly
      std::condition_variable mConditionalVariable;
      //! A variable associated with the conditional variable which specifies whether all packets have been sent and all replies have been received
      bool mFlushDone;

      /**
        Variable storing "number of bytes to follow" field for the TCP chunk currently being sent.
        @note Having this field in the TCP stream increases the efficiency (specifically, throughput) of sending data over the TCP stream, since the server application can wait for the whole chunk to arrive before unpacking it. 
        @note I.e. with this field in the TCP stream, the server (e.g. ControlHub) can handle more data in each TCP receive call, and therefore doesn't have to call TCP receive function so often.
        @note Similarly uHAL handles more data in each TCP send call, and therefore doesn't have to call the TCP send function so often.
      */
      uint32_t mSendByteCounter;

      /**
        Variable used to store "number of bytes to follow" field for the next/current TCP chunk being received.
        @note Having this field in the TCP stream increases the efficiency (specifically, throughput) of receiving data, since uHAL can wait for the whole chunk to arrive before unpacking it.
        @note I.e. with this field in the TCP stream, uHAL can handle more data in each TCP receive call, and therefore doesn't have to call the TCP receive function so often.
      */
      uint32_t mReplyByteCounter;

      /**
        The buffers containing the payload for the send operation that's currently in progress
        @note When communicating with the ControlHub it is more efficient to send as much data as possible - i.e. multiple IPbus packets - to minimise the number of TCP chunks that are unpacked at each end of the TCP connection.
      */
      std::vector< std::shared_ptr< Buffers > > mDispatchBuffers;

      /**
        The buffers containing the payloads for the receive operation that's currently in progress
        @note When communicating with the ControlHub it is more efficient to send as much data as possible - i.e. multiple IPbus packets - to minimise the number of TCP chunks that are unpacked at each end of the TCP connection.
      */
      std::pair< std::vector< std::shared_ptr< Buffers > >, SteadyClock_t::time_point > mReplyBuffers;

      /**
        A pointer to an exception object for passing exceptions from the worker thread to the main thread.
        Exceptions must always be created on the heap (i.e. using `new`) and deletion will be handled in the main thread
      */
      uhal::exception::exception* mAsynchronousException;


      SteadyClock_t::time_point mLastSendQueued;
      SteadyClock_t::time_point mLastRecvQueued;

      TimeIntervalStats mRTTStats;
      TimeIntervalStats mLSTStats;
      TimeIntervalStats mInterSendTimeStats;
      TimeIntervalStats mInterRecvTimeStats;
  };

}


#endif
