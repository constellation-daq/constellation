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

#ifndef _uhal_ProtocolIPbusCore_hpp_
#define _uhal_ProtocolIPbusCore_hpp_


#include <deque>
#include <functional>
#include <iosfwd>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "uhal/ClientInterface.hpp"
#include "uhal/definitions.hpp"
#include "uhal/log/exception.hpp"
#include "uhal/ValMem.hpp"


namespace boost {
  namespace posix_time {
    class time_duration;
  }
}

namespace uhal
{
  // Forward declaration
  struct URI;

  /**
  	Enumerated type to define the IPbus transaction type.
  	Note that they are stored here as (raw_type << 3) so that the LSL operation does not need to be performed every time a new transaction is created
  */
  enum IPbusTransactionType
  {
    B_O_T,
    READ,
    WRITE,
    RMW_BITS,
    RMW_SUM,
    R_A_I,
    NI_READ,
    NI_WRITE,
    CONFIG_SPACE_READ
  };

  /**
    Streaming operator for formatting objects of the uhal::IPbusTransactionType
    @param aStr a stream to which to append the formatted data
    @param aIPbusTransactionType an enum object to be formatted
    @return a reference to the stream for chaining stram calls
  */
  std::ostream& operator<< ( std::ostream& aStr , const uhal::IPbusTransactionType& aIPbusTransactionType );
}


namespace uhal
{
  namespace exception
  {
    //! Exception class to handle the case where the IPbus header could not be parsed.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( IPbusCoreUnparsableTransactionHeader , TransactionLevelError, "Exception class to handle the case where the IPbus transaction header could not be parsed." )
    //! Exception class to handle the case where an incorrect value for the IPbus transaction type and/or ID was returned.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( IPbusTransactionFieldsIncorrect , TransactionLevelError, "Exception class to handle the case where an incorrect value for the IPbus transaction type and/or ID was returned." )

    class IPbusCoreResponseCodeSet : public TransactionLevelError {
    public:
      IPbusCoreResponseCodeSet(const ClientInterface& aClient, uint32_t aId, IPbusTransactionType aType, uint32_t aWordCount, uint8_t aResponseCode, const std::string& aResponseMsg, uint32_t aBaseAddress, const std::pair<uint32_t, uint32_t>& aHeaders, const std::pair<uint32_t, uint32_t>& aPacketOffsets);

      std::string description() const throw();
    };

  }


  //! A class providing the core IPbus packing functionality
  class IPbusCore : public ClientInterface
  {

    public:
      /**
      	Default constructor
      	@param aId the uinique identifier that the client will be given.
      	@param aUri a struct containing the full URI of the target.
      	@param aTimeoutPeriod the default timeout period (can be changed later)
      */
      IPbusCore ( const std::string& aId, const URI& aUri , const boost::posix_time::time_duration& aTimeoutPeriod );

      //! Destructor
      virtual ~IPbusCore();

      /**
        Read a single, unmasked, unsigned word from the configuration address space
        @param aAddr the address of the register to read
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > readConfigurationSpace ( const uint32_t& aAddr );

      /**
        Read a single, masked, unsigned word from the configuration address space
        @param aAddr the address of the register to read
        @param aMask the mask to apply to the value after reading
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      ValWord< uint32_t > readConfigurationSpace ( const uint32_t& aAddr, const uint32_t& aMask );

    protected:

      //! Send a byte order transaction
      virtual ValHeader implementBOT( );

      /**
        Write a single, unmasked word to a register
        @param aAddr the address of the register to write
        @param aValue the value to write to the register
      */
      virtual ValHeader implementWrite ( const uint32_t& aAddr, const uint32_t& aValue );

      /**
        Write a block of data to a block of registers or a block-write port
        @param aAddr the address of the register to write
        @param aValues the values to write to the registers or a block-write port
        @param aMode whether we are writing to a block of registers (INCREMENTAL) or a block-write port (NON_INCREMENTAL)
      */
      virtual ValHeader implementWriteBlock ( const uint32_t& aAddr, const std::vector< uint32_t >& aValues, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL );

