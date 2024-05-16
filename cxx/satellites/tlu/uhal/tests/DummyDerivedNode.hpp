#ifndef _uhal_tests_DummyDerivedNode_hpp_
#define _uhal_tests_DummyDerivedNode_hpp_

#include "uhal/DerivedNode.hpp"

#include "uhal/log/log.hpp"
#include <iostream>

namespace uhal {
  namespace tests {

    class DummyParentNode : public uhal::Node {
      UHAL_DERIVEDNODE(DummyParentNode)
      public:

      ///
      DummyParentNode(const Node& aNode);

      ///
      virtual ~DummyParentNode();

      /// 
      void printParameters() const; 
    };

    //! Class further derived from Level1
    class DummyChildNode : public DummyParentNode {
      UHAL_DERIVEDNODE(DummyChildNode)
      public:
        //
        DummyChildNode(const Node& aNode);
        ///
        virtual ~DummyChildNode();
    };

  }
}
#endif
