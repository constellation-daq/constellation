---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Katherine"
subtitle: "Satellite to control the Timepix3 readout system Katherine"
---

## Description

This satellite controls the Katherine readout system for Timepix3 detectors.


## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required unless
noted otherwise.

| Parameter     | Description | Type | Default Value |
|---------------|-------------|------|---------------|
| `ip_address`        | IP address of the Katherine system | String | |
| `chip_id`           | ID of the chip to be operated. Optional setting, this will only be checked when provided. If it does not match, an error is reported. | String | |
| `dacs_file`         | Path to file with DAC settings     | Path | |
| `px_config_file`    | Path to file with individual pixel trim settings | Path | |
| `positive_polarity` | Threshold polarity switch | Bool | `true`    |
| `sequential_mode`   | Switch to enable/disable sequential mode | Bool | `false`   |
| `op_mode`           | Operation mode, can be `TOA_TOT`, `TOA`, `EVT_ITOT` or `MASK` | | `TOA_TOT` |
| `shutter_mode`      | Shutter operation mode, can be `POS_EXT`, `NEG_EXT`, `POS_EXT_TIMER`, `NEG_EXT_TIMER` or `AUTO` | | `AUTO` |
| `shutter_width`     | Width of the shutter window, only relevant for shutter modes `POS_EXT_TIMER` and `NEG_EXT_TIMER` | Int | |
| `pixel_buffer`      | Depth of the pixel buffer. This many pixel hits are accumulate before sending. The default corresponds to one full Timepix3 frame | Int  | 65536     |
| `data_buffer`       | Depth of the data buffer in units of Timepix3 packets (6 bytes each). Data is either decoded or sent directly from this buffer. The default corresponds to 200 MB memory. | Int  | 34952533 |
| `no_frames`         | Number of frames to acquire. Needs to be set to 1 for data-driven mode (`sequential_mode = false`) | Int | 1 |

### Configuration Example

The following example configuration could be used to control a Katherine readout system satellite with a Timepix3 detector.
The satellite is configured to connect to the system at `192.168.1.182` and to use the provided local files for DAC settings
and pixel matrix configuration. The pixel buffer is set to 300 pixel hits, all other settings are left at their defaults:

```ini
[Satellites.Katherine.TPX3]
ip_address = "192.168.1.182"
dacs_file = "E2-W0005_dacs.txt"
px_config_file = "E2-W0005_trimdacs.txt"
pixel_buffer = 300
```

## Custom Commands

The following custom commands are exposed to the command interface by this satellite:

| Command     | Description | Arguments | Return Value | Allowed States |
|-------------|-------------|-----------|--------------|----------------|
| `get_temperature_readout` | Read the current temperature from the Katherine readout board | - | Temperature in degree Celsius, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_temperature_sensor` | Read the current temperature from the temperature sensor | - | Temperature in degree Celsius, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_adc_voltage` | Read the voltage from the ADC channel provided as parameter | channel number, `int` | channel reading, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_chip_id` | Read the chip ID of the attached sensor | - | Chip ID, `string` | `INIT`, `ORBIT`, `RUN` |
