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

      Marc Magrans de Abril, CERN
      email: marc.magrans.de.abril <AT> cern.ch

      Andrew Rose, Imperial College, London
      email: awr01 <AT> imperial.ac.uk

---------------------------------------------------------------------------
*/

#ifndef _uhal_tests_PerfTester_hpp_
#define _uhal_tests_PerfTester_hpp_

/*!
* \class PerfTester
* \brief Generate custom IPbus/uHAL tests from the command line
*
* \author Robert Frazier
* \date July 2012
*/


// C++ headers
#include <vector>
#include <string>

// Linux C++ headers
#include <sys/time.h>

// uHAL headers
#include "uhal/uhal.hpp"

namespace uhal
{
  namespace tests
  {
    class PerfTester
    {
      public:

        /// Constructor - takes no arguments, does nothing.
        PerfTester();

        /// Destructor
        ~PerfTester() {}

        /// Pass in the two command-line parameter variables, this will define the test that then gets run.
        /*!
         * Returns 0 if a test was run, or non-zero if something was wrong with the command-line arguments, etc.
         */
        int run ( int argc, char* argv[] );


      private:

        // PRIVATE TYPEDEFS

        /// An unsigned 32-bit word.
        typedef uint32_t U32;

        /// A vector of unsigned 32-bit words.
        typedef std::vector< U32 > U32Vec;

        /// A Validated Vector of U32
        typedef uhal::ValVector< U32 > U32ValVec;

        /// A vector of strings
        typedef std::vector< std::string > StringVec;

        /// Function pointer typedef to a test function.
        typedef void ( PerfTester::*PtrToTestFunc ) ();

        /// Typedef for a map that links the name of a test to the function that performs it.
        typedef std::map<std::string, PtrToTestFunc> TestFuncMap;

        /// Typedef for a map that links the name of a test to its description
        typedef std::map<std::string, std::string> TestDescMap;

        /// Typedef for a ClientInterface shared_ptr
        typedef std::shared_ptr<uhal::ClientInterface> ClientPtr;

        /// Typedef for a vector of raw client interfaces
        typedef std::vector< ClientPtr > ClientVec;


        // PRIVATE MEMBER VARIABLES

        TestDescMap m_testDescMap;  ///< Maps test name to test description
        TestFuncMap m_testFuncMap;  ///< Maps test name to test function
        StringVec m_deviceURIs;  ///< Vector of individual connection URI strings.
        ClientVec m_clients;  ///< Vector of low-level uHAL clients.
        std::string m_testName;  ///< Holds the test name
        uint64_t m_iterations;  ///< Number of test iterations
        std::string m_baseAddrStr;  ///< Base addr of reg/ram the test will use. Use a string as workaround for hex input via boost::program_options
        boost::uint32_t m_baseAddr;  ///< The m_baseAddrStr as converted into an actual unsigned value.
        boost::uint32_t m_bandwidthTestDepth;  ///< The depth of read/write used in bandwidth tests
        bool m_verbose;  ///< Verbosity true/false flag.
        bool m_perIterationDispatch; ///< Perform a network dispatch every iteration flag.
        bool m_includeConnect; ///< Include (e.g. TCP) connect time in reported bandwidth/latency


        // PRIVATE MEMBER FUNCTIONS - Test infrastructure

        /// Outputs the standard help text to screen.
        void outputHelpText ( const std::string& argDescriptions ) const;

        /// Outputs the test names and descriptions to screen.
        void outputTestDescriptionsList() const;

        /// Returns true if the user has entered bad command line arguments
        bool badInput() const;

        /// Outputs the user's choices to screen
        void outputUserChoices() const;

        /// Constructs and sets up the appropriate IPbusClient for use in the test
        void buildClients();

        /// Outputs a standard result set to screen - provide it with the number of seconds the test took.
        void outputStandardResults ( double totalSeconds ) const;

