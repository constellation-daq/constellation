/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <array>
#include <chrono>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/utils/type.hpp"

using namespace Catch::Matchers;
using namespace constellation::utils;

namespace test {
    class TestClass {};
}; // namespace test

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Test demangle", "[satellite]") {
    // std::vector
    using Vector = std::vector<int>;
    REQUIRE_THAT(demangle<Vector>(), Equals("std::vector<int>"));
    // std::array
    using Array = std::array<double, 1>;
    REQUIRE_THAT(demangle<Array>(), Equals("std::array<double, 1>"));
    // std::map
    using Map = std::map<char, char>;
    REQUIRE_THAT(demangle<Map>(), Equals("std::map<char, char>"));
    // std::string and std::string_view
    REQUIRE_THAT(demangle<std::string>(), Equals("std::string"));
    REQUIRE_THAT(demangle<std::string_view>(), Equals("std::string_view"));
    // std::chrono::system_clock::time_point
    REQUIRE_THAT(demangle<std::chrono::system_clock::time_point>(), Equals("std::chrono::system_clock::time_point"));
    // std::monostate
    REQUIRE_THAT(demangle<std::monostate>(), Equals("std::monostate"));
    // Custom class
    REQUIRE_THAT(demangle<test::TestClass>(), Equals("test::TestClass"));
    // Nesting
    using Nested = std::vector<std::map<std::string, std::array<std::chrono::system_clock::time_point, 123>>>;
    REQUIRE_THAT(demangle<Nested>(),
                 Equals("std::vector<std::map<std::string, std::array<std::chrono::system_clock::time_point, 123>>>"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
