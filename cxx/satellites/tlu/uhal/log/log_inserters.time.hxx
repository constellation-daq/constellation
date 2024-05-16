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


#include <ostream>       // for operator<<, ostream
#include <time.h>        // for tm


// Forward declarations
struct timeval;
struct tm;


namespace uhal
{

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 , char D5 ,
          time_element T6 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> > ( aTime );
  }


  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> > ( aTime );
  }


  template< time_element T0, char D0 ,
  time_element T1 , char D1 ,
  time_element T2 , char D2 ,
  time_element T3 , char D3 ,
  time_element T4 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> > ( aTime );
  }


  template< time_element T0, char D0 ,
  time_element T1 , char D1 ,
  time_element T2 , char D2 ,
  time_element T3 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> > ( aTime );
  }


  template< time_element T0, char D0 ,
  time_element T1 , char D1 ,
  time_element T2 >
  _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> > ( aTime );
  }


  template< time_element T0, char D0 ,
  time_element T1 >
  _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> > ( aTime );
  }


  template< time_element T0 >
  _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> > Time ( const timeval& aTime )
  {
    return _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> > ( aTime );
  }




  template< time_element T0, char D0 ,
  time_element T1 , char D1 ,
  time_element T2 , char D2 ,
  time_element T3 , char D3 ,
  time_element T4 , char D4 ,
  time_element T5 , char D5 ,
  time_element T6 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,D5,T6> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D1;
    print< T2 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D2;
    print< T3 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D3;
    print< T4 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D4;
    print< T5 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D5;
    print< T6 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 , char D4 ,
          time_element T5 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,D4,T5,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D1;
    print< T2 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D2;
    print< T3 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D3;
    print< T4 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D4;
    print< T5 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 , char D3 ,
          time_element T4 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,D3,T4,' ',null,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D1;
    print< T2 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D2;
    print< T3 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D3;
    print< T4 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 , char D2 ,
          time_element T3 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,D2,T3,' ',null,' ',null,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D1;
    print< T2 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D2;
    print< T3 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0, char D0 ,
          time_element T1 , char D1 ,
          time_element T2 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,D1,T2,' ',null,' ',null,' ',null,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D1;
    print< T2 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0, char D0 ,
          time_element T1 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,D0,T1,' ',null,' ',null,' ',null,' ',null,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    aStr << D0;
    print< T1 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }

  template< time_element T0 >
  std::ostream& operator<< ( std::ostream& aStr ,   const _Time< TimeFmt<T0,' ',null,' ',null,' ',null,' ',null,' ',null,' ',null> >& aTime )
  {
    tm* lLocalTime ( localtime ( &aTime.value().tv_sec ) );
    print< T0 > ( aStr , lLocalTime , aTime.value().tv_usec );
    return aStr;
  }




  template< typename FORMAT >
  _Time< FORMAT > Time ( const timeval& aTime , const FORMAT& aFmt )
  {
    return _Time< FORMAT > ( aTime );
  }

}

