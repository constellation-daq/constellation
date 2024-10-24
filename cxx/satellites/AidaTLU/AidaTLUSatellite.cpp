/**
 * @file
 * @brief Implementation of TLU Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "AidaTLUSatellite.hpp"

#include <sstream>

#include <unistd.h>

#include "constellation/core/config/Value.hpp"
#include "constellation/core/log/log.hpp"
#include "constellation/core/protocol/CSCP_definitions.hpp"
#include "constellation/satellite/Satellite.hpp"

using namespace constellation::config;
using namespace constellation::protocol;

AidaTLUSatellite::AidaTLUSatellite(std::string_view type, std::string_view name) : TransmitterSatellite(type, name) {

    register_command("get_tlu_status",
                     "Read current TLU status (trigger ID, number of particles, scalars)",
                     {CSCP::State::ORBIT, CSCP::State::RUN},
                     &AidaTLUSatellite::get_tlu_status,
                     this);

    register_timed_metric(
        "TRIGGER_NUMBER", "", metrics::Type::LAST_VALUE, std::chrono::seconds(1), {CSCP::State::RUN}, [this]() {
            return m_trigger_n.load();
        });

    register_timed_metric(
        "TRIGGER_RATE", "Hz", metrics::Type::LAST_VALUE, std::chrono::seconds(1), {CSCP::State::RUN}, [this]() {
            const auto time_diff = 1e-9 * static_cast<double>(m_lasttime.load() - m_starttime.load());
            if(time_diff == 0.) {
                return 0.;
            }
            return static_cast<double>(m_trigger_n.load()) / time_diff;
        });

    LOG(STATUS) << getCanonicalName() << " created";
}

void AidaTLUSatellite::initializing(constellation::config::Configuration& config) {

    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    // return to clean state before starting to initialize
    m_tlu.reset();

    // start with initialization procedure, as in EUDAQ
    LOG(INFO) << "Reset successful; start initializing " << getCanonicalName();

    LOG(INFO) << "TLU INITIALIZE ID: " + std::to_string(config.get<int>("confid", 0));
    std::string uhal_conn = "file://cxx/satellites/AidaTLU/default_config/aida_tlu_connection.xml";
    std::string uhal_node = "aida_tlu.controlhub";
    uhal_conn = config.get<std::string>("ConnectionFile", uhal_conn);
    uhal_node = config.get<std::string>("DeviceName", uhal_node);
    m_tlu = std::unique_ptr<tlu::AidaTluController>(new tlu::AidaTluController(uhal_conn, uhal_node));

    if(config.get<bool>("skipini", false)) {
        LOG(INFO) << "TLU SKIPPING INITIALIZATION (skipini = 1)";
    } else {
        uint8_t verbose = config.get<uint8_t>("verbose", 0);
        LOG(INFO) << "TLU VERBOSITY SET TO: " + std::to_string(verbose);

        // Define constants
        m_tlu->DefineConst(config.get<int>("nDUTs", 4), config.get<int>("nTrgIn", 6));

        // Import I2C addresses for hardware
        // Populate address list for I2C elements
        m_tlu->SetI2C_core_addr(config.get<uint8_t>("I2C_COREEXP_Addr", 0x21));
        m_tlu->SetI2C_clockChip_addr(config.get<uint8_t>("I2C_CLK_Addr", 0x68));
        m_tlu->SetI2C_DAC1_addr(config.get<uint8_t>("I2C_DAC1_Addr", 0x13));
        m_tlu->SetI2C_DAC2_addr(config.get<uint8_t>("I2C_DAC2_Addr", 0x1f));
        m_tlu->SetI2C_EEPROM_addr(config.get<uint8_t>("I2C_ID_Addr", 0x50));
        m_tlu->SetI2C_expander1_addr(config.get<uint8_t>("I2C_EXP1_Addr", 0x74));
        m_tlu->SetI2C_expander2_addr(config.get<uint8_t>("I2C_EXP2_Addr", 0x75));
        m_tlu->SetI2C_pwrmdl_addr(config.get<uint8_t>("I2C_DACModule_Addr", 0x1C),
                                  config.get<uint8_t>("I2C_EXP1Module_Addr", 0x76),
                                  config.get<uint8_t>("I2C_EXP2Module_Addr", 0x77),
                                  config.get<uint8_t>("I2C_pwrId_Addr", 0x51));
        m_tlu->SetI2C_disp_addr(config.get<uint8_t>("I2C_disp_Addr", 0x3A));

        // Initialize TLU hardware
        m_tlu->InitializeI2C(verbose);
        m_tlu->InitializeIOexp(verbose);
        if(config.get<bool>("intRefOn", false)) {
            m_tlu->InitializeDAC(config.get<bool>("intRefOn", false), config.get<float>("VRefInt", 2.5), verbose);
        } else {
            m_tlu->InitializeDAC(config.get<bool>("intRefOn", false), config.get<float>("VRefExt", 1.3), verbose);
        }

        // Initialize the Si5345 clock chip using pre-generated file
        if(config.get<bool>("CONFCLOCK", true)) {
            std::string clkConfFile;
            std::string defaultCfgFile = "cxx/satellites/AidaTLU/default_config/aida_tlu_clk_config.txt";
            clkConfFile = config.get<std::string>("CLOCK_CFG_FILE", defaultCfgFile);
            if(clkConfFile == defaultCfgFile) {
                LOG(WARNING)
                    << "TLU: Could not find the parameter for clock configuration in the INI file. Using the default.";
            }
            int clkres = m_tlu->InitializeClkChip(clkConfFile, verbose);
            if(clkres == -1) {
                LOG(CRITICAL) << "TLU: clock configuration failed.";
                // ToDo throw something at someone
            }
        }

        // Reset IPBus registers
        m_tlu->ResetSerdes();
        m_tlu->ResetCounters();
        m_tlu->SetTriggerVeto(1);
        m_tlu->ResetFIFO();
        m_tlu->ResetEventsBuffer();

        m_tlu->ResetTimestamp();
    }

    // store configuration parameters needed during launch procedure
    load_launch_config(config);
}

void AidaTLUSatellite::load_launch_config(constellation::config::Configuration& config) {
    m_launch_config.confid = config.get<unsigned int>("confid", 0);
    m_launch_config.verbose = config.get<uint8_t>("verbose", 0);
    m_launch_config.delayStart = config.get<uint32_t>("delayStart", 0);
    m_launch_config.skipconf = config.get<bool>("skipconf", false);
    m_launch_config.HDMI1_set = config.get<unsigned int>("HDMI1_set", 0b0001);
    m_launch_config.HDMI2_set = config.get<unsigned int>("HDMI2_set", 0b0001);
    m_launch_config.HDMI3_set = config.get<unsigned int>("HDMI3_set", 0b0001);
    m_launch_config.HDMI4_set = config.get<unsigned int>("HDMI4_set", 0b0001);
    m_launch_config.HDMI1_clk = config.get<unsigned int>("HDMI1_clk", 1);
    m_launch_config.HDMI2_clk = config.get<unsigned int>("HDMI2_clk", 1);
    m_launch_config.HDMI3_clk = config.get<unsigned int>("HDMI3_clk", 1);
    m_launch_config.HDMI4_clk = config.get<unsigned int>("HDMI4_clk", 1);
    m_launch_config.LEMOclk = config.get<bool>("LEMOclk", true);
    m_launch_config.DACThreshold0 = config.get<float>("DACThreshold0", 1.2);
    m_launch_config.DACThreshold1 = config.get<float>("DACThreshold1", 1.2);
    m_launch_config.DACThreshold2 = config.get<float>("DACThreshold2", 1.2);
    m_launch_config.DACThreshold3 = config.get<float>("DACThreshold3", 1.2);
    m_launch_config.DACThreshold4 = config.get<float>("DACThreshold4", 1.2);
    m_launch_config.DACThreshold5 = config.get<float>("DACThreshold5", 1.2);
    m_launch_config.in0_STR = config.get<unsigned int>("in0_STR", 0);
    m_launch_config.in1_STR = config.get<unsigned int>("in1_STR", 0);
    m_launch_config.in2_STR = config.get<unsigned int>("in2_STR", 0);
    m_launch_config.in3_STR = config.get<unsigned int>("in3_STR", 0);
    m_launch_config.in4_STR = config.get<unsigned int>("in4_STR", 0);
    m_launch_config.in5_STR = config.get<unsigned int>("in5_STR", 0);
    m_launch_config.in0_DEL = config.get<unsigned int>("in0_DEL", 0);
    m_launch_config.in1_DEL = config.get<unsigned int>("in1_DEL", 0);
    m_launch_config.in2_DEL = config.get<unsigned int>("in2_DEL", 0);
    m_launch_config.in3_DEL = config.get<unsigned int>("in3_DEL", 0);
    m_launch_config.in4_DEL = config.get<unsigned int>("in4_DEL", 0);
    m_launch_config.in5_DEL = config.get<unsigned int>("in5_DEL", 0);
    m_launch_config.trigMaskHi = config.get<uint32_t>("trigMaskHi", 0xFFFF);
    m_launch_config.trigMaskLo = config.get<uint32_t>("trigMaskLo", 0xFFFE);
    m_launch_config.trigPol = config.get<uint64_t>("trigPol", 0x003F);
    m_launch_config.PMT1_V = config.get<float>("PMT1_V", 0.0);
    m_launch_config.PMT2_V = config.get<float>("PMT2_V", 0.0);
    m_launch_config.PMT3_V = config.get<float>("PMT3_V", 0.0);
    m_launch_config.PMT4_V = config.get<float>("PMT4_V", 0.0);
    m_launch_config.DUTMask = config.get<int32_t>("DUTMask", 1);
    m_launch_config.DUTMaskMode = config.get<int32_t>("DUTMaskMode", 0xff);
    m_launch_config.DUTMaskModeModifier = config.get<int32_t>("DUTMaskModeModifier", 0xff);
    m_launch_config.DUTIgnoreBusy = config.get<int32_t>("DUTIgnoreBusy", 0xF);
    m_launch_config.DUTIgnoreShutterVeto = config.get<int32_t>("DUTIgnoreShutterVeto", 1);
    m_launch_config.EnableShutterMode = config.get<bool>("EnableShutterMode", 0);
    m_launch_config.ShutterSource = config.get<int8_t>("ShutterSource", 0);
    m_launch_config.ShutterOnTime = config.get<int32_t>("ShutterOnTime", 0);
    m_launch_config.ShutterOffTime = config.get<int32_t>("ShutterOffTime", 0),
    m_launch_config.ShutterVetoOffTime = config.get<int32_t>("ShutterVetoOffTime", 0);
    m_launch_config.InternalShutterInterval = config.get<int32_t>("InternalShutterInterval", 0);
    m_launch_config.InternalTriggerFreq = config.get<uint32_t>("InternalTriggerFreq", 0);
    m_launch_config.EnableRecordData = config.get<uint32_t>("EnableRecordData", 1);
}

void AidaTLUSatellite::launching() {
    LOG(INFO) << "Launching " << getCanonicalName();

    LOG(INFO) << "CONFIG ID: " + std::to_string(m_launch_config.confid);
    LOG(INFO) << "TLU VERBOSITY SET TO: " + std::to_string(m_launch_config.verbose);
    LOG(INFO) << "TLU DELAY START SET TO: " + std::to_string(m_launch_config.delayStart) + " ms";

    std::lock_guard<std::mutex> lock {m_tlu_mutex};
    m_tlu->SetTriggerVeto(1);
    if(m_launch_config.skipconf) {
        LOG(INFO) << "TLU SKIPPING CONFIGURATION (skipconf = 1)";
    } else {
        // Enable HDMI connectors
        LOG(INFO) << " -DUT CONFIGURATION";
        m_tlu->configureHDMI(1, m_launch_config.HDMI1_set, m_launch_config.verbose);
        m_tlu->configureHDMI(2, m_launch_config.HDMI2_set, m_launch_config.verbose);
        m_tlu->configureHDMI(3, m_launch_config.HDMI3_set, m_launch_config.verbose);
        m_tlu->configureHDMI(4, m_launch_config.HDMI4_set, m_launch_config.verbose);

        // Select clock to HDMI
        m_tlu->SetDutClkSrc(1, m_launch_config.HDMI1_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(2, m_launch_config.HDMI2_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(3, m_launch_config.HDMI3_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(4, m_launch_config.HDMI4_clk, m_launch_config.verbose);

        // Set lemo clock
        LOG(INFO) << " -CLOCK OUTPUT CONFIGURATION";
        m_tlu->enableClkLEMO(m_launch_config.LEMOclk, m_launch_config.verbose);

        // Set thresholds
        LOG(INFO) << " -DISCRIMINATOR THRESHOLDS CONFIGURATION";
        m_tlu->SetThresholdValue(0, m_launch_config.DACThreshold0, m_launch_config.verbose);
        m_tlu->SetThresholdValue(1, m_launch_config.DACThreshold1, m_launch_config.verbose);
        m_tlu->SetThresholdValue(2, m_launch_config.DACThreshold2, m_launch_config.verbose);
        m_tlu->SetThresholdValue(3, m_launch_config.DACThreshold3, m_launch_config.verbose);
        m_tlu->SetThresholdValue(4, m_launch_config.DACThreshold4, m_launch_config.verbose);
        m_tlu->SetThresholdValue(5, m_launch_config.DACThreshold5, m_launch_config.verbose);

        // Set trigger stretch and delay
        std::vector<unsigned int> stretcVec = {m_launch_config.in0_STR,
                                               m_launch_config.in1_STR,
                                               m_launch_config.in2_STR,
                                               m_launch_config.in3_STR,
                                               m_launch_config.in4_STR,
                                               m_launch_config.in5_STR};

        std::vector<unsigned int> delayVec = {m_launch_config.in0_DEL,
                                              m_launch_config.in1_DEL,
                                              m_launch_config.in2_DEL,
                                              m_launch_config.in3_DEL,
                                              m_launch_config.in4_DEL,
                                              m_launch_config.in5_DEL};

        LOG(INFO) << " -ADJUST STRETCH AND DELAY";
        m_tlu->SetPulseStretchPack(stretcVec, m_launch_config.verbose);
        m_tlu->SetPulseDelayPack(delayVec, m_launch_config.verbose);

        // Set triggerMask
        // The conf function does not seem happy with a 32-bit default. Need to check.
        LOG(INFO) << " -DEFINE TRIGGER MASK";
        m_tlu->SetTriggerMask(m_launch_config.trigMaskHi, m_launch_config.trigMaskLo);

        // Set triggerPolarity
        LOG(INFO) << " -DEFINE TRIGGER POLARITY";
        m_tlu->SetTriggerPolarity(m_launch_config.trigPol);

        // Set PMT power
        LOG(INFO) << " -PMT OUTPUT VOLTAGES";
        m_tlu->pwrled_setVoltages(m_launch_config.PMT1_V,
                                  m_launch_config.PMT2_V,
                                  m_launch_config.PMT3_V,
                                  m_launch_config.PMT4_V,
                                  m_launch_config.verbose);

        LOG(INFO) << " -DUT OPERATION MODE";
        m_tlu->SetDUTMask(m_launch_config.DUTMask, m_launch_config.verbose);         // Which DUTs are on
        m_tlu->SetDUTMaskMode(m_launch_config.DUTMaskMode, m_launch_config.verbose); // AIDA (x1) or EUDET (x0)
        m_tlu->SetDUTMaskModeModifier(m_launch_config.DUTMaskModeModifier, m_launch_config.verbose); // Only for EUDET
        m_tlu->SetDUTIgnoreBusy(m_launch_config.DUTIgnoreBusy, m_launch_config.verbose); // Ignore busy in AIDA mode
        m_tlu->SetDUTIgnoreShutterVeto(m_launch_config.DUTIgnoreShutterVeto, m_launch_config.verbose); //

        LOG(INFO) << " -SHUTTER OPERATION MODE";
        m_tlu->SetShutterParameters(m_launch_config.EnableShutterMode,
                                    m_launch_config.ShutterSource,
                                    m_launch_config.ShutterOnTime,
                                    m_launch_config.ShutterOffTime,
                                    m_launch_config.ShutterVetoOffTime,
                                    m_launch_config.InternalShutterInterval,
                                    m_launch_config.verbose);
        LOG(INFO) << " -AUTO TRIGGER SETTINGS";
        m_tlu->SetInternalTriggerFrequency(m_launch_config.InternalTriggerFreq, m_launch_config.verbose);

        LOG(INFO) << " -FINALIZING TLU CONFIGURATION";
        m_tlu->SetEnableRecordData(m_launch_config.EnableRecordData);
        m_tlu->GetEventFifoCSR();
        m_tlu->GetEventFifoFillLevel();
    }
}

void AidaTLUSatellite::landing() {
    LOG(INFO) << "Landing TLU";

    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    LOG(INFO) << "Set all PMT_V = 0";
    m_tlu->pwrled_setVoltages(0, 0, 0, 0, m_launch_config.verbose);
}

void AidaTLUSatellite::starting(std::string_view run_identifier) {
    LOG(INFO) << "Starting run " << run_identifier << "...";

    // Set tags for the begin-of-run message:
    setBORTag("FirmwareID", m_tlu->GetFirmwareVersion());
    setBORTag("BoardID", m_tlu->GetBoardID());

    // For EudaqNativeWriter
    setBORTag("eudaq_event", "TluRawDataEvent");
    setBORTag("frames_as_blocks", true);

    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    LOG(DEBUG) << "Resetting TLU counters...";
    m_tlu->ResetCounters();
    m_tlu->ResetEventsBuffer();
    m_tlu->ResetFIFO();
    std::this_thread::sleep_for(std::chrono::milliseconds(m_launch_config.delayStart));

    // Send reset pulse to all DUTs and reset internal counters
    LOG(INFO) << "Starting TLU...";
    m_tlu->SetRunActive(1);
    m_tlu->SetTriggerVeto(0);
}

void AidaTLUSatellite::stopping() {
    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    // Set TLU internal logic to stop
    LOG(INFO) << "Stopping TLU...";
    m_tlu->SetTriggerVeto(1);
    m_tlu->SetRunActive(0);
}

void AidaTLUSatellite::running(const std::stop_token& stop_token) {
    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    m_starttime.store(m_tlu->GetCurrentTimestamp() * 25);

    while(!stop_token.stop_requested()) {
        m_lasttime.store(m_tlu->GetCurrentTimestamp() * 25);

        m_tlu->ReceiveEvents(m_launch_config.verbose);
        while(!m_tlu->IsBufferEmpty()) {
            tlu::fmctludata* data = m_tlu->PopFrontEvent();
            uint32_t trigger_n = data->eventnumber;
            uint64_t ts_raw = data->timestamp;
            uint64_t ts_ns = ts_raw * 25;
            LOG_IF(DEBUG, trigger_n % 1000 == 0) << "Received trigger " << trigger_n << " after " << ts_ns / 1e9 << " s.";

            // Store trigger number
            m_trigger_n.store(trigger_n);

            // Store data:
            auto msg = newDataMessage();

            // Add timestamps in picoseconds and trigger number
            msg.addTag("timestamp_begin", ts_ns * 1000);
            msg.addTag("timestamp_end", (ts_ns + 25) * 1000);
            msg.addTag("trigger", trigger_n);
            msg.addTag("flag_trigger", true);

            // Using "compact" binary data format here:
            std::vector<uint8_t> datablock;
            // 6 (fineTS) + 1 (6x triggers, but that is only a single bit each,so one uint8 should be fine) + 4
            // (32bit eventtype) + 7*4 scalers (dropped for now ???)
            datablock.reserve(7);
            datablock.push_back(static_cast<uint8_t>(data->sc0));
            datablock.push_back(static_cast<uint8_t>(data->sc1));
            datablock.push_back(static_cast<uint8_t>(data->sc2));
            datablock.push_back(static_cast<uint8_t>(data->sc3));
            datablock.push_back(static_cast<uint8_t>(data->sc4));
            datablock.push_back(static_cast<uint8_t>(data->sc5));
            datablock.push_back(((data->input5 & 0x1) << 5) + ((data->input4 & 0x1) << 4) + ((data->input3 & 0x1) << 3) +
                                ((data->input2 & 0x1) << 2) + ((data->input1 & 0x1) << 1) + (data->input0 & 0x1));

            // Add data to message frame
            msg.addFrame(std::move(datablock));

            // Send the data off - or fail:
            sendDataMessage(msg);

            delete data;
        }
    }
}

std::string AidaTLUSatellite::get_tlu_status() {
    uint32_t sl0, sl1, sl2, sl3, sl4, sl5, pret, post;
    pret = m_tlu->GetPreVetoTriggers();
    post = m_tlu->GetPostVetoTriggers();
    m_tlu->GetScaler(sl0, sl1, sl2, sl3, sl4, sl5);
    std::stringstream out {};
    out << "Trigger ID: " << post << ", Particles: " << pret << ", Scalars: " << sl0 << ":" << sl1 << ":" << sl2 << ":"
        << sl3 << ":" << sl4 << ":" << sl5;
    return out.str();
}
