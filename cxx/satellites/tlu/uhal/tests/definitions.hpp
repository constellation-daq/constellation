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

#ifndef _uhal_tests_definitions_hpp_
#define _uhal_tests_definitions_hpp_


namespace uhal {
namespace tests {

enum DeviceType {
  IPBUS_1_3_UDP,
  IPBUS_1_3_TCP,
  IPBUS_1_3_CONTROLHUB,
  IPBUS_2_0_UDP, 
  IPBUS_2_0_TCP,
  IPBUS_2_0_CONTROLHUB,
  IPBUS_2_0_PCIE
};

} // end ns tests
} // end ns uhal


#define UHAL_TESTS_DEFINE_CLIENT_TEST_CASES( test_suite_name , test_case_name , test_fixture, test_case_contents ) \
  \
  BOOST_AUTO_TEST_SUITE( ipbusudp_1_3 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_1_3_UDP> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( ipbustcp_1_3 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_1_3_TCP> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( chtcp_1_3 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_1_3_CONTROLHUB> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( ipbusudp_2_0 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_2_0_UDP> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( ipbustcp_2_0 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_2_0_TCP> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( chtcp_2_0 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_2_0_CONTROLHUB> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  \
  BOOST_AUTO_TEST_SUITE( ipbuspcie_2_0 ) \
  \
  BOOST_AUTO_TEST_SUITE( test_suite_name ) \
  \
  BOOST_FIXTURE_TEST_CASE( test_case_name , test_fixture<IPBUS_2_0_PCIE> ) \
  {\
    test_case_contents \
  }\
  \
  BOOST_AUTO_TEST_SUITE_END() \
  \
  BOOST_AUTO_TEST_SUITE_END()


#endif
