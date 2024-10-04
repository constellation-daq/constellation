---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Flight Recorder"
subtitle: "Satellite to record log messages from a Constellation"
---

## Description

- configure global log level to listen to
- configure per-satellite log level or topic to listen to
- provide file pattern to be used
- handover to new log file happens with `starting`


## Building

The Flight Recorder satellite has no external dependencies and is build by default.

## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required.

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `my_param` | Unsigned integer | Number of channels to be used | `1024` |
| `other_param` | String | Name of the communication module | `"antenna"` |

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


| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_channel_reading` | This command returns the reading from the given channel number | channel number, `int` | channel reading, `double` | `INIT`, `ORBIT` |
| `get_module_name` | Reads the name of the communication module | - | module name, `string` | all |


## Metrics

The following metrics are distributed by this satellite and can be subscribed to. Timed metrics provide an interval in units of time, triggered metrics in number of calls.

| Metric | Description | Interval | Type |
|--------|-------------|----------|------|
| `STAT/CPULOAD` | Current CPU load of the satellite host machine | 3s | AVERAGE |
| `STAT/TEMP` | Highest reported system temperature of the satellite | 5s | AVERAGE |
| `STAT/EVENTS` | Currently processed event number | 100 | LAST_VALUE |
