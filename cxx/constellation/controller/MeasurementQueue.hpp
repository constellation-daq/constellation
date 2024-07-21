/**
 * @file
 * @brief Helper class for queuing measurement series
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <queue>

#include "constellation/build.hpp"
#include "constellation/controller/Controller.hpp"
#include "constellation/core/config/Dictionary.hpp"

namespace constellation::controller {

    /** Measurement queue class which allows to queue and fetch measurement configurations which can be used to reconfigure
     * a constellation
     */
    class CNSTLN_API MeasurementQueue {
    public:
        using Measurement = std::map<std::string, constellation::config::Dictionary>;

        /**
         * @brief Construct a measurement queue
         */
        MeasurementQueue(Controller& controller) : controller_(controller) {};

        /**
         * @brief Destruct the measurement queue
         */
        virtual ~MeasurementQueue();

        /// @cond doxygen_suppress
        // No copy/move constructor/assignment
        MeasurementQueue(const MeasurementQueue& other) = delete;
        MeasurementQueue& operator=(const MeasurementQueue& other) = delete;
        MeasurementQueue(MeasurementQueue&& other) = delete;
        MeasurementQueue& operator=(MeasurementQueue&& other) = delete;
        /// @endcond

        void appendMeasurement(Measurement measurement) { measurements_.push(std::move(measurement)); }

        Measurement currentMeasurement() { return measurements_.front(); }

        std::size_t size() { return measurements_.size(); }

        /**
         * @brief Current progress of the measurement queue
         * @details Returns the fraction of measurements successfully performed over the total number of measurements. Since
         * measurements are popped from the queue once they have been done, this uses a separate counter for the total
         * number of measurements to be performed.
         *
         * @return Progress of the queue
         */
        double progress() { return static_cast<double>(size_at_start_ - measurements_.size()) / size_at_start_; }

        void start();

        void halt();

        void interrupt();

    private:
        /** Queue of measurements */
        std::queue<Measurement> measurements_;

        /** Number of queue elements at the start of the measurements */
        std::size_t size_at_start_;

        /** Controller to use for the measurements */
        Controller& controller_;
    };

} // namespace constellation::controller
