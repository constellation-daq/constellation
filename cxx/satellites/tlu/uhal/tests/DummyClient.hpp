#ifndef _uhal_tests_DummyClient_hpp_
#define _uhal_tests_DummyClient_hpp_

#include "uhal/ClientInterface.hpp"

#include <iostream>


namespace uhal {
  namespace tests {

    //! Empty client class, written to test registration
    class DummyClient : public ClientInterface {
    public:
      DummyClient ( const std::string& aId, const URI& aUri );
      ~DummyClient();

    private:

      void implementDispatch ( std::shared_ptr< Buffers > aBuffers );

      ValHeader implementBOT( );

      ValHeader implementWrite ( const uint32_t& aAddr, const uint32_t& aValue );

      ValHeader implementWriteBlock ( const uint32_t& aAddr, const std::vector< uint32_t >& aValues, const defs::BlockReadWriteMode& aMode );

      ValWord< uint32_t > implementRead ( const uint32_t& aAddr, const uint32_t& aMask );

      ValVector< uint32_t > implementReadBlock ( const uint32_t& aAddr, const uint32_t& aSize, const defs::BlockReadWriteMode& aMode );

      ValWord< uint32_t > implementRMWbits ( const uint32_t& aAddr , const uint32_t& aANDterm , const uint32_t& aORterm );

      ValWord< uint32_t > implementRMWsum ( const uint32_t& aAddr , const int32_t& aAddend );

      exception::exception* validate ( uint8_t* aSendBufferStart ,
          uint8_t* aSendBufferEnd ,
          std::deque< std::pair< uint8_t* , uint32_t > >::iterator aReplyStartIt ,
          std::deque< std::pair< uint8_t* , uint32_t > >::iterator aReplyEndIt );

      uint32_t getMaxNumberOfBuffers();

      uint32_t getMaxSendSize();

      uint32_t getMaxReplySize();
    };

  }
}
#endif
