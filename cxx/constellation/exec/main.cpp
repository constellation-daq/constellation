/**
 * @file
 * @brief satellite executable
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "constellation/exec/satellite.hpp"

using namespace constellation::exec;

int main(int argc, char* argv[]) {
    return satellite_main(argc, argv, "satellite");
}
