/**
 * @file
 * @brief Implementation of TLU Satellite
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#include "TluSatellite.hpp"

#include <unistd.h>

#include "constellation/core/logging/log.hpp"
#include "constellation/satellite/Satellite.hpp"

// generator function for loading satellite from shared library
extern "C" std::shared_ptr<Satellite> generator(std::string_view type_name, std::string_view satellite_name) {
    return std::make_shared<TluSatellite>(type_name, satellite_name);
}

TluSatellite::TluSatellite(std::string_view type, std::string_view name) : Satellite(type, name), m_starttime(0), m_lasttime(0), m_duration(0){
    LOG(logger_, STATUS) << "TluSatellite " << getCanonicalName() << " created";
}

void TluSatellite::initializing(constellation::config::Configuration& config) {
    LOG(logger_, INFO) << "Initializing " << getCanonicalName();

    LOG(logger_, INFO) << "TLU INITIALIZE ID: " + std::to_string(config.get<int>("initid", 0));
    std::string uhal_conn = "file://./../../../cxx/satellites/tlu/default_config/aida_tlu_connection.xml";
    std::string uhal_node = "fmctlu.udp";
    uhal_conn = config.get<std::string>("ConnectionFile", uhal_conn);
    uhal_node = config.get<std::string>("DeviceName",uhal_node);
    m_tlu = std::unique_ptr<tlu::AidaTluController>(new tlu::AidaTluController(uhal_conn, uhal_node));

    if( config.get<bool>("skipini", false) ){
        LOG(logger_, INFO) << "TLU SKIPPING INITIALIZATION (skipini = 1)";
    }
    else{
        m_verbose = config.get<uint8_t>("verbose",0);
        LOG(logger_, INFO) << "TLU VERBOSITY SET TO: " + std::to_string(m_verbose);

        // Define constants
        m_tlu->DefineConst(config.get<int>("nDUTs", 4), config.get<int>("nTrgIn", 6));

        // Import I2C addresses for hardware
        // Populate address list for I2C elements
        m_tlu->SetI2C_core_addr(config.get<uint8_t>("I2C_COREEXP_Addr", 0x21));
        m_tlu->SetI2C_clockChip_addr(config.get<uint8_t>("I2C_CLK_Addr", 0x68));
        m_tlu->SetI2C_DAC1_addr(config.get<uint8_t>("I2C_DAC1_Addr",0x13) );
        m_tlu->SetI2C_DAC2_addr(config.get<uint8_t>("I2C_DAC2_Addr",0x1f) );
        m_tlu->SetI2C_EEPROM_addr(config.get<uint8_t>("I2C_ID_Addr", 0x50) );
        m_tlu->SetI2C_expander1_addr(config.get<uint8_t>("I2C_EXP1_Addr",0x74));
        m_tlu->SetI2C_expander2_addr(config.get<uint8_t>("I2C_EXP2_Addr",0x75) );
        m_tlu->SetI2C_pwrmdl_addr(config.get<uint8_t>("I2C_DACModule_Addr",  0x1C), config.get<uint8_t>("I2C_EXP1Module_Addr",  0x76), config.get<uint8_t>("I2C_EXP2Module_Addr",  0x77), config.get<uint8_t>("I2C_pwrId_Addr",  0x51));
        m_tlu->SetI2C_disp_addr(config.get<uint8_t>("I2C_disp_Addr",0x3A));

        // Initialize TLU hardware
        m_tlu->InitializeI2C(m_verbose);
        m_tlu->InitializeIOexp(m_verbose);
        if (config.get<bool>("intRefOn", false)){
            m_tlu->InitializeDAC(config.get<bool>("intRefOn", false), config.get<float>("VRefInt", 2.5), m_verbose);
        }
        else{
            m_tlu->InitializeDAC(config.get<bool>("intRefOn", false), config.get<float>("VRefExt", 1.3), m_verbose);
        }

        // Initialize the Si5345 clock chip using pre-generated file
        if (config.get<bool>("CONFCLOCK", true)){
            std::string  clkConfFile;
            std::string defaultCfgFile= "./../../../../cxx/satellites/tlu/default_config/aida_tlu_clk_config.txt";
            clkConfFile= config.get<std::string>("CLOCK_CFG_FILE", defaultCfgFile);
            if (clkConfFile== defaultCfgFile){
                LOG(logger_, WARNING) << "TLU: Could not find the parameter for clock configuration in the INI file. Using the default.";
            }
            int clkres;
            clkres= m_tlu->InitializeClkChip( clkConfFile, m_verbose  );
            if (clkres == -1){
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
}

void TluSatellite::launching() {
    LOG(logger_, INFO) << "Launching TLU";
}

void TluSatellite::landing() {
    LOG(logger_, INFO) << "Landing TLU";
}

void TluSatellite::reconfiguring(const constellation::config::Configuration& /*partial_config*/) {}

void TluSatellite::starting(std::uint32_t run_number) {
    LOG(logger_, INFO) << "Starting run " << run_number << "...";
}

void TluSatellite::stopping() {
    LOG(logger_, INFO) << "Stopping run...";
}

void TluSatellite::running(const std::stop_token& stop_token) {
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
