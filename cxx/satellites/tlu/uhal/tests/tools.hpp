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

      Marc Magrans de Abril, CERN
      email: marc.magrans.de.abril <AT> cern.ch

      Andrew Rose, Imperial College, London
      email: awr01 <AT> imperial.ac.uk

      Tom Williams, Rutherford Appleton Laboratory, Oxfordshire
      email: tom.williams <AT> cern.ch

---------------------------------------------------------------------------
*/

#ifndef _uhal_tests_tools_hpp_
#define _uhal_tests_tools_hpp_


#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <sys/time.h>
#include <thread>

#include "uhal/ConnectionManager.hpp"
#include "uhal/HwInterface.hpp"
#include "uhal/tests/definitions.hpp"



namespace uhal {
namespace tests {

class DummyHardwareInterface;

class DummyHardwareRunner {
public:
  DummyHardwareRunner(DummyHardwareInterface* aHw);
  ~DummyHardwareRunner();

  void setReplyDelay (const std::chrono::microseconds& aDelay);

private:
  std::unique_ptr<DummyHardwareInterface> mHw;
  std::thread mHwThread;
};


/// A very simple timer
class Timer
{
public:

  Timer() :m_start()
  {
    gettimeofday ( &m_start, NULL );
  }

  /// Returns number of elapsed seconds since the timer was instantiated.
  double elapsedSeconds()
  {
    timeval now;
    gettimeofday ( &now, NULL );
    time_t sec = now.tv_sec - m_start.tv_sec;
    suseconds_t usec = now.tv_usec - m_start.tv_usec;
    return static_cast<double> ( sec + usec/1000000. );
  }

private:
  timeval m_start;

}; /* End of class Timer */


double measureReadLatency(ClientInterface& aClient, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aDispatchEachIteration, bool aVerbose);

double measureReadLatency(const std::vector<ClientInterface*>& aClients, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aDispatchEachIteration, bool aVerbose);

double measureWriteLatency(ClientInterface& aClient, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aDispatchEachIteration, bool aVerbose);

double measureWriteLatency(const std::vector<ClientInterface*>& aClients, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aDispatchEachIteration, bool aVerbose);

double measureFileReadLatency(const std::string& aFilePath, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aVerbose);

double measureFileWriteLatency(const std::string& aFilePath, uint32_t aBaseAddr, uint32_t aDepth, size_t aNrIterations, bool aVerbose);

} // end ns tests
} // end ns uhal

#endif
