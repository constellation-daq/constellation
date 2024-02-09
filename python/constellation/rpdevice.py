#!/usr/bin/env python3

import rp
import numpy as np


# Class for the RedPitaya 250-14 4CH input
class RedPitaya250_14_4CH:
    def __init__(self, conf):
        self.configuration_file = conf
        self.set_device_configuration()

    # ===========================================================================
    # Open API interface
    # ===========================================================================
    def open_device_interface(self):
        rp.rp_Init()
        print("Device Ready")

    # ===========================================================================
    # Switch on/off the acquisition
    # ===========================================================================
    def enable_acquisition(self):
        rp.rp_AcqStart()
        print("Acquisition On")

    def disable_acquisition(self):
        rp.rp_AcqStop()
        print("Acquisition Off")

    def initialize_readpointer(self):
        self._readpos = self.get_write_pointer()

    # ===========================================================================
    # Close API interface
    # ===========================================================================
    def close_device_interface(self):
        rp.rp_Release()
        print("Device Closed")

    # ===========================================================================
    # Do initial configuration
    # ===========================================================================
    def set_device_configuration(self):
        # Initialization of the Serial interface
        try:
            self.open_device_interface()
            # Specifies the size, number of channels and data type of data buffer
            self._buffChannels = self.configuration_file["Device"]["Configuration"][
                "bufferChannels"
            ]
            self._buffSize = self.configuration_file["Device"]["Configuration"][
                "bufferSize"
            ]
            self._buffInt = self.configuration_file["Device"]["Configuration"][
                "bufferInt"
            ]
            self._buffDouble = self.configuration_file["Device"]["Configuration"][
                "bufferDouble"
            ]
            self._buffFloat = self.configuration_file["Device"]["Configuration"][
                "bufferFloat"
            ]
            """
            # Specifies the data format for sampling
            self._dataFormat = self.configuration_file["Device"]["Configuration"][
                "dataFormat"
            ]

            # Specifies trigger delay, level and if acquisition should stop
            self._triggerDelay = self.configuration_file["Device"]["Configuration"][p
                "triggerDelay"
            ]
            self._triggerLevel = self.configuration_file["Device"]["Configuration"][
                "triggerLevel"
            ]
            self._triggerStop = self.configuration_file["Device"]["Configuration"][
                "triggerStop"
            ]

            # Specifies clock synchronization
            self._clksynchronization = self.configuration_file["Device"][
                "Configuration"
            ]["clkSynchronization"]

            # Specifies decimation
            self._acqDecimation = self.configuration_file["Device"]["Configuration"][
                "acqDecimation"
            ] """

            # Set up the source
            rp.rp_Reset()
            rp.rp_AcqReset()
            rp.rp_AcqSetDecimation(rp.RP_DEC_1)
            # rp.rp_AcqSetDecimation(self._acqDecimation)

            # Set up daisy chain synchronization
            # if self._clksynchronization == "ON":
            #    rp.rp_SetEnableDiasyChainClockSync(True)

            # Set up the buffer
            self._buffer = {}
            for idx in range(self._buffChannels):
                if self._buffInt:
                    self._buffer[idx] = rp.i16Buffer(self._buffSize)
                elif self._buffDouble:
                    self._buffer[idx] = rp.d16Buffer(self._buffSize)
                elif self._buffFloat:
                    self._buffer[idx] = rp.f16Buffer(self)

            self._rpChannels = [rp.RP_CH_1, rp.RP_CH_2, rp.RP_CH_3, rp.RP_CH_4]

            """ NOTE: This is probably not necessary params in our case but might be good for more general purpose. Fix trig assignment for all other cases
            # Set up the trigger
            if self._triggerStop == "OFF":
                rp.rp_AcqSetArmKeep(True)
            else:
                rp.rp_AcqSetArmKeep(False)

             if(self._buffChannels == 4):
                rp.rp_AcqSetTriggerLevel(rp.RP_CH_1, self._triggerLevel)
                rp.rp_AcqSetTriggerLevel(rp.RP_CH_2, self._triggerLevel)
                rp.rp_AcqSetTriggerLevel(rp.RP_CH_3, self._triggerLevel)
                rp.rp_AcqSetTriggerLevel(rp.RP_CH_4, self._triggerLevel)
            rp.rp_AcqSetTriggerDelay(self._triggerDelay)

            self._readpos = self.get_write_pointer()
            # rp.rp_AcqSetTrigger() TODO: check via config for channel trig
            """

            """ print(
                "Device at Port "
                + self.configuration_file["Device"]["Configuration"]["Port"]
                + " Configured"
            ) """

        except ValueError:
            print("ERROR: No API connection. Check device!")

    # Reset and disconnect
    def disconnect(self):
        self.reset()
        rp.rp_Release()

    def reset(self):
        rp.rp_reset()  # Might want to change to rp_AcqReset()

    # Read from the buffer and collect data
    def sample_raw(self, channel, buffer, chunk):
        """Sample data from given channel"""

        rp.rp_AcqGetDataRaw(channel, self._readpos, chunk, buffer.cast())

        data_raw = np.zeros(chunk, dtype=int)

        for idx in range(0, chunk, 1):
            data_raw[idx] = buffer[idx]

        return data_raw

    def get_data(
        self,
    ):  # TODO: Check performance. This was lifted from the redpitaya examples
        """Sample every buffer channel and return raw data in numpy array"""

        # Obtain to which point the buffer has written
        self._writepos = self.get_write_pointer()

        # Skip sampling if we haven't moved
        if self._readpos == self._writepos:
            return

        # Calculate sample size
        chunk = (self._writepos - self._readpos + self._buffSize) % self._buffSize

        # Sample data for every channel and convert to list of numpy arrays
        data = []
        for channel in range(self._buffChannels):
            data.append(
                self.sample_raw(self._rpChannels[channel], self._buffer[channel], chunk)
            )

        # Update readpointer
        self._readpos = self._writepos

        data = np.vstack(data, dtype=int).transpose().flatten()
        return data

    # NOTE: current function only grabs the write pointer
    def get_write_pointer(self):
        """Obtain write pointer for given channel"""
        return rp.rp_AcqGetWritePointer()[1]

    # Monitoring
    def state(self):
        print("If the script stops here, the output is turned off\n.")
        print("Read pointer is at:", self._readpos)
        print(
            "Write pointer is at:",
            {self._rpChannels: self.get_write_pointer(self._rpChannels)},
        )
