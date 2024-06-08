/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <filesystem>
#include <stdexcept>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/exceptions.hpp"

using namespace Catch::Matchers;
using namespace constellation::log;
using namespace constellation::exec;
using namespace std::literals::string_view_literals;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Load library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader = DSOLoader("Prototype", logger);
    REQUIRE(loader.loadSatelliteGenerator() != nullptr);
    REQUIRE_THAT(loader.getDSOName(), Equals("Prototype"));
}

TEST_CASE("Case-insensitive library loading", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader = DSOLoader("pRoToTyPe", logger);
    REQUIRE(loader.loadSatelliteGenerator() != nullptr);
    REQUIRE_THAT(loader.getDSOName(), Equals("Prototype"));
}

TEST_CASE("Try loading missing library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    REQUIRE_THROWS_MATCHES(DSOLoader("MissingLib", logger),
                           DSOLoadingError,
                           Message("Error while loading shared library \"MissingLib\": Could not find libMissingLib.so"));
}

TEST_CASE("Load wrong library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader =
        DSOLoader("ConstellationCore",
                  logger,
                  std::filesystem::path(CNSTLN_BUILDDIR) / "cxx" / "constellation" / "core" / "libConstellationCore.so");
    REQUIRE_THROWS_AS(loader.loadSatelliteGenerator(), DSOFunctionLoadingError);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
