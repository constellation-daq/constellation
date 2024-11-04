/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>

#include "constellation/controller/ControllerConfiguration.hpp"
#include "constellation/controller/exceptions.hpp"

using namespace Catch::Matchers;
using namespace constellation::controller;

namespace {
    std::filesystem::path test_files_dir() {
        const auto* cxx_tests_dir = std::getenv("CXX_TESTS_DIR"); // NOLINT(concurrency-mt-unsafe)
        return std::filesystem::path(cxx_tests_dir) / "test_files";
    }
} // namespace

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Non-existing TOML file", "[controller]") {
    const auto test_file = std::filesystem::path("non-existing.toml");
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(test_file),
                           ConfigFileNotFoundError,
                           Message("Could not read configuration file non-existing.toml"));
}

TEST_CASE("Invalid TOML file", "[controller]") {
    const auto test_file = test_files_dir() / "invalid_toml.txt";
    REQUIRE_THROWS_AS(ControllerConfiguration(test_file), ConfigFileParseError);
}

TEST_CASE("Inhomogeneous array", "[controller]") {
    const auto test_file = test_files_dir() / "inhomogeneous_array.toml";
    REQUIRE_THROWS_MATCHES(ControllerConfiguration(test_file),
                           ConfigFileTypeError,
                           Message("Invalid value type for key array: Array is not homogeneous"));
}

TEST_CASE("Valid TOML file", "[controller]") {
    const auto test_file = test_files_dir() / "good_config.toml";
    const ControllerConfiguration config {test_file};

    // Global only
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.at("bool").get<bool>() == true);
    REQUIRE(global_config.at("string").get<std::string>() == "global");
    REQUIRE(global_config.at("int").get<int>() == -42);
    REQUIRE(global_config.at("float").get<double>() == 3.14);
    REQUIRE(global_config.at("array_bool").get<std::vector<bool>>() == std::vector<bool>({true, false, false, true}));
    REQUIRE(global_config.at("array_string").get<std::vector<std::string>>() ==
            std::vector<std::string>({"global1", "global2"}));
    REQUIRE(global_config.at("array_int").get<std::vector<int>>() == std::vector<int>({1, 2, 3}));
    REQUIRE(global_config.at("array_float").get<std::vector<double>>() == std::vector<double>({0.1}));
    REQUIRE(std::holds_alternative<std::monostate>(global_config.at("empty_array")));

    // Global + Type
    const auto section_config = config.getSatelliteConfiguration("Dummy.NotASatellite");
    REQUIRE(section_config.at("bool").get<bool>() == true);
    REQUIRE(section_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(section_config.at("string").get<std::string>() == "type");

    // Global + Type + Satellite
    const auto d1_config = config.getSatelliteConfiguration("Dummy.D1");
    REQUIRE(d1_config.at("bool").get<bool>() == true);
    REQUIRE(d1_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d1_config.at("string").get<std::string>() == "D1");
    REQUIRE(d1_config.at("satellite").get<bool>() == true);

    // Case-insensitivity check
    const auto d2_config = config.getSatelliteConfiguration("Dummy.D2");
    REQUIRE(d2_config.at("bool").get<bool>() == true);
    REQUIRE(d2_config.at("type").get<std::string>() == "Dummy");
    REQUIRE(d2_config.at("string").get<std::string>() == "D2");
    REQUIRE(d2_config.at("satellite").get<bool>() == true);
}

TEST_CASE("No global section", "[controller]") {
    const std::string_view toml_string = "# Config without global section";
    const ControllerConfiguration config {toml_string};
    const auto global_config = config.getSatelliteConfiguration("NotA.Satellite");
    REQUIRE(global_config.empty());
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
