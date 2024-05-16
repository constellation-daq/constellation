
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


#ifndef _uhal_tests_DummyHardwareOptions_hpp_
#define _uhal_tests_DummyHardwareOptions_hpp_


#include <boost/program_options.hpp>

#include "uhal/log/LogLevels.hpp"
#include "uhal/log/log.hpp"


namespace uhal {
  namespace tests {
  
    //! Struct to store the dummy hardware command line options
    struct DummyHardwareOptions
    {
      //! The delay in seconds between the request and response of the first IPbus transaction
      uint32_t delay;
      //! The port used by the dummy hardware
      uint16_t port;
      //! Whether we use the big-endian hack
      bool bigendian;
      //! IPbus version number - 1 or 2
      uint32_t version;

      /**
        Static function to parse the command line arguments into a struct containing the information
        @param argc the number of command line arguments
        @param argv array of c-strings containing the command line arguments
      */
      static DummyHardwareOptions parseFromCommandLine( int argc,char* argv[] );
    };
  
  }
}
 
//   /**
//      Function to parse the command line arguments into a struct containing the information
//      @param argc the number of command line arguments
//      @param argv array of c-strings containing the command line arguments
//   */
    uhal::tests::DummyHardwareOptions uhal::tests::DummyHardwareOptions::parseFromCommandLine ( int argc,char* argv[] )
    {
      // Declare the supported options.
      boost::program_options::options_description desc ( "Allowed options" );
      desc.add_options()
      ( "help,h", "Produce this help message" )
      ( "delay,d", boost::program_options::value<uint32_t>()->default_value ( 0 ) , "Reply delay for first packet (in seconds) - optional" )
      ( "port,p", boost::program_options::value<uint16_t>() , "Port number to listen on - required" )
      ( "big-endian,b", "Include the big-endian hack (version 2 only)" )
      ( "version,v", boost::program_options::value<uint32_t>() , "IPbus Major version (1 or 2) - required" )
      ( "verbose,V", "Produce verbose output" )
      ;
      boost::program_options::variables_map vm;
  
      try
      {
        boost::program_options::store ( boost::program_options::parse_command_line ( argc, argv, desc ), vm );
        boost::program_options::notify ( vm );
  
        if ( vm.count ( "help" ) )
        {
          std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
          std::cout << desc << std::endl;
          exit ( 0 );
        }
  
        uhal::tests::DummyHardwareOptions lResult;
        lResult.delay = vm["delay"].as<uint32_t>();
        lResult.port = vm["port"].as<uint16_t>();
        lResult.version = vm["version"].as<uint32_t>();
        lResult.bigendian = bool ( vm.count ( "big-endian" ) );
  
        if ( ( lResult.version == 1 ) && ( lResult.bigendian ) )
        {
          log ( Error , "-big-endian flag does nothing with version set to 1" );
        }
  
        if ( vm.count ( "verbose" ) )
        {
          setLogLevelTo ( Debug() );
        }
        else
        {
          setLogLevelTo ( Notice() );
        }
  
        return lResult;
      }
      catch ( std::exception& e )
      {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cout << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
        std::cout << desc << std::endl;
        exit ( 1 );
      }
    }


#endif

