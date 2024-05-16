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


#ifndef _uhal_log_inserters_location_hpp_
#define _uhal_log_inserters_location_hpp_


#define ThisLocation() uhal::Location( __PRETTY_FUNCTION__ , __FILE__ , __LINE__ )


#include <iostream>
#include <stdint.h>


namespace uhal
{
  //! A class to wrap the function name, filename and line-number location of its construction for the purpose of debugging and tracking unwinding exceptions
  class Location
  {

    public:
      /**
      	Constructor
      	@param aFunction the name of the current function as returned by the __PRETTY_FUNCTION__ macro
      	@param aFile the name of the current file as returned by the __FILE__ macro
      	@param aLine the number of the current line as returned by the __LINE__ macro
      */
      Location ( const char* aFunction , const char* aFile , const uint32_t& aLine );

      //! the name of the current function as returned by the __PRETTY_FUNCTION__ macro
      const char* mFunction;
      //! the name of the current file as returned by the __FILE__ macro
      const char* mFile;
      //! the number of the current line as returned by the __LINE__ macro
      const uint32_t mLine;
  };

  std::ostream& operator<< ( std::ostream& aStr , const Location& aLocation );

}

#endif
