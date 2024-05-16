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

#ifndef _uhal_tests_PCIeDummyHardware_hpp_
#define _uhal_tests_PCIeDummyHardware_hpp_


#include "uhal/tests/DummyHardware.hpp"


namespace uhal {
namespace tests {

class PCIeDummyHardware : public DummyHardware<2, 0>
{
public:
  typedef DummyHardware<2, 0> base_type;

  PCIeDummyHardware(const std::string& aDevicePathHostToFPGA, const std::string& aDevicePathFPGAToHost, const uint32_t& aReplyDelay, const bool& aBigEndianHack);

  ~PCIeDummyHardware();

  void run();

  void stop();

private:
  static void fifoRead(int aFileDescriptor, const uint32_t aAddr, const uint32_t aNrWords, std::vector<uint32_t>& aValues);

  static bool fileWrite(int aFileDescriptor, const uint32_t aAddr, const std::vector<uint32_t>& aValues);

  static bool fileWrite(int aFileDescriptor, const uint32_t aAddr, const uint8_t* const aPtr, const size_t aNrBytes);


  std::string mDevicePathHostToFPGA, mDevicePathFPGAToHost;

  const uint32_t mNumberOfPages;
  const uint32_t mWordsPerPage;
  uint32_t mNextPageIndex;
  uint32_t mPublishedPageCount;

  bool mStop;

  int mDeviceFileHostToFPGA, mDeviceFileFPGAToHost;
};

} // end ns tests
} // end ns uhal

#endif
