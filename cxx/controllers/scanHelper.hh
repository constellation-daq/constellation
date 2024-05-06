#pragma once

#include <cstdlib>
#include <map>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <vector>

// #include "eudaq/Config.hh"

/**
 * @brief The Scan class
 * @abstract Store all parameters required to perform a scan
 * Additionally, the configs to be used are created and stored
 *
 */
class Scan {
public:
    /**
     * @brief The ScanSection struct, which stores the information from one user section in a Scan Configuration
     */
    struct ScanSection {
        double start, step, stop, defaultV;
        int secIndex;
        bool nested;
        int scanParallelTo;
        std::string name, parameter, eventCounter;

        ScanSection(double start,
                    double stop,
                    double step,
                    std::string name,
                    std::string parameter,
                    std::string eventCounter,
                    double defaultV,
                    int idx,
                    bool nested = false,
                    int scanParallelTo = 0)
            : start(start), stop(stop), step(step), name(name), nested(nested), parameter(parameter),
              eventCounter(eventCounter), defaultV(defaultV), secIndex(idx), scanParallelTo(scanParallelTo) {}
    };
    Scan()
        : m_allow_nested_scan(false), m_scan_is_time_based(true), m_time_per_step(0), m_repeatScans(false),
          m_events_per_step(-10), m_current_step(0), m_config_files(), m_steps_per_scan(), m_n_steps(0),
          m_scanned_parameter(), m_scanned_producer(), m_events_counting_component(), m_config_file_prefix("defaultScan") {
        reset();
    }

    /**
     * @brief setupScan
     * @param globalConfFile = default configuration file
     * @param scanConfFile   = scan config file
     * @return true if the scan could be configured correctly, false else
     */
    bool setupScan(std::string globalConfFile, std::string scanConfFile);
    /**
     * @brief Reset all local parameters, created files are not deleted
     */
    void reset();
    /**
     * @brief currentStep
     * @return Current step in scan. Can be larger than the number of steps if a scan is started in an lopp
     */
    int currentStep() const { return m_current_step; }
    /**
     * @brief nSteps
     * @return number of steps in scan
     */
    int nSteps() const { return m_n_steps; }

    /**
     * @brief timePerStep
     * @return time per Step
     */
    int timePerStep() const { return m_time_per_step; }
    /**
     * @brief eventsPerStep
     * @return events per Step
     */
    int eventsPerStep() const { return m_events_per_step; }

    /**
     * @brief nextConfig
     * @return the configuration filename for the next step in the scan or 'finished', if the scan is completed
     */
    std::string nextConfig();
    /**
     * @brief scanIsTimeBased
     * @return true if the steps are selected based on runtime
     */
    bool scanIsTimeBased() const { return m_scan_is_time_based; }
    /**
     * @brief currentCountingComponent
     * @return the connection which provides the number of events required in a scan step
     */
    std::string currentCountingComponent() const;
    /**
     * @brief scanHasbeenStarted
     * @return true if the scan has been started already
     */
    bool scanHasbeenStarted() const { return m_first_step_done; }
    /**
     * @brief scanStarted
     */
    void scanStarted() { m_first_step_done = true; }

private:
    /**
     * @brief ERROR message from EUDAQ_ERROR with return statement
     * @param msg
     * @return false
     */
    bool scanError(std::string msg) {
        std::cout << msg << std::endl;
        EUDAQ_ERROR(msg);
        return false;
    }
    /**
     * @brief read the global config
     * @param conf
     * @return true if reading completed, false otherwise
     */
    bool readGlobal(eudaq::ConfigurationSP conf);

    /**
     * @brief Store configuration for a step
     * @param conf Configuration to be stored
     * @return True if file created
     */
    bool storeConfigFile(eudaq::ConfigurationSP conf);

    /**
     * @brief Creates the configuration files for each scan
     * @param condition Recursive iterator over the scan sections 'sec'
     * @param conf Default configuration
     * @param sec Scan section being read
     */
    void createConfigsMulti(unsigned condition,
                            eudaq::ConfigurationSP conf,
                            std::unordered_map<int, std::vector<ScanSection>> mapSec);

    /**
     * @brief add additional information for each section
     * @param s Section read in
     */
    void addSection(ScanSection s);

    std::vector<std::string> m_config_files;
    bool m_allow_nested_scan = false;
    bool m_scan_is_time_based = true;
    bool m_repeatScans = false;
    bool m_first_step_done = false;
    int m_time_per_step = 0;
    int m_events_per_step = 0;
    std::vector<int> m_steps_per_scan;
    int m_n_steps = 0;
    std::vector<std::string> m_scanned_parameter;
    std::vector<std::string> m_scanned_producer;
    std::vector<std::string> m_events_counting_component;
    std::string m_config_file_prefix;
    int m_current_step = 0;
};