      /**
        Read a single, masked, unsigned word
        @param aAddr the address of the register to read
        @param aMask the mask to apply to the value after reading
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRead ( const uint32_t& aAddr, const uint32_t& aMask = defs::NOMASK );

      /**
        Read a block of unsigned data from a block of registers or a block-read port
        @param aAddr the lowest address in the block of registers or the address of the block-read port
        @param aSize the number of words to read
        @param aMode whether we are reading from a block of registers (INCREMENTAL) or a block-read port (NON_INCREMENTAL)
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValVector< uint32_t > implementReadBlock ( const uint32_t& aAddr, const uint32_t& aSize, const defs::BlockReadWriteMode& aMode=defs::INCREMENTAL );

      /**
        Read a single, masked, unsigned word from the configuration address space
        @param aAddr the address of the register to read
        @param aMask the mask to apply to the value after reading
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementReadConfigurationSpace ( const uint32_t& aAddr, const uint32_t& aMask = defs::NOMASK );

      /**
        Read the value of a register, apply the AND-term, apply the OR-term, set the register to this new value and return a copy of the new value to the user
        @param aAddr the address of the register to read, modify, write
        @param aANDterm the AND-term to apply to existing value in the target register
        @param aORterm the OR-term to apply to existing value in the target register
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRMWbits ( const uint32_t& aAddr , const uint32_t& aANDterm , const uint32_t& aORterm );

      /**
        Read the value of a register, add the addend, set the register to this new value and return a copy of the new value to the user
        @param aAddr the address of the register to read, modify, write
        @param aAddend the addend to add to the existing value in the target register
        @return a Validated Memory which wraps the location to which the reply data is to be written
      */
      virtual ValWord< uint32_t > implementRMWsum ( const uint32_t& aAddr , const int32_t& aAddend );


    protected:
      /**
      	Function which the transport protocol calls when the IPbus reply is received to check that the headers are as expected
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
        Abstract interface of function to calculate the IPbus header for a particular protocol version
        @param aType the type of the IPbus transaction
        @param aWordCount the word count field of the IPbus header
        @param aTransactionId the TransactionId of the IPbus header
        @param aInfoCode the response status of the transaction
        @return an IPbus header
      */
      virtual uint32_t implementCalculateHeader ( const IPbusTransactionType& aType , const uint32_t& aWordCount , const uint32_t& aTransactionId , const uint8_t& aInfoCode ) = 0;

      /**
        Abstract interface of function to parse an IPbus header for a particular protocol version
        @param aHeader an IPbus header to be parsed
        @param aType return the type of the IPbus transaction
        @param aWordCount return the word count field of the IPbus header
        @param aTransactionId return the TransactionId of the IPbus header
        @param aInfoCode return the response status of the IPbus header
        @return whether extraction succeeded
      */
      virtual bool implementExtractHeader ( const uint32_t& aHeader , IPbusTransactionType& aType , uint32_t& aWordCount , uint32_t& aTransactionId , uint8_t& aInfoCode ) = 0;

      //! Returns the InfoCode for request transactions in this IPbus version.
      virtual uint8_t requestTransactionInfoCode() const = 0;

      //! Returns the maximum value of the word count in the transaction header, for each IPbus version
      virtual uint32_t getMaxTransactionWordCount() const = 0;

      //! Function which is called when an exception is thrown
      virtual void dispatchExceptionHandler();

    private:

      virtual std::function<void (std::ostream&, const uint8_t&)> getInfoCodeTranslator() = 0;

      //! The transaction counter which will be incremented in the sent IPbus headers
      uint32_t mTransactionCounter;
  };


  template<typename T>
  struct TranslatedFmt {
  public:
    TranslatedFmt(const T& aData, const std::function<void (std::ostream&, const T&)>& aFunction);
    ~TranslatedFmt();

    const T& mData;
    const std::function<void (std::ostream&, const T&)>& mFunc;
  };

  template <typename T>
  TranslatedFmt<T>::TranslatedFmt(const T& aData, const std::function<void (std::ostream&, const T&)>& aFunction) :
    mData(aData),
    mFunc(aFunction)
  {
  }

  template <typename T>
  TranslatedFmt<T>::~TranslatedFmt()
  {
  }

  template <typename T>
  std::ostream& operator<<(std::ostream& aStream, const TranslatedFmt<T>& aFmt)
  {
    aFmt.mFunc(aStream, aFmt.mData);
    return aStream;
  }
}


#endif
