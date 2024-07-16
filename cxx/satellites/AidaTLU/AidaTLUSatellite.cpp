/**
 * @file
 * @brief Implementation of TLU Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "AidaTLUSatellite.hpp"

#include <unistd.h>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

AidaTLUSatellite::AidaTLUSatellite(std::string_view type, std::string_view name)
    : Satellite(type, name), m_starttime(0), m_lasttime(0), m_duration(0) {
    LOG(logger_, STATUS) << "TluSatellite " << getCanonicalName() << " created";

    // ToDo: implement
    // This satellite supports reconfiguration:
    //support_reconfigure();
}

void AidaTLUSatellite::initializing(constellation::config::Configuration& config) {

    // return to clean state before starting to initialize
    m_tlu.reset();

    // start with initialization procedure, as in EUDAQ
    LOG(logger_, INFO) << "Reset successful; start initializing " << getCanonicalName();

    LOG(logger_, INFO) << "TLU INITIALIZE ID: " + std::to_string(config.get<int>("initid", 0));
    std::string uhal_conn =
        "file:///home/feindtf/programs/constellation/cxx/satellites/AidaTLU/default_config/aida_tlu_connection.xml";
    std::string uhal_node = "aida_tlu.controlhub";
    uhal_conn = config.get<std::string>("ConnectionFile", uhal_conn);
    uhal_node = config.get<std::string>("DeviceName", uhal_node);
    m_tlu = std::unique_ptr<tlu::AidaTluController>(new tlu::AidaTluController(uhal_conn, uhal_node));

    if(config.get<bool>("skipini", false)) {
        LOG(logger_, INFO) << "TLU SKIPPING INITIALIZATION (skipini = 1)";
    } else {
        uint8_t verbose = config.get<uint8_t>("verbose", 0);
        LOG(logger_, INFO) << "TLU VERBOSITY SET TO: " + std::to_string(verbose);

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
            std::string defaultCfgFile =
                "/home/feindtf/programs/constellation/cxx/satellites/AidaTLU/default_config/aida_tlu_clk_config.txt";
            clkConfFile = config.get<std::string>("CLOCK_CFG_FILE", defaultCfgFile);
            if(clkConfFile == defaultCfgFile) {
                LOG(logger_, WARNING)
                    << "TLU: Could not find the parameter for clock configuration in the INI file. Using the default.";
            }
            int clkres;
            clkres = m_tlu->InitializeClkChip(clkConfFile, verbose);
            if(clkres == -1) {
                LOG(logger_, CRITICAL) << "TLU: clock configuration failed.";
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
    m_launch_config.in0_STR = config.get<unsigned int>("in0_STR",0);
    m_launch_config.in1_STR = config.get<unsigned int>("in1_STR",0);
    m_launch_config.in2_STR = config.get<unsigned int>("in2_STR",0);
    m_launch_config.in3_STR = config.get<unsigned int>("in3_STR",0);
    m_launch_config.in4_STR = config.get<unsigned int>("in4_STR",0);
    m_launch_config.in5_STR = config.get<unsigned int>("in5_STR",0);
    m_launch_config.in0_DEL = config.get<unsigned int>("in0_DEL",0);
    m_launch_config.in1_DEL = config.get<unsigned int>("in1_DEL",0);
    m_launch_config.in2_DEL = config.get<unsigned int>("in2_DEL",0);
    m_launch_config.in3_DEL = config.get<unsigned int>("in3_DEL",0);
    m_launch_config.in4_DEL = config.get<unsigned int>("in4_DEL",0);
    m_launch_config.in5_DEL = config.get<unsigned int>("in5_DEL",0);
    m_launch_config.trigMaskHi = config.get<uint32_t>("trigMaskHi", 0xFFFF);
    m_launch_config.trigMaskLo = config.get<uint32_t>("trigMaskLo", 0xFFFE);
    m_launch_config.trigPol = config.get<uint64_t>("trigPol", 0x003F);
    m_launch_config.PMT1_V = config.get<float>("PMT1_V", 0.0);
    m_launch_config.PMT2_V = config.get<float>("PMT2_V", 0.0);
    m_launch_config.PMT3_V = config.get<float>("PMT3_V", 0.0);
    m_launch_config.PMT4_V = config.get<float>("PMT4_V", 0.0);
    m_launch_config.DUTMask = config.get<int32_t>("DUTMask",1);
    m_launch_config.DUTMaskMode = config.get<int32_t>("DUTMaskMode",0xff);
    m_launch_config.DUTMaskModeModifier = config.get<int32_t>("DUTMaskModeModifier",0xff);
    m_launch_config.DUTIgnoreBusy = config.get<int32_t>("DUTIgnoreBusy",0xF);
    m_launch_config.DUTIgnoreShutterVeto = config.get<int32_t>("DUTIgnoreShutterVeto",1);
    m_launch_config.EnableShutterMode = config.get<bool>("EnableShutterMode",0);
    m_launch_config.ShutterSource = config.get<int8_t>("ShutterSource",0);
    m_launch_config.ShutterOnTime = config.get<int32_t>("ShutterOnTime",0);
    m_launch_config.ShutterOffTime = config.get<int32_t>("ShutterOffTime",0),
    m_launch_config.ShutterVetoOffTime = config.get<int32_t>("ShutterVetoOffTime",0);
    m_launch_config.InternalShutterInterval = config.get<int32_t>("InternalShutterInterval",0);
    m_launch_config.InternalTriggerFreq = config.get<uint32_t>("InternalTriggerFreq", 0);
    m_launch_config.EnableRecordData = config.get<uint32_t>("EnableRecordData", 1);
}

void AidaTLUSatellite::launching() {
    LOG(logger_, INFO) << "Launching " << getCanonicalName();

    LOG(logger_,INFO) << "CONFIG ID: " + std::to_string(m_launch_config.confid);
    LOG(logger_,INFO) << "TLU VERBOSITY SET TO: " + std::to_string(m_launch_config.verbose);
    LOG(logger_,INFO) << "TLU DELAY START SET TO: " + std::to_string(m_launch_config.delayStart) + " ms";

    m_tlu->SetTriggerVeto(1);
    if( m_launch_config.skipconf){
        LOG(logger_,INFO) << "TLU SKIPPING CONFIGURATION (skipconf = 1)";
    }
    else{
        // Enable HDMI connectors
        LOG(logger_,INFO) << " -DUT CONFIGURATION";
        m_tlu->configureHDMI(1, m_launch_config.HDMI1_set, m_launch_config.verbose);
        m_tlu->configureHDMI(2, m_launch_config.HDMI2_set, m_launch_config.verbose);
        m_tlu->configureHDMI(3, m_launch_config.HDMI3_set, m_launch_config.verbose);
        m_tlu->configureHDMI(4, m_launch_config.HDMI4_set, m_launch_config.verbose);

        // Select clock to HDMI
        m_tlu->SetDutClkSrc(1, m_launch_config.HDMI1_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(2, m_launch_config.HDMI2_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(3, m_launch_config.HDMI3_clk, m_launch_config.verbose);
        m_tlu->SetDutClkSrc(4, m_launch_config.HDMI4_clk, m_launch_config.verbose);

        //Set lemo clock
        LOG(logger_,INFO) << " -CLOCK OUTPUT CONFIGURATION";
        m_tlu->enableClkLEMO(m_launch_config.LEMOclk, m_launch_config.verbose);

        // Set thresholds
        LOG(logger_,INFO) << " -DISCRIMINATOR THRESHOLDS CONFIGURATION";
        m_tlu->SetThresholdValue(0, m_launch_config.DACThreshold0, m_launch_config.verbose);
        m_tlu->SetThresholdValue(1, m_launch_config.DACThreshold1, m_launch_config.verbose);
        m_tlu->SetThresholdValue(2, m_launch_config.DACThreshold2, m_launch_config.verbose);
        m_tlu->SetThresholdValue(3, m_launch_config.DACThreshold3, m_launch_config.verbose);
        m_tlu->SetThresholdValue(4, m_launch_config.DACThreshold4, m_launch_config.verbose);
        m_tlu->SetThresholdValue(5, m_launch_config.DACThreshold5, m_launch_config.verbose);

        // Set trigger stretch and delay
        std::vector<unsigned int> stretcVec = {
            m_launch_config.in0_STR,
            m_launch_config.in1_STR,
            m_launch_config.in2_STR,
            m_launch_config.in3_STR,
            m_launch_config.in4_STR,
            m_launch_config.in5_STR};

        std::vector<unsigned int> delayVec = {
            m_launch_config.in0_DEL,
            m_launch_config.in1_DEL,
            m_launch_config.in2_DEL,
            m_launch_config.in3_DEL,
            m_launch_config.in4_DEL,
            m_launch_config.in5_DEL};

        LOG(logger_,INFO) << " -ADJUST STRETCH AND DELAY";
        m_tlu->SetPulseStretchPack(stretcVec, m_launch_config.verbose);
        m_tlu->SetPulseDelayPack(delayVec, m_launch_config.verbose);

        // Set triggerMask
        // The conf function does not seem happy with a 32-bit default. Need to check.
        LOG(logger_,INFO) << " -DEFINE TRIGGER MASK";
        m_tlu->SetTriggerMask(m_launch_config.trigMaskHi, m_launch_config.trigMaskLo);

        // Set triggerPolarity
        LOG(logger_,INFO) << " -DEFINE TRIGGER POLARITY";
        m_tlu->SetTriggerPolarity(m_launch_config.trigPol);

        // Set PMT power
        LOG(logger_,INFO) << " -PMT OUTPUT VOLTAGES";
        m_tlu->pwrled_setVoltages(m_launch_config.PMT1_V, m_launch_config.PMT2_V, m_launch_config.PMT3_V, m_launch_config.PMT4_V, m_launch_config.verbose);

        LOG(logger_,INFO) << " -DUT OPERATION MODE";
        m_tlu->SetDUTMask(m_launch_config.DUTMask, m_launch_config.verbose); // Which DUTs are on
        m_tlu->SetDUTMaskMode(m_launch_config.DUTMaskMode, m_launch_config.verbose); // AIDA (x1) or EUDET (x0)
        m_tlu->SetDUTMaskModeModifier(m_launch_config.DUTMaskModeModifier, m_launch_config.verbose); // Only for EUDET
        m_tlu->SetDUTIgnoreBusy(m_launch_config.DUTIgnoreBusy, m_launch_config.verbose); // Ignore busy in AIDA mode
        m_tlu->SetDUTIgnoreShutterVeto(m_launch_config.DUTIgnoreShutterVeto, m_launch_config.verbose); //

        LOG(logger_,INFO) << " -SHUTTER OPERATION MODE";
        m_tlu->SetShutterParameters(m_launch_config.EnableShutterMode,
            m_launch_config.ShutterSource,
            m_launch_config.ShutterOnTime,
            m_launch_config.ShutterOffTime,
            m_launch_config.ShutterVetoOffTime,
            m_launch_config.InternalShutterInterval,
            m_launch_config.verbose);
        LOG(logger_,INFO) << " -AUTO TRIGGER SETTINGS";
        m_tlu->SetInternalTriggerFrequency(m_launch_config.InternalTriggerFreq, m_launch_config.verbose);

        LOG(logger_,INFO) << " -FINALIZING TLU CONFIGURATION";
        m_tlu->SetEnableRecordData(m_launch_config.EnableRecordData);
        m_tlu->GetEventFifoCSR();
        m_tlu->GetEventFifoFillLevel();
    }
}

void AidaTLUSatellite::landing() {
    LOG(logger_, INFO) << "Landing TLU";

    std::lock_guard<std::mutex> lock {m_tlu_mutex};

    LOG(logger_,INFO) << "  Set all PMT_V = 0";
    m_tlu->pwrled_setVoltages(0, 0, 0, 0, m_launch_config.verbose);
}

void AidaTLUSatellite::reconfiguring(const constellation::config::Configuration& /*partial_config*/) {}

void AidaTLUSatellite::starting(std::string_view run_identifier) {
    LOG(logger_, INFO) << "Starting run " << run_identifier << "...";
}

void AidaTLUSatellite::stopping() {
    LOG(logger_, INFO) << "Stopping run...";
}

void AidaTLUSatellite::running(const std::stop_token& stop_token) {
    LOG(logger_, INFO) << "Starting run loop...";
    sleep(1);
    LOG(logger_, INFO) << "Running, ";

    while(!stop_token.stop_requested()) {
        // sleep 1s
        sleep(1);
        LOG(logger_, INFO) << "...keep on running";
    }

    LOG(logger_, INFO) << "Exiting run loop";
}
