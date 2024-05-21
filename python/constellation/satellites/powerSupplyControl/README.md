---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Keithley6517B"
description: "Satellite for controlling a Keithley 6517B via RS-232"
---

## Description

This satellite can be used to control a Keithley model 6517B via an RS-232 interface. The power supply sources a given voltage, and measures the current.

## Parameters

* `port`: Hardware port that the Keithley is connected to. Use `COM<n>` for Windows and `/dev/ttyUSB<n>` for Unix systems, where `n` is an integer. Note that the port has to have sufficient permissions.
* `baud_rate`: Baud rate for the communication. Has to match the setting on the device.
* `measure`: Quantity to measure. Can be `V`, `I`, or `R`.
* `autorange`: Automatic range selection for the current measurement. Possible values are `ON` and `OFF`.
* `voltage_limit`: Overvoltage protection limit, set in the hardware.
* `minimum_allowed_voltage`: Software limit for allowed minimum voltage.
* `maximum_allowed_voltage`: Software limit for allowed maximum voltage.

* `stat_publishing_interval`: Interval that stat information is published with, in seconds.
* `safe_voltage_level`: Safe level, where the boasing starts, and that is ramped to in case of constellation issues.
* `voltage_step`: Step size used when ramping voltage.
* `settle_time`: Time between steps when ramping voltage.
* `voltage_set`: Voltage to source.

* `sample_points`: Size of the buffer (in samples) that is used to read the voltage and current.
* `trigger_delay`: Time between sampling to the buffer.

## Custom commands

* `ramp_voltage`: Ramp voltage to a given value. Function takes three arguments; value to ramp to (in volts), step size (in volts), and optionally settle time (in seconds). Allowed in the states `INIT` and `ORBIT`.
* `get_current`: Read the current current. Takes no parameters.

## Usage

```TOML
[satellites.Keithley6517B.device1]
port = "/dev/ttyUSB0"
baud_rate = 19200
measure = "I"
autorange = "ON"
voltage_limit = 20
minimum_allowed_voltage = -0.4
maximum_allowed_voltage = 0.4

stat_publishing_interval = 5.0
safe_voltage_level = 0.0
voltage_step = 0.05
voltage_set = 0.4
settle_time = 0.1

sample_points = 2
trigger_delay = 0.001
```
