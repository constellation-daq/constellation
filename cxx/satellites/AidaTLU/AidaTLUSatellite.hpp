/**
 * @file
 * @brief Caribou Satellite definition
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <atomic>

#include "constellation/satellite/TransmitterSatellite.hpp"

#include "AidaTLU/aida_include/AidaTluController.hh"

using namespace constellation::config;
using namespace constellation::satellite;

class AidaTLUSatellite : public TransmitterSatellite {
public:
    AidaTLUSatellite(std::string_view type, std::string_view name);

public:
    void initializing(Configuration& config) override;
    void launching() override;
    void landing() override;
    void starting(std::string_view run_identifier) override;
    void stopping() override;
    void running(const std::stop_token& stop_token) override;

private:
    std::string get_tlu_status();

private:
    struct launch_configuration {
        unsigned int confid;
        uint8_t verbose;
        uint32_t delayStart;
        bool skipconf;
        unsigned int HDMI1_set;
        unsigned int HDMI2_set;
        unsigned int HDMI3_set;
        unsigned int HDMI4_set;
        unsigned int HDMI1_clk;
        unsigned int HDMI2_clk;
        unsigned int HDMI3_clk;
        unsigned int HDMI4_clk;
        bool LEMOclk;
        float DACThreshold0;
        float DACThreshold1;
        float DACThreshold2;
        float DACThreshold3;
        float DACThreshold4;
        float DACThreshold5;
        unsigned int in0_STR;
        unsigned int in1_STR;
        unsigned int in2_STR;
        unsigned int in3_STR;
        unsigned int in4_STR;
        unsigned int in5_STR;
        unsigned int in0_DEL;
        unsigned int in1_DEL;
        unsigned int in2_DEL;
        unsigned int in3_DEL;
        unsigned int in4_DEL;
        unsigned int in5_DEL;
        uint32_t trigMaskHi;
        uint32_t trigMaskLo;
        uint64_t trigPol;
        float PMT1_V;
        float PMT2_V;
        float PMT3_V;
        float PMT4_V;
        int32_t DUTMask;
        int32_t DUTMaskMode;
        int32_t DUTMaskModeModifier;
        int32_t DUTIgnoreBusy;
        int32_t DUTIgnoreShutterVeto;
        bool EnableShutterMode;
        int8_t ShutterSource;
        int32_t ShutterOnTime;
        int32_t ShutterOffTime;
        int32_t ShutterVetoOffTime;
        int32_t InternalShutterInterval;
        uint32_t InternalTriggerFreq;
        uint32_t EnableRecordData;
    } m_launch_config;
    void load_launch_config(Configuration& config);

    std::unique_ptr<tlu::AidaTluController> m_tlu;
    std::mutex m_tlu_mutex;

    std::atomic_uint32_t m_trigger_n;
    std::atomic_uint64_t m_starttime;
    std::atomic_uint64_t m_lasttime;
};
