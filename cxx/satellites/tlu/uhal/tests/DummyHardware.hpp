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
	@date 2013
*/

#ifndef _uhal_tests_DummyHardware_hpp_
#define _uhal_tests_DummyHardware_hpp_


#include <chrono>
#include <deque>
#include <vector>

#include "uhal/IPbusInspector.hpp"


namespace uhal
{
  namespace tests 
  {

    //! The mask for the address space (size of the address space in one larger than this) 
    static const uint32_t ADDRESSMASK = 0x00FFFFFF;
    //! The size of the reply history for IPbus 2.0
    static const uint32_t REPLY_HISTORY_DEPTH = 5;
    //! Size of the receive and reply buffers
    static const uint32_t BUFFER_SIZE = 100000;
  

    //! Common abstract base class for IPbus 1.3 and 2.0 dummy hardware
    class DummyHardwareInterface {
    public:
      DummyHardwareInterface(const std::chrono::microseconds& aReplyDelay) :
        mReplyDelay(aReplyDelay)
      {
      }

      virtual ~DummyHardwareInterface() {}

        //! Function which "starts" the dummy hardware; does not return until the 'stop' method is called
        virtual void run() = 0;

        //! Stops this dummy hardware instance - i.e. makes the 'run' method return
        virtual void stop() = 0;

        template <class DurationType>
        void setReplyDelay(const DurationType& aDelay)
        {
          mReplyDelay = aDelay;
        }

      protected:
        //! The delay in seconds between the request and reply of the first transaction
        std::chrono::microseconds mReplyDelay;
    };


    //! Abstract base class to emulate IPbus hardware
    template< uint8_t IPbus_major , uint8_t IPbus_minor >
    class DummyHardware : public DummyHardwareInterface, public HostToTargetInspector< IPbus_major , IPbus_minor >
    {
        typedef HostToTargetInspector< IPbus_major , IPbus_minor > base_type;
  
      public:
        /**
          Constructor
          @param aReplyDelay a time delay between the reply and response for the first transaction
          @param aBigEndianHack whether we are using the dummy hardware with a client which uses the big-endian hack.
        */
        DummyHardware ( const uint32_t& aReplyDelay, const bool& aBigEndianHack );
  
        virtual ~DummyHardware();

      protected:
        /**
          Function which analyses the received IPbus packet and creates the suitable response
          @param aByteCount the number of bytes received
        */
        void AnalyzeReceivedAndCreateReply ( const uint32_t& aByteCount );
  
        void SetEndpoint( const uint32_t& aAddress , const uint32_t&  aValue );

      private:

        uint32_t GetEndpoint( const uint32_t& aAddress );

        //! Analyse request and create reply when a Byte-OrderTransaction is observed
        void bot();

        /**
          Analyse request and create reply when a non-incrementing read is observed
          @param aAddress the base address of the read
        */
        void ni_read ( const uint32_t& aAddress );

        /**
          Analyse request and create reply when an incrementing read is observed
          @param aAddress the base address of the read
        */
        void read ( const uint32_t& aAddress );

        /**
          Analyse request and create reply when an incrementing "configuration space" read is observed
          @param aAddress the base address of the read
        */
        void readConfigurationSpace ( const uint32_t& aAddress );

        /**
          Analyse request and create reply when a non-incrementing write is observed
          @param aAddress the base address of the write
          @param aIt iterator to the start of the payload
          @param aEnd iterator to the end of the payload
        */
        void ni_write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );
  
        /**
          Analyse request and create reply when an incrementing write is observed
          @param aAddress the base address of the write
          @param aIt iterator to the start of the payload
          @param aEnd iterator to the end of the payload
        */
        void write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );
  
        /**
          Analyse request and create reply when a read-modify-write sum is observed
          @param aAddress the base address of the write
          @param aAddend the value to be added
        */
        void rmw_sum ( const uint32_t& aAddress , const uint32_t& aAddend );
  
        /**
          Analyse request and create reply when a read-modify-write bits is observed
          @param aAddress the base address of the write
          @param aAndTerm the value to be and'ed
          @param aOrTerm the value to be or'ed
        */
        void rmw_bits ( const uint32_t& aAddress , const uint32_t& aAndTerm , const uint32_t& aOrTerm );
  
        //! Analyse request and create reply when the header is unknown
        void unknown_type();
  
        //! Analyse request and create reply when an IPbus 2.0 control packet header is observed
        bool control_packet_header();
  
        //! Analyse request and create reply when an IPbus 2.0 status packet header is observed
        void status_packet_header();
  
        //! Analyse request and create reply when an IPbus 2.0 resend packet header is observed
        void resend_packet_header();
  
        //! Analyse request and create reply when an unknown IPbus 2.0 packet header is observed
        void unknown_packet_header();
  
  
      private:
        //! The memory space of the virtual hardware
        std::vector< uint32_t > mMemory;

        //! The configuration space within the virtual hardware
        std::vector< uint32_t > mConfigurationSpace;
  
      protected:
        //! The buffer for the incoming IPbus packet
        std::vector< uint32_t > mReceive;
        //! The buffer for the outgoing IPbus packet
        std::vector< uint32_t > mReply;
  
        //! The history of the replies for the retry mechanism (IPbus 2.0 and above only)
        std::deque< std::pair< uint32_t , std::vector< uint32_t > > > mReplyHistory;
        
        //! The last sent packet header for the retry mechanism (IPbus 2.0 and above only)
        uint32_t mLastPacketHeader;
        
        //! History of the IPbus 2.0 packet-types received
        std::deque< uint8_t > mTrafficHistory;
  
        //! History of the received control packet headers
        std::deque< uint32_t > mReceivedControlPacketHeaderHistory;
        //! History of the sent control packet headers
        std::deque< uint32_t > mSentControlPacketHeaderHistory;
  
      private:
        //! Whether we are talking to an IPbus client which includes the big-endian hack
        bool mBigEndianHack;
    };
  }

}

#endif


