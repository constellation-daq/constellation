/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/core/utils/enum.hpp"
#include "constellation/core/utils/timers.hpp"
#include "constellation/core/utils/type.hpp"

using namespace Catch::Matchers;
using namespace constellation::utils;

namespace test {
    class TestClass {};
}; // namespace test

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Stopwatch Timer", "[core]") {
    using namespace std::chrono_literals;
    auto timer = StopwatchTimer();
    timer.start();
    std::this_thread::sleep_for(50ms);
    timer.stop();
    REQUIRE(timer.duration() >= 50ms);
}

TEST_CASE("Timeout Timer", "[core]") {
    using namespace std::chrono_literals;
    auto timer = TimeoutTimer(200ms);
    timer.reset();
    REQUIRE_FALSE(timer.timeoutReached());
    std::this_thread::sleep_for(200ms);
    REQUIRE(timer.timeoutReached());
    REQUIRE(timer.startTime() < std::chrono::steady_clock::now());
}

TEST_CASE("Test demangle", "[core]") {
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

TEST_CASE("Enum names", "[core]") {
    // Scoped enum
    enum class Color : std::uint8_t { RED = 0x1, BLUE = 0x2, GREEN = 0x4 };
    REQUIRE_THAT(enum_name(Color::RED), Equals("RED"));
    REQUIRE_THAT(enum_names<Color>(), RangeEquals(std::array<std::string_view, 3>({"RED", "BLUE", "GREEN"})));
    // Unscoped enum (works also as flag)
    enum ColorMix : std::uint8_t { WHITE = 0x0, RED = 0x1, BLUE = 0x2, GREEN = 0x4 };
    REQUIRE_THAT(enum_name(ColorMix::WHITE), Equals("WHITE"));
    REQUIRE_THAT(enum_name(ColorMix::RED | ColorMix::BLUE), Equals("RED|BLUE"));
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
