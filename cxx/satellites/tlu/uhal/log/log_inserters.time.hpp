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


#ifndef _uhal_log_inserters_time_hpp_
#define _uhal_log_inserters_time_hpp_


#include <iostream>
#include <stdint.h>
#include <sys/time.h>

#include "uhal/log/log_inserter_helper.hpp"


// Forward declarations
struct timeval;
struct tm;


namespace uhal
{

  //! Enumerated type defining the different elements which can be used for formatting a time
  enum time_element
  {
    null,	/**< an unused entry */
    year,	/**< year formatted as four digits e.g. 2012 */
    yr,		/**< year formatted as two digits e.g. 12 */
    strmth,	/**< month formatted as three character string e.g. Jun */
    mth,	/**< month formatted as two digits e.g. 06 */
    day,	/**< day of the month formatted as two digits e.g. 27 */
    hr,		/**< hour of the day formatted as two digits, 24-hour clock e.g. 14 */
    min,	/**< minutes past the hour formatted as two digits e.g. 06 */
    sec,	/**< seconds past the minute formatted as two digits e.g. 09 */
    usec	/**< microseconds past the second formatted as exactly six digits e.g. 012345 */
  };

  //! A struct whose template parameters represent a time format
  template< time_element T0 , char D0 = ' ' ,
  time_element T1 = null, char D1 = ' ' ,
  time_element T2 = null, char D2 = ' ' ,
  time_element T3 = null, char D3 = ' ' ,
  time_element T4 = null, char D4 = ' ' ,
  time_element T5 = null, char D5 = ' ' ,
  time_element T6 = null > struct TimeFmt {};

  //! Typedef the most commonly used time format (day/month/year hour:minut:second) for convenience
  typedef TimeFmt<day,'/',mth,'/',year,' ',hr,':',min,':',sec> DefaultTimeFmt;


  //! Forward declaration
  template< typename FORMAT > class _Time;

  /**
    Helper function which wrap the template uglyness in a pretty package
    @param aTime a time object to be formatted and logged
    @return a time wrapper object whose template parameters fully encapsulate the default formatting
  */
  _Time< DefaultTimeFmt > Time ( const timeval& aTime );


  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 , char D5 ,
          time_element T6 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> > Time ( const timeval& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> > Time ( const timeval& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> > Time ( const timeval& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

  template< time_element T0, char D0 ,
          time_element T1 >
  _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

  template< time_element T0 >
  _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );



  /**
    Helper function which wrap the template uglyness in a pretty package
    @param aTime a time object to be formatted and logged
    @param aFmt a time format object whose template parameters fully encapsulate the desired formatting
    @return a time wrapper object whose template parameters fully encapsulate the desired formatting
  */
  template< typename FORMAT > _Time< FORMAT > Time ( const timeval& aTime , const FORMAT& aFmt );



  /**
  	A private class whose template parameters fully encapsulate the formatting and which is used to wrap a reference to an underlying type. A log_inserter function must be available to handle this type.
  	The first parameter (the type) was templated so that we could overload for multiple time types but, so far, only timeval has been implemented
  */
  template< typename FORMAT >
  class _Time : public RefWrapper< timeval >
  {
      friend _Time< DefaultTimeFmt > Time ( const timeval& aTime );
      friend _Time< FORMAT > Time<> ( const timeval& aTime , const FORMAT& aFmt );

      template< time_element T0, char D0 ,
              time_element T1 , char D1 ,
              time_element T2 , char D2 ,
              time_element T3 , char D3 ,
              time_element T4 , char D4 ,
              time_element T5 , char D5 ,
              time_element T6 >
      friend _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> > Time ( const timeval& aTime );

      template< time_element T0, char D0 ,
              time_element T1 , char D1 ,
              time_element T2 , char D2 ,
              time_element T3 , char D3 ,
              time_element T4 , char D4 ,
              time_element T5 >
      friend _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> > Time ( const timeval& aTime );

      template< time_element T0, char D0 ,
              time_element T1 , char D1 ,
              time_element T2 , char D2 ,
              time_element T3 , char D3 ,
              time_element T4 >
      friend _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> > Time ( const timeval& aTime );

      template< time_element T0, char D0 ,
              time_element T1 , char D1 ,
              time_element T2 , char D2 ,
              time_element T3 >
      friend _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

      template< time_element T0, char D0 ,
              time_element T1 , char D1 ,
              time_element T2 >
      friend _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

      template< time_element T0, char D0 ,
              time_element T1 >
      friend _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );

      template< time_element T0 >
      friend _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime );


      /**
      	Constructor
      	@param aT an object for which we will are wrapping a reference
      */
      _Time ( const timeval& aTime ) : RefWrapper< timeval > ( aTime ) {}
  };



  /**
    Format a time element for for sending to the log
    @param aTm a tm struct containing the time to be formatted (down to the second)
    @param aUsec the number of microseconds past the second
  */
  template< time_element T >
  void print ( std::ostream& aStr , const tm* aTm , const uint32_t& aUsec );


  /**
  	A helper function to return the current time
  	@return the current time
  */
  timeval Now();


  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 , char D5 ,
          time_element T6 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> >& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> >& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> >& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> >& aTime );

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> >& aTime );

  template< time_element T0, char D0 ,
          time_element T1 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> >& aTime );

  template< time_element T0 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> >& aTime );

}


#include "uhal/log/log_inserters.time.hxx"

#endif
