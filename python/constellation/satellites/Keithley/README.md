---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Keithley"
subtitle: "Satellite controlling a Keithley power supply"
---

## Description

This satellite uses the RS-232 serial port of Keithley Source Measure Units to control them remotely. The satellite can
control a single Keithley SMU and operate it as voltage source. Among other settings, it is possible to configure the
over-voltage protection (OVP), the current compliance as well as the target voltage. Voltage changes are performed as ramps,
i.e. the voltage is incremented or decremented in small steps.

```{note}
This satellite requires write access to the serial port or USB-to-serial converter the device is attached to. It is
recommended to **not run this satellite with root privileges** but to allow the regular user access to the required port
instead.
On Unix, if the port is e.g. `/dev/ttyUSB0`, the group can be found via `stat /dev/ttyUSB0` (the Gid entry).
If the group is e.g. `dialout`, the permissions can be given with `sudo usermod -a -G dialout $USER`.
A restart is required for the changes to be effective.
```

## Requirements

The Keithley satellite requires the `[keithley]` component, which can be installed e.g. via:

```sh
pip install --no-build-isolation -e .[keithley]
```

## Supported devices

The device model to be controlled by this satellite can be selected via the `device` configuration parameter. The following
devices are current supported:

### `2410`

The `2410` device supports the Keithley Series 2400 SourceMeter in RS-232 mode.
The following serial communication settings are required and have to be configured on the SMU before starting the satellite.
They can be found at MENU -> COMMUNICATION -> RS-223


- `BAUD`: `19200`
- `BITS`: `8`
- `PARITY`: `EVEN`
- `TERMINATOR`: `<CR+LF>`
- `FLOW-CTRL`: `NONE`

Possible terminals are `front` and `rear`.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `device` | Device | String | - |
| `port` | Serial port to connect to | String | - |
| `terminal` | Terminal output to control | String | - |
| `voltage` | Target output voltage | Float | - |
| `voltage_step` | Voltage step in which to ramp to target voltage | Float | - |
| `settle_time` | Time to wait before continuing with the next voltage step | Float | - |
| `ovp` | Voltage limit for over-voltage protection | Float | - |
| `compliance` | Current limit in Ampere | Float | - |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `VOLTAGE` | Voltage output | Float | `LAST_VALUE` | 10s |
| `CURRENT` | Current output | Float | `LAST_VALUE` | 10s |
| `IN_COMLIANCE` | If in compliance | Bool | `LAST_VALUE` | 10s |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `in_compliance` | Check if the current is in compliance | - | Bool | `ORBIT`, `RUN` |
| `read_output` | Reads voltage, current and timestamp | - | Dictionary | `ORBIT`, `RUN` |
