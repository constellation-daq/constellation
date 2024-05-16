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

      Tom Williams, Rutherford Appleton Laboratory, Oxfordshire
      email: tom.williams <AT> cern.ch

---------------------------------------------------------------------------
*/


#ifndef _uhal_detail_utilities_hpp_
#define _uhal_detail_utilities_hpp_


#include <stdint.h>
#include <string>
#include <vector>


namespace uhal
{
  class ClientInterface;
  class Node;

  namespace detail
  {
    //! Generates a short string summarising which nodes match the specified address
    std::string getAddressDescription(const Node& aNode, const uint32_t aAddress, const size_t& aMaxListSize);

    //! Generates a short string summarising which nodes match the specified address
    std::string getAddressDescription(const ClientInterface& aClient, const uint32_t aAddress, const size_t& aMaxListSize);

    bool compareNodeAddr ( const Node* aNodeL, const Node* aNodeR );

    std::vector<std::pair<const Node*, const Node*> > getAddressOverlaps(const Node& aNode);

    void printNodeOverlapDescription(std::ostream& aStream, const Node& aNode1, const Node& aNode2);

    bool writeNodeOverlapReport(const std::string& aFilePath, const std::vector<std::pair<const Node*, const Node*> >& aNodes, const std::string& aHeader);

  }
}


#endif
