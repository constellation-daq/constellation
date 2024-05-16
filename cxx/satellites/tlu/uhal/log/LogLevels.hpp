
#ifndef _uhal_log_LogLevels_hpp_
#define _uhal_log_LogLevels_hpp_


#include <iostream>
#include <stdint.h>


// NOTE: the UHAL_LOG_INSERT_WARNING is defined to bridge between the compilers used on Linux and OSX.
// While on linux the 'warning' attribute is defined, it is not in OSX 10.9 clang.
// On the other hand clang's 'deprecated' is messageless in g++, while it supports messages in clang.
// To make the story short, UHAL_LOG_INSERT_WARNING uses the most appropriated attribute for the system in use.
#ifndef __clang__
#define UHAL_LOG_INSERT_WARNING warning
#else
#define UHAL_LOG_INSERT_WARNING deprecated
#endif


namespace uhal
{

  template< typename U>
  void insert ( std::ostream& aStr , const U& aU )
  {
    aStr << aU;
  }

  __attribute__ ( ( UHAL_LOG_INSERT_WARNING ( "Insertion of integer types can result in implicit casts. Consider using the Integer() formatter instead" ) ) )
  void insert ( std::ostream& aStr , const uint32_t& aUint ) ;

  __attribute__ ( ( UHAL_LOG_INSERT_WARNING ( "Insertion of integer types can result in implicit casts. Consider using the Integer() formatter instead" ) ) )
  void insert ( std::ostream& aStr , const int32_t& aInt );

  __attribute__ ( ( UHAL_LOG_INSERT_WARNING ( "Insertion of boolean types can result in implicit casts. Consider using the Boolean() formatter instead" ) ) )
  void insert ( std::ostream& aStr , const bool& aBool );


  template< typename T >
  class BaseLogLevel
  {
    protected:
      typedef void ( *fPtr ) ( std::ostream& aStr );

      BaseLogLevel ( std::ostream& aStr , fPtr aHeadFunction , fPtr aTailFunction ) :
        mStr ( aStr ),
        mHeadFunction ( aHeadFunction ),
        mTailFunction ( aTailFunction )
      {}

    public:
      T& operator() ()
      {
        return static_cast<T&> ( *this );
      }

      T& head()
      {
        mHeadFunction ( mStr );
        return static_cast<T&> ( *this );
      }

      T& tail()
      {
        mTailFunction ( mStr );
        return static_cast<T&> ( *this );
      }

      std::ostream& stream()
      {
        return mStr;
      }

    private:
      std::ostream& mStr;
      fPtr mHeadFunction;
      fPtr mTailFunction;

  };


  //! Helper struct representing the Fatal log level to allow us to specialize functions according to their log level
  class FatalLevel : public BaseLogLevel< FatalLevel >
  {
      typedef BaseLogLevel< FatalLevel > Base;

    public:
      FatalLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = FatalLevel::colour_head, Base::fPtr aTailFunction = FatalLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern FatalLevel Fatal;


  //! Helper struct representing the Error log level to allow us to specialize functions according to their log level
  class ErrorLevel : public BaseLogLevel< ErrorLevel >
  {
      typedef BaseLogLevel< ErrorLevel > Base;

    public:
      ErrorLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = ErrorLevel::colour_head, Base::fPtr aTailFunction = ErrorLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern ErrorLevel Error;



  //! Helper struct representing the Warning log level to allow us to specialize functions according to their log level
  class WarningLevel : public BaseLogLevel< WarningLevel >
  {
      typedef BaseLogLevel< WarningLevel > Base;

    public:
      WarningLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = WarningLevel::colour_head, Base::fPtr aTailFunction = WarningLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern WarningLevel Warning;

  //! Helper struct representing the Notice log level to allow us to specialize functions according to their log level
  class NoticeLevel : public BaseLogLevel< NoticeLevel >
  {

      typedef BaseLogLevel< NoticeLevel > Base;

    public:
      NoticeLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = NoticeLevel::colour_head, Base::fPtr aTailFunction = NoticeLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern NoticeLevel Notice;

  //! Helper struct representing the Info log level to allow us to specialize functions according to their log level
  class InfoLevel : public BaseLogLevel< InfoLevel >
  {
      typedef BaseLogLevel< InfoLevel > Base;

    public:
      InfoLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = InfoLevel::colour_head, Base::fPtr aTailFunction = InfoLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern InfoLevel Info;

  //! Helper struct representing the Debug log level to allow us to specialize functions according to their log level
  class DebugLevel : public BaseLogLevel< DebugLevel >
  {
      typedef BaseLogLevel< DebugLevel > Base;

    public:
      DebugLevel ( std::ostream& aStr = std::cout , Base::fPtr aHeadFunction = DebugLevel::colour_head, Base::fPtr aTailFunction = DebugLevel::colour_tail );

      static void colour_head ( std::ostream& aStr );
      static void colour_tail ( std::ostream& aStr );
  };

  extern DebugLevel Debug;
}


#endif
