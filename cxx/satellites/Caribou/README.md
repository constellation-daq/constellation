---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Caribou"
subtitle: "Constellation Satellite for controlling Caribou devices via their Peary interface"
---

## Description

This module allows to integrate devices running with the Caribou readout system into the Constellation ecosystem.

This satellite supports the reconfiguration transition. Any registers or memory registers known to the configured Caribou device can be updated during reconfiguration.

### FSM State Implementations

The satellite interfaces the Peary device manager to add devices and to control them. The following actions are performed in the different FSM stages:

* **Initialization**: The device to be instantiated is taken from the configuration parameter `type`. Device names are case sensitive and have to be available in the
  linked Peary installation. In addition, the following configuration keys are available for the initialization:

  * `peary_verbosity`: Set the Peary-internal logging verbosity for output on the terminal of the satellite. Please refer to the Peary documentation for more information. The verbosity can be changed at any point using the `peary_verbosity` command this satellite exposes.
  * `number_of_frames`: This key allows buffering of multiple Caribou device frames into a single Constellation data message. If set to a value larger than `1`, frames are first buffered and upon reaching the desired buffer depth, they are collectively sent. A value of, for example, `number_of_frames = 100` would therefore result in one message being sent every 100 frames read from the device. This message would contain 100 frames with the individual data blocks. This can be used to reduce the number of packets sent via the network and to better make use of available bandwidth.
  * `adc_signal`, `adc_frequency`: These keys can be used to sample the slow ADC on the Carboard in regular intervals. Here, `adc_signal` is the name of the ADC input channel as assigned via the Peary periphery for the given device, and `adc_frequency` is the number of frames read from the device after which a new sampling is attempted, the default is 1000. If no `adc_signal` is set, no ADC reading is attempted.

  **All other parameters are converted to strings and forwarded to the Peary instance**.

* **Launching**: During launch, the device is powered using Peary's `powerOn()` command. After this, the satellite waits for one second in order to allow the AIDA TLU to fully configure and make the clock available on the DUT outputs. Then, the `configure()` command of the Peary device interface is called.

* **Reconfiguration**: Any register returned by the device by `list_registers()` or memory registers from `list_memories()` can be updated during reconfiguration. The satellite attempts to read any of these values from the provided reconfiguration config and, if successful, updates the device.

* **Start / Stop**: During start and stop, the corresponding Peary device functions `daqStart()` and `daqStop()` are called.


### Other Features

Sometimes it is necessary to run two devices through Caribou, one of which only requires power and configuration, while the readout happens over the other. For this purpose, the parameter `secondary_device` can be set. This **secondary device** will only be powered (via `powerOn()`) and configured (via `configure()`) but no data will be attempted to retrieve. Readout is only performed for the primary device.

Since the Peary device libraries are not thread-safe, all access to Peary libraries is guarded using a `std::lock_guard` with a central mutex to avoid concurrent device access. The Peary device manager itself ensures that no two instances are executed on the same hardware. This means, only one Constellation satellite can be started per Caribou board.

## Parameters

The following parameters are read and interpreted by this satellite:

| Parameter          | Description | Type | Default Value |
|--------------------|-------------|------|---------------|
| `type`             | Type of the Caribou device to be instantiated, corresponds to the (case-sensitive) device class name | `string` | - |
| `peary_verbosity`  | Verbosity of the Peary logger. See custom commands for possible values. The verbosity can be changed at any time using the custom command described below | `string` | `INFO` |
| `adc_signal`       | Optional channel name of the Carboard ADC to be read in regular intervals | `string` | `"antenna"` |
| `adc_frequency`    | Number of frames read from the attached Caribou device until ADC is sampled again | `int` | 1000 |
| `number_of_frames` | Number of Caribou device frames to be sent with a single Constellation data message | `int` | 1 |


### Configuration Example

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point

```ini
[Caribou.H2M]
type = "H2M"
config_file = "~/h2m_config.conf"
```

## Custom Commands

The Caribou satellite exposes the following commands via the control interface. None of these commands alter the state of the attached device, but they only provide additional information on the device and its periphery.


| Command     | Description | Arguments | Return Value | Allowed States |
|-------------|-------------|-----------|--------------|----------------|
| `peary_verbosity` | Set verbosity of the Peary logger | Desired verbosity as string, possible values are `FATAL`, `STATUS`, `ERROR`, `WARNING`, `INFO`, `DEBUG` and `TRACE` | - | all |
| `get_peary_verbosity` | Get the currently configured verbosity of the Peary logger | - | Current Peary verbosity level | all |
| `list_registers` | List all available register names for the attached Caribou device | - | List of registers, array of `string` | `INIT`, `ORBIT`, `RUN` |
| `list_memories` | List all memory registers for the attached Caribou device | - | List of memory registers, array of `string` | `INIT`, `ORBIT`, `RUN` |
| `get_voltage` | Get selected output voltage of the attached Caribou device | Voltage name, `string` | Voltage reading in V, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_current` | Get selected output current of the attached Caribou device | Current name, `string` | Current reading in A, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_power` | Get selected output power of the attached Caribou device | Power name, `string` | Power reading in W, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_register` | Read the value of a register on the attached Caribou device | Register name, `string` | Register reading, `int` | `INIT`, `ORBIT`, `RUN` |
| `get_memory` | Read the value of a FPGA memory register on the attached Caribou device | Memory register name, `string` | Memory register reading, `int` | `INIT`, `ORBIT`, `RUN` |
| `get_adc` | Read a voltage from the Carboard ADC | Voltage name, `string` | ADC channel reading, `double` | `INIT`, `ORBIT`, `RUN` |
