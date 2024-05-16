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

#ifndef _uhal_IPbusInspector_hpp_
#define _uhal_IPbusInspector_hpp_


#include <stdint.h>
#include <vector>

#include "uhal/ProtocolIPbusCore.hpp"


namespace uhal
{

  //! Helper class to decode IPbus packets as passed from the Client to the Target
  template< uint8_t IPbus_major , uint8_t IPbus_minor >
  class HostToTargetInspector
  {
    public:
      //! Default constructor
      HostToTargetInspector( );

      //! Destructor
      virtual ~HostToTargetInspector( );

    protected:
      //! The current raw transaction header
      uint32_t mHeader;
      //! The current transaction type
      IPbusTransactionType mType;
      //! The current word count
      uint32_t mWordCounter;
      //! The current transaction id
      uint32_t mTransactionId;
      //! The current error code/flag
      uint8_t mResponseGood;

      //! The current raw IPbus2 packet header
      uint32_t mPacketHeader;
      //! The current IPbus2 packet counter
      uint32_t mPacketCounter;
      //! The current IPbus2 packet type
      uint32_t mPacketType;

    public:
      /**
        Analyse an IPbus packet held as a vector of uint32_t's
        @param aIt Starting iterator of the IPbus packet
        @param aEnd End point of the IPbus packet
        @param aContinueOnError set the behaviour after errors
        @return success or failure
      */
      bool analyze ( std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd , const bool& aContinueOnError = true );

    protected:
      //! Virtual callback function called when a Byte-OrderTransaction is observed
      virtual void bot();

      /**
        Virtual callback function called when a non-incrementing read is observed
        @param aAddress the base address of the read
      */
      virtual void ni_read ( const uint32_t& aAddress );

      /**
        Virtual callback function called when an incrementing read is observed
        @param aAddress the base address of the read
      */
      virtual void read ( const uint32_t& aAddress );

      /**
        Virtual callback function called when an incrementing "configuration space" read is observed
        @param aAddress the base address of the read
      */
      virtual void readConfigurationSpace ( const uint32_t& aAddress );

      /**
        Virtual callback function called when a non-incrementing write is observed
        @param aAddress the base address of the write
        @param aIt iterator to the start of the payload
        @param aEnd iterator to the end of the payload
      */
      virtual void ni_write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );

      /**
        Virtual callback function called when an incrementing write is observed
        @param aAddress the base address of the write
        @param aIt iterator to the start of the payload
        @param aEnd iterator to the end of the payload
      */
      virtual void write ( const uint32_t& aAddress , std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );

      /**
        Virtual callback function called when a read-modify-write sum is observed
        @param aAddress the base address of the write
        @param aAddend the value to be added
      */
      virtual void rmw_sum ( const uint32_t& aAddress , const uint32_t& aAddend );

      /**
        Virtual callback function called when a read-modify-write bits is observed
        @param aAddress the base address of the write
        @param aAndTerm the value to be and'ed
        @param aOrTerm the value to be or'ed
      */
      virtual void rmw_bits ( const uint32_t& aAddress , const uint32_t& aAndTerm , const uint32_t& aOrTerm );

      //! Virtual callback function for the case where the header is unknown
      virtual void unknown_type();

      //! Virtual callback function called when an IPbus 2.0 control packet header is observed
      virtual bool control_packet_header ();

      //! Virtual callback function called when an IPbus 2.0 status packet header is observed
      virtual void status_packet_header();

      //! Virtual callback function called when an IPbus 2.0 resend packet header is observed
      virtual void resend_packet_header();

      //! Virtual callback function called when an unknown IPbus 2.0 packet header is observed
      virtual void unknown_packet_header();
  };



  //! Helper class to decode IPbus packets as passed from the Target to the Client
  template< uint8_t IPbus_major , uint8_t IPbus_minor >
  class TargetToHostInspector
  {
    public:
      //! Default constructor
      TargetToHostInspector( );
      //! Destructor
      virtual ~TargetToHostInspector( );

    protected:
      //! The current raw transaction header
      uint32_t mHeader;
      //! The current transaction type
      IPbusTransactionType mType;
      //! The current word count
      uint32_t mWordCounter;
      //! The current transaction id
      uint32_t mTransactionId;
      //! The current error code/flag
      uint8_t mResponseGood;

      //! The current raw IPbus2 packet header
      uint32_t mPacketHeader;
      //! The current IPbus2 packet counter
      uint32_t mPacketCounter;
      //! The current IPbus2 packet type
      uint32_t mPacketType;

    public:

      /**
        Analyse an IPbus packet held as a vector of uint32_t's
        @param aIt Starting iterator of the IPbus packet
        @param aEnd End point of the IPbus packet
        @param aContinueOnError set the behaviour after errors
        @return success or failure
      */
      bool analyze ( std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd , const bool& aContinueOnError = true );

    protected:
      //! Virtual callback function called when a Byte-OrderTransaction is observed
      virtual void bot();

      /**
        Virtual callback function called when a non-incrementing read is observed
        @param aIt iterator to the start of the payload
        @param aEnd iterator to the end of the payload        
      */
      virtual void ni_read ( std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );

      /**
        Virtual callback function called when an incrementing read is observed
        @param aIt iterator to the start of the payload
        @param aEnd iterator to the end of the payload        
      */
      virtual void read ( std::vector<uint32_t>::const_iterator& aIt , const std::vector<uint32_t>::const_iterator& aEnd );

      //! Virtual callback function called when a non-incrementing write is observed
      virtual void ni_write ( );

      //! Virtual callback function called when an incrementing write is observed
      virtual void write ( );

      /**
        Virtual callback function called when a read-modify-write sum is observed
        @param aNewValue the value before/after the change (depending on IPbus version)
      */
      virtual void rmw_sum ( const uint32_t& aNewValue );
 
      /**
        Virtual callback function called when a read-modify-write bits is observed
        @param aNewValue the value before/after the change (depending on IPbus version)
      */
      virtual void rmw_bits ( const uint32_t& aNewValue );

      //! Virtual callback function for the case where the header is unknown
      virtual void unknown_type();

      //! Virtual callback function called when an IPbus 2.0 control packet header is observed
      virtual bool control_packet_header ();

      //! Virtual callback function called when an IPbus 2.0 status packet header is observed
      virtual void status_packet_header();

      //! Virtual callback function called when an unknown IPbus 2.0 packet header is observed
      virtual void unknown_packet_header();
  };

}


#endif
