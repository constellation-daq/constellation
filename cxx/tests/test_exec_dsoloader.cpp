/**
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "constellation/build.hpp"
#include "constellation/core/log/Logger.hpp"
#include "constellation/exec/DSOLoader.hpp"
#include "constellation/exec/exceptions.hpp"

using namespace Catch::Matchers;
using namespace constellation::log;
using namespace constellation::exec;

// NOLINTBEGIN(cert-err58-cpp,misc-use-anonymous-namespace)

TEST_CASE("Load library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader = DSOLoader("Sputnik", logger);
    REQUIRE(loader.loadSatelliteGenerator() != nullptr);
    REQUIRE_THAT(loader.getDSOName(), Equals("Sputnik"));
}

TEST_CASE("Case-insensitive library loading", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader = DSOLoader("sPuTnIk", logger);
    REQUIRE(loader.loadSatelliteGenerator() != nullptr);
    REQUIRE_THAT(loader.getDSOName(), Equals("Sputnik"));
}

TEST_CASE("Try loading missing library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    REQUIRE_THROWS_MATCHES(DSOLoader("MissingLib", logger),
                           DSOLoadingError,
                           Message("Error while loading shared library \"MissingLib\": Could not find " +
                                   DSOLoader::to_dso_file_name("MissingLib")));
}

TEST_CASE("Load wrong library", "[exec][exec::dsoloader]") {

    auto logger = Logger("DSOLoader");
    auto loader = DSOLoader("ConstellationCore",
                            logger,
                            std::filesystem::path(CNSTLN_BUILDDIR) / "cxx" / "constellation" / "core" /
                                DSOLoader::to_dso_file_name("ConstellationCore"));
    REQUIRE_THROWS_AS(loader.loadSatelliteGenerator(), DSOFunctionLoadingError);
}

// NOLINTEND(cert-err58-cpp,misc-use-anonymous-namespace)
