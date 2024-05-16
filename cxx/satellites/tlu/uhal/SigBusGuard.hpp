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


      Tom Williams, Rutherford Appleton Laboratory
      email: tom.williams <AT> cern.ch

      Dan Gastler, Boston University
      email: dgastler <AT> bu.edu

---------------------------------------------------------------------------
*/


#ifndef _uhal_SigBusGuard_hpp_
#define _uhal_SigBusGuard_hpp_


#include <functional>
#include <mutex>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <string>

#include "uhal/log/exception.hpp"
#include "uhal/ClientInterface.hpp"


namespace uhal {

  namespace exception {
    UHAL_DEFINE_DERIVED_EXCEPTION_CLASS ( SigBusError , TransactionLevelError, "Exception caused by a SIGBUS signal." )
    UHAL_DEFINE_EXCEPTION_CLASS ( SignalHandlerNotRegistered , "Exception associated with signal handler registration errors." )
    UHAL_DEFINE_EXCEPTION_CLASS ( SignalMaskingFailure , "Exception associated with signal masking errors." )
    UHAL_DEFINE_EXCEPTION_CLASS ( SignalNotBlocked , "Exception associated with SIGBUS not being masked when using uHAL." )
  }

  class SigBusGuard {
  public:
    SigBusGuard();

    ~SigBusGuard();

    void protect(const std::function<void()>&, const std::string&);

    static void blockSIGBUS();

  private:
    static void handle(int);
    std::lock_guard<std::mutex> mLockGuard;
    struct sigaction mAction;
    struct sigaction mOriginalAction;
    sigset_t mOriginalMask;

    static std::mutex sMutex;
    static sigjmp_buf sEnv;
    volatile static sig_atomic_t sProtected;
  };

}

#endif
