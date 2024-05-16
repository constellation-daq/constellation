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

      Tom Williams, Rutherford Appleton Laboratory, Oxfordshire
      email: tom.williams <AT> cern.ch

---------------------------------------------------------------------------
*/

/**
	@file
	@author Tom Williams
	@date September 2017
*/

#ifndef _uhal_ProtocolPCIe_hpp_
#define _uhal_ProtocolPCIe_hpp_


#include <chrono>
#include <deque>                           // for deque
#include <memory>
#include <mutex>
#include <stddef.h>                        // for size_t
#include <stdint.h>                        // for uint32_t, uint8_t
#include <string>                          // for string
#include <utility>                         // for pair
#include <vector>                          // for vector

#include <boost/interprocess/managed_shared_memory.hpp>

#include "uhal/ClientInterface.hpp"
#include "uhal/log/exception.hpp"
#include "uhal/ProtocolIPbus.hpp"


namespace uhal
{
  class Buffers;
  struct URI;

  namespace exception
  {
    //! Exception class to handle the case in which the PCIe connection timed out.
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( PCIeTimeout , ClientTimeout , "Exception class to handle the case in which the PCIe connection timed out." )
    //! Exception class to handle a failure to read from the specified device files during initialisation
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( PCIeInitialisationError , TransportLayerError , "Exception class to handle a failure to read from the specified device files during initialisation." )
    //! Exception class to handle a low-level seek/read/write error after initialisation
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( PCIeCommunicationError , TransportLayerError , "Exception class to handle a low-level seek/read/write error after initialisation." )
    //! Exception class to handle errors from pthread mutex-related functions
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( MutexError , TransportLayerError , "Exception class to handle errors from pthread mutex-related functions." )
  }

  //! Transport protocol to transfer an IPbus buffer via PCIe
  class PCIe : public IPbus< 2 , 0 >
  {
    public:
      class PacketFmt {
      public:
        PacketFmt(const uint8_t* const, const size_t);
        PacketFmt(const std::vector< std::pair<const uint8_t*, size_t> >& aData);
        ~PacketFmt();

        const std::vector< std::pair<const uint8_t*, size_t> > mData;
      };

      class File {
      public:
        File(const std::string& aPath, int aFlags);
        ~File();

        const std::string& getPath() const;
        void setPath(const std::string& aPath);

        void open();
        void close();

        void createBuffer(const size_t aNrBytes);

        void read(const uint32_t aAddr, const uint32_t aNrWords, std::vector<uint32_t>& aValues);

        void write(const uint32_t aAddr, const std::vector<uint32_t>& aValues);

        void write(const uint32_t aAddr, const uint8_t* const aPtr, const size_t aNrBytes);

        void write(const uint32_t aAddr, const std::vector<std::pair<const uint8_t*, size_t> >& aData);

        bool haveLock() const;

        void lock();

        void unlock();

      private:
        std::string mPath;
        int mFd;
        int mFlags;
        bool mLocked;
        size_t mBufferSize;
        char* mBuffer;
      };

    private:

      class RobustMutex {
      public:
        RobustMutex();
        ~RobustMutex();

        void lock();

        void unlock();

        uint64_t getCounter() const;

        bool isActive() const;

        void startSession();

        void endSession();

      private:
        RobustMutex(const RobustMutex&);

        pthread_mutex_t mMutex;
        uint64_t mCount;
        bool mSessionActive;
      };

      template <class T>
      class SharedObject {
      public:

        SharedObject(const SharedObject<T>&) = delete;
        SharedObject<T>& operator=(const SharedObject<T>&) = delete;

        SharedObject(const std::string& aName);
        ~SharedObject();

        T* operator->();

        T& operator*();

      private:
        std::string mName;
        boost::interprocess::managed_shared_memory mSharedMem;
        T* mObj;
      };

      static std::string getSharedMemName(const std::string& );

      typedef RobustMutex IPCMutex_t;
      typedef std::unique_lock<IPCMutex_t> IPCScopedLock_t;

    public:
      /**
        Constructor
        @param aId the uinique identifier that the client will be given.
        @param aUri a struct containing the full URI of the target.
      */
      PCIe ( const std::string& aId, const URI& aUri );

      //!	Destructor
      virtual ~PCIe();

    private:

      PCIe ( const PCIe& aPCIe );

      PCIe& operator= ( const PCIe& aPCIe );

      /**
        Send the IPbus buffer to the target, read back the response and call the packing-protocol's validate function
        @param aBuffers the buffer object wrapping the send and recieve buffers that are to be transported
        If multithreaded, adds buffer to the dispatch queue and returns. If single-threaded, calls the dispatch-worker dispatch function directly and blocks until the response is validated.
      */
      void implementDispatch ( std::shared_ptr< Buffers > aBuffers );

      //! Concrete implementation of the synchronization function to block until all buffers have been sent, all replies received and all data validated
      virtual void Flush( );

      //! Function which tidies up this protocol layer in the event of an exception
      virtual void dispatchExceptionHandler();

      typedef IPbus< 2 , 0 > InnerProtocol;

      typedef std::chrono::steady_clock SteadyClock_t;

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

      //! Set up the connection to the device
      void connect();

      //! Set up the connection to the device
      void connect(IPCScopedLock_t& );

      //! Close the connection to the device
      void disconnect();

      //! Write request packet to next page in host-to-FPGA device file 
      void write(const std::shared_ptr<Buffers>& aBuffers);

      //! Read next pending reply packet from appropriate page of FPGA-to-host device file, and validate contents
      void read();

      bool mConnected;

      //! Host-to-FPGA device file
      File mDeviceFileHostToFPGA;
      //! FPGA-to-host device file
      File mDeviceFileFPGAToHost;
      //! FPGA-to-host interrupt (event) file
      File mDeviceFileFPGAEvent;

      SharedObject<IPCMutex_t> mIPCMutex;
      bool mIPCExternalSessionActive;
      uint64_t mIPCSessionCount;

      bool mXdma7seriesWorkaround;

      bool mUseInterrupt;

      std::chrono::microseconds mSleepDuration;

      uint32_t mNumberOfPages, mMaxInFlight, mPageSize, mMaxPacketSize, mIndexNextPage, mPublishedReplyPageCount, mReadReplyPageCount;

      //! The list of buffers still awaiting a reply
      std::deque < std::shared_ptr< Buffers > > mReplyQueue;
  };

  std::ostream& operator<<(std::ostream& aStream, const PCIe::PacketFmt& aPacket);

}


#endif
