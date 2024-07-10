/**
 * @file
 * @brief Caribou Satellite definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include "constellation/satellite/Satellite.hpp"

#include "aida_include/AidaTluController.hh"

using namespace constellation::config;
using namespace constellation::satellite;

class TluSatellite : public Satellite {
public:
    TluSatellite(std::string_view type, std::string_view name);

public:
    void initializing(Configuration& config) override;
    void launching() override;
    void landing() override;
    void reconfiguring(const Configuration& partial_config) override;
    void starting(std::string_view run_identifier) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    bool m_exit_of_run;
    std::mutex m_mtx_tlu;

    std::unique_ptr<tlu::AidaTluController> m_tlu;
    uint64_t m_starttime = 0;
    uint64_t m_lasttime = 0;
    double m_duration = 0;

    uint8_t m_verbose;
    uint32_t m_delayStart;
};