        /// Returns a random uint32_t in the range [0,maxSize], with 1/x probability distribution -- so that p(x=0) = p(2<=x<4) = p(2^n <= x < 2^n+1)
        static uint32_t getRandomBlockSize ( const uint32_t maxSize );

        /// Returns a buffer of random numbers
        static U32Vec getRandomBuffer ( unsigned size );

        /// Compares a write buffer with one or more ValVec read responses
        bool buffersEqual ( const U32Vec& writeBuffer, const U32ValVec& readBuffer ) const;

        /// Validation test -- single-register write/read-back
        static bool validation_test_single_write_read ( uhal::ClientInterface& c, const uint32_t addr, const bool perTransactionDispatch, const bool aVerbose );

        /// Validation test -- block write/read-back
        static bool validation_test_block_write_read ( uhal::ClientInterface& c, const uint32_t addr, const uint32_t depth, const bool perTransactionDispatch, const bool aVerbose );

        /// Validation test -- write, RMW bits, read
        static bool validation_test_write_rmwbits_read ( uhal::ClientInterface& c, const uint32_t addr, const bool perTransactionDispatch, const bool aVerbose );

        /// Validation test -- write, RMW sum, read
        static bool validation_test_write_rmwsum_read ( uhal::ClientInterface& c, const uint32_t addr, const bool perTransactionDispatch, const bool aVerbose );

        // PRIVATE MEMBER FUNCTIONS - IPbus test functions that users can run

        void bandwidthRxTest();  ///< Read bandwidth test
        void bandwidthTxTest();  ///< Write bandwidth test
        void validationTest();   ///< Historic basic firmware/software validation test

    public:

        static bool runValidationTest(const std::vector<ClientInterface*>& aClients, const uint32_t aBaseAddr, const uint32_t aDepth, const size_t aNrIterations, const bool aDispatchEachIteration, const bool aVerbose);

    private:
        void sandbox();          ///< An area for a user-definable test

        // PRIVATE CLASSES

        class QueuedTransaction
        {
          public:
            QueuedTransaction() {  }
            virtual ~QueuedTransaction() {  }

            virtual bool check_values() = 0;
        };


        class QueuedBlockRead : public QueuedTransaction
        {
          public:
            QueuedBlockRead ( const uint32_t addr, const ValVector<uint32_t>& valVector, std::vector<uint32_t>::const_iterator expectedValuesIt );
            ~QueuedBlockRead();

            virtual bool check_values();

          private:
            // PRIVATE MEMBER DATA
            uint32_t m_depth, m_addr;
            ValVector<uint32_t> m_valVector;
            std::vector<uint32_t> m_expected;
        };


        class QueuedBlockWrite : public QueuedTransaction
        {
          public:
            QueuedBlockWrite ( const uint32_t addr, const uint32_t depth, const ValHeader& valHeader );
            ~QueuedBlockWrite();

            virtual bool check_values();

          private:
            // PRIVATE MEMBER DATA
            uint32_t m_depth, m_addr;
            ValHeader m_valHeader;
        };


        class QueuedRmwBits : public QueuedTransaction
        {
          public:
            QueuedRmwBits ( const uint32_t addr, const uint32_t a, const uint32_t b, const ValWord<uint32_t>& valWord, const uint32_t expected );
            ~QueuedRmwBits();
            virtual bool check_values();

          private:
            const uint32_t m_addr, m_and, m_or;
            ValWord<uint32_t> m_valWord;
            const uint32_t m_expected;
        };


        class QueuedRmwSum : public QueuedTransaction
        {
          public:
            QueuedRmwSum ( const uint32_t addr, const uint32_t a, const ValWord<uint32_t>& valWord, const uint32_t expected );
            ~QueuedRmwSum();
            virtual bool check_values();

          private:
            const uint32_t m_addr, m_addend;
            ValWord<uint32_t> m_valWord;
            const uint32_t m_expected;
        };

    }; /* End of class PerfTester */

  }  /* End of namespace tests */

} /* End of namespace uhal */

#endif /* _uhal_tests_PerfTester_hpp_ */
