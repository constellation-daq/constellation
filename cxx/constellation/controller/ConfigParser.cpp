/**
 * @file
 * @brief Configuration file parser implementation
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "ConfigParser.hpp"
#include "exceptions.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "constellation/core/config/exceptions.hpp"

using namespace constellation::controller;
using namespace constellation::config;

ConfigParser::ConfigParser(std::filesystem::path file) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(file.string());
    } catch(const toml::parse_error& err) {
        throw ConfigParseError(err.source(), err.description());
    }
}
