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

      Tom Williams, Rutherford Appleton Laboratory, Oxfordshire
      email: tom.williams <AT> cern.ch

---------------------------------------------------------------------------
*/

#ifndef _uhal_tests_fixtures_hpp_
#define _uhal_tests_fixtures_hpp_


#include <stdint.h>
#include <string>

#include "uhal/ConnectionManager.hpp"
#include "uhal/HwInterface.hpp"
#include "uhal/tests/definitions.hpp"
#include "uhal/tests/tools.hpp"


namespace uhal {
namespace tests {

struct AbstractFixture {
protected:
  AbstractFixture() {}
  virtual ~AbstractFixture() {}

  static std::string getAddressFileURI();

public:
  static std::string connectionFileURI;
  // HW client timeout in milliseconds
  static size_t timeout;
  static bool quickTest;
};



template <DeviceType type> struct MinimalFixture;


template <DeviceType type>
struct MinimalFixture : public AbstractFixture {
  MinimalFixture();
  virtual ~MinimalFixture();

  uhal::HwInterface getHwInterface() const;

  static const DeviceType deviceType;

protected:
  uint16_t devicePort;
  std::string deviceId;
};


template <>
struct MinimalFixture<IPBUS_2_0_PCIE> : public AbstractFixture {
  MinimalFixture(); 
  ~MinimalFixture();

  uhal::HwInterface getHwInterface() const;

  static const DeviceType deviceType;
  std::string hardwareToClientFile;
  std::string clientToHardwareFile;
  std::string deviceId;
};

template <DeviceType type>
const DeviceType MinimalFixture<type>::deviceType = type;

template <>
MinimalFixture<IPBUS_1_3_UDP>::MinimalFixture();

template <>
MinimalFixture<IPBUS_1_3_TCP>::MinimalFixture();

template <>
MinimalFixture<IPBUS_1_3_CONTROLHUB>::MinimalFixture();

template <>
MinimalFixture<IPBUS_2_0_UDP>::MinimalFixture();

template <>
MinimalFixture<IPBUS_2_0_TCP>::MinimalFixture();

template <>
MinimalFixture<IPBUS_2_0_CONTROLHUB>::MinimalFixture();


template <DeviceType type>
MinimalFixture<type>::~MinimalFixture()
{
}

template <DeviceType type>
HwInterface MinimalFixture<type>::getHwInterface() const
{
  ConnectionManager manager(connectionFileURI);
  HwInterface hw(manager.getDevice(deviceId));
  hw.setTimeoutPeriod(timeout);
  return hw;
}




template <DeviceType type>
struct DummyHardwareFixture : public MinimalFixture<type> {
  DummyHardwareFixture();
  ~DummyHardwareFixture() {}

  DummyHardwareRunner hwRunner;
};


template <>
DummyHardwareFixture<IPBUS_1_3_UDP>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_1_3_TCP>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_1_3_CONTROLHUB>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_2_0_UDP>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_2_0_TCP>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_2_0_CONTROLHUB>::DummyHardwareFixture();

template <>
DummyHardwareFixture<IPBUS_2_0_PCIE>::DummyHardwareFixture();


} // end ns tests
} // end ns uhal


#endif
