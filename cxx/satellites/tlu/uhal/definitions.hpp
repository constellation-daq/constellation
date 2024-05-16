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
	@author Marc Magrans De Abril
	@date 2012
*/

#ifndef _uhal_definitions_hpp_
#define _uhal_definitions_hpp_

#include <stdint.h>

namespace uhal
{
  namespace defs
  {
    //! define Read and Write permissions of a uhal Node
    enum NodePermission {READ=0x1,WRITE=0x2,READWRITE=0x3};

    //! define whether transactions target a single register, a block of registers, a block-read/write port or whether node is purely hierarchical
    enum BlockReadWriteMode {SINGLE,INCREMENTAL,NON_INCREMENTAL,HIERARCHICAL};

    //! define what it means to have no mask
    const uint32_t NOMASK = 0xFFFFFFFF;
  }
}

#endif

