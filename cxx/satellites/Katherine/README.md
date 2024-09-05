---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Katherine"
subtitle: "Satellite to control the Timepix3 readout system Katherine"
---

## Description

This satellite controls the Katherine readout system for Timepix3 detectors.


## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required.

| Parameter     | Description | Type | Default Value |
|---------------|-------------|------|---------------|

| `ip_address`        | IP address of the Katherine system | String | |
| `dacs_file`         | Path to file with DAC settings     | Path | |
| `px_config_file`    | Path to file with individual pixel trim settings | Path | |
| `positive_polarity` | Threshold polarity switch | Bool | `true`    |
| `sequential_mode`   | Switch to enable/disable sequential mode | Bool | `false`   |
| `op_mode`           | Operation mode, can be `TOA_TOT`, `TOA`, `EVT_ITOT` or `MASK` | | `TOA_TOT` |
| `shutter_mode`      | Shutter operation mode, can be `POS_EXT`, `NEG_EXT`, `POS_EXT_TIMER`, `NEG_EXT_TIMER` or `AUTO` | | `AUTO` |
| `shutter_width`     | Width of the shutter window, only relevant for shutter modes `POS_EXT_TIMER` and `NEG_EXT_TIMER` | Int | |
| `pixel_buffer`      | Depth of the pixel buffer. This many pixel hits are accumulate before sending | Int  | 65536     |
| `no_frames`         | Number of frames to acquire. Needs to be set to 1 for data-driven mode (`sequential_mode = false`) | Int | 1 |

### Configuration Example

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point

```ini
[Sputnik.1]
my_param = 7
other_param = "antenna"
```

## Custom Commands

This section describes all custom commands the satellite exposes to the command interface. The description should contain the name and the description of the
command as well as all of its arguments, the return value and the allowed states:


| Command     | Description | Arguments | Return Value | Allowed States |
|-------------|-------------|-----------|--------------|----------------|
| `get_channel_reading` | This command returns the reading from the given channel number | channel number, `int` | channel reading, `double` | `INIT`, `ORBIT` |
| `get_module_name` | Reads the name of the communication module | - | module name, `string` | all |


## Metrics

The following metrics are distributed by this satellite and can be subscribed to. Timed metrics provide an interval in units of time, triggered metrics in number of calls.

| Metric         | Description | Interval | Type |
|----------------|-------------|----------|------|
| `STAT/CPULOAD` | Current CPU load of the satellite host machine | 3s | AVERAGE |
| `STAT/TEMP`    | Highest reported system temperature of the satellite | 5s | AVERAGE |
| `STAT/EVENTS`  | Currently processed event number | 100 | LAST_VALUE |
