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

/**
	@file
	@author Andrew W. Rose
	@date 2012
*/

#ifndef _uhal_log_exception_hpp_
#define _uhal_log_exception_hpp_


#include <exception>
#include <string>
#include <sys/time.h>


//!	Macro for simplifying the declaration and definition of derived exception types
#define UHAL_DEFINE_DERIVED_EXCEPTION_CLASS( ClassName , BaseClassName, ClassDescription )\
  class ClassName : public BaseClassName {\
  public:\
    ClassName() : BaseClassName() {}\
    ClassName(const std::string& aMessage) : BaseClassName() {append(aMessage.c_str());}\
    void throwAsDerivedType(){ throw ClassName(*this); } \
  protected:\
    std::string description() const throw() { return std::string( ClassDescription ); } \
};

#define UHAL_DEFINE_EXCEPTION_CLASS( ClassName , ClassDescription ) UHAL_DEFINE_DERIVED_EXCEPTION_CLASS(ClassName, uhal::exception::exception, ClassDescription)


namespace uhal
{

  //! A namespace for all exceptions to live in - this will hopefully make documentation a bit clearer
  namespace exception
  {

    //! An abstract base exception class, including an interface to throw as the derived type (for passing exceptions between threads)
    class exception : public std::exception
    {
      public:

        exception ();

        exception ( const exception& aExc );

        /**
          Assignment operator
        		  @param aExc an exception to copy
        		  @return a reference to this object
        */
        exception& operator= ( const exception& aExc );

        virtual ~exception() throw();

        /**
        	Function which returns the error message associated with an exception
        	If no error message has previously been defined, then it makes the typename readable (where appropriate) and returns this instead.
        	@return the error message associated with an exception
        */
        virtual const char* what() const throw();

        //!	Function which casts a pointer from the base type of this object to a derived type of this object and rethrows as the derived type
        virtual void throwAsDerivedType() = 0;

        /**
        	Add additional information to the exception message
        	@param aCStr additional information to be added to the exception message
        */
        void append ( const char* aCStr ) throw();


      protected:
        /**
        	Return the description associated with this type of exception
        	@return the description associated with this type of exception
        */
        virtual std::string description() const throw() = 0;

      private:
        /// The time at which the exception was thrown
        timeval mTime;

        /// Memory into which message is added by calls to append
        char* mString;
    };

  }
}


#endif
