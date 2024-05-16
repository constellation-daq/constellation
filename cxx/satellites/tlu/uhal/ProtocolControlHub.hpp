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

#ifndef _uhal_ProtocolControlHub_hpp_
#define _uhal_ProtocolControlHub_hpp_

#include "uhal/ClientInterface.hpp"
#include "uhal/log/exception.hpp"
#include "uhal/log/log.hpp"

#include <iomanip>
#include <iostream>
#include <string>


namespace uhal
{

  namespace exception
  {
    //! Exception class to handle the case where parsing the target URL failed.
    UHAL_DEFINE_EXCEPTION_CLASS ( ParsingTargetURLfailed , "Exception class to handle the case where parsing the target URL failed." )

    //! Exception class to handle the case where hostname to IP lookup failed
    UHAL_DEFINE_EXCEPTION_CLASS ( HostnameToIPlookupFailed , "Exception class to handle the case where hostname to IP lookup failed." )

    //! Exception class to handle the case where the received header does not match the expected header.
    UHAL_DEFINE_EXCEPTION_CLASS ( XMLfileMissingRequiredParameters , "Exception class to handle the case where the received header does not match the expected header." )

    //! Exception class to handle the case where the target does not respond to the ControlHub
    UHAL_DEFINE_EXCEPTION_CLASS ( ControlHubReturnedWrongAddress , "Exception class to handle the case where the ControlHub returned the wrong IP or Port" )

    //! Exception class to handle the case where the target does not respond to the ControlHub
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( ControlHubTargetTimeout , ClientTimeout , "Exception class to handle the case where the target does not respond to the ControlHub" )

    //! Exception class to handle cases in which the ControlHub returns a non-zero error code (excluding target timeouts)
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( ControlHubErrorCodeSet, PacketLevelError , "Exception class to handle cases in which the ControlHub returns a non-zero error code (excluding target timeouts)" )
  }

  /**
    Extract an IP-address and port number from a URI object
    @param aUri a URI object to be parsed
    @return a pair containing an IP-address (first) and port number (second)
  */
  std::pair< uint32_t , uint16_t > ExtractTargetID ( const URI& aUri );


  //! Transport protocol to transfer an IPbus buffer via ControlHub
  template < typename InnerProtocol >
  class ControlHub : public InnerProtocol
  {

    public:
      /**
        Constructor
        @param aId the uinique identifier that the client will be given.
        @param aUri a struct containing the full URI of the target.
      */
      ControlHub ( const std::string& aId, const URI& aUri );

      //! Destructor
      virtual ~ControlHub();

    protected:

      /**
      	Add a preamble to an IPbus buffer
        @param aBuffers a buffer to which to add the preamble
      */
      virtual void preamble ( std::shared_ptr< Buffers > aBuffers );

      /**
        Get the size of the preamble added by this protocol layer
        @return the size of the preamble added by this protocol layer
      */
      virtual uint32_t getPreambleSize();

      /**
      	Finalize an IPbus buffer before it is transmitted
        @param aBuffers a buffer on which to do the predispatch operation
      */
      virtual void predispatch ( std::shared_ptr< Buffers > aBuffers );

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
          std::deque< std::pair< uint8_t* , uint32_t > >::iterator aReplyEndIt );

      /**
        Returns the maximum number of buffers that should be in-flight from the uHAL client at any given time. 
        @return the maximum number of buffers in-flight
        @note Currently set to 60 based on experience of performance measurements in building 904 IPbus test.
        @note Since TCP allows the ControlHub to throttle incoming traffic, in the long term this "number in-flight" limit may not been needed. 
        @note But this limit is kept for the moment for safety (i.e. to prevent uHAL ever flooding ControlHub with data).
      */
      virtual uint32_t getMaxNumberOfBuffers();

      //! Function which tidies up this protocol layer in the event of an exception
      virtual void dispatchExceptionHandler();

    private:
      static void translateErrorCode(std::ostream& aStream, const uint16_t& aErrorCode);

      //! The IP address of the target device that is connected to the Control Hub
      uint32_t mDeviceIPaddress;

      //! The port number of the target device that is connected to the Control Hub
      uint16_t mDevicePort;

      //! A struct representing the preamble which will be prepended to an IPbus buffer for the benefit of the Control Hub
      struct tpreamble
      {
        //! The number of 32-bit words in the IPbus packet (legacy and could be removed)
        uint16_t* mSendWordCountPtr;

        //! A legacy counter
        uint32_t mReplyChunkByteCounter;
        //! The returned target device ID (IP address)
        uint32_t mReplyDeviceIPaddress;
        //! The returned target device ID (port number)
        uint16_t mReplyDevicePort;
        //! An error code returned describing the status of the control hub
        uint16_t mReplyErrorCode;
      };

      //! A queue of preample structs making the memory used by the preambles persistent during the dispatch. Must lock mPreamblesMutex when accessing this deque.
      std::deque< tpreamble > mPreambles;
      //! Mutex to be used when accessing mPreambles
      std::mutex mPreamblesMutex;
  };


}

#endif
