---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "LakeShore218"
description: "Satellite controlling a LakeShore Model 218 temperature monitor"
---

## Description

This satellite uses the RS-232 serial port of LakeShore Model 218 temperature monitors to read the temperature of the
connected temperature sensors.

```{note}
This satellite requires write access to the serial port or USB-to-serial converter the device is attached to. It is
recommended to **not run this satellite with root privileges** but to allow the regular user access to the required port
instead.
On Unix, if the port is e.g. `/dev/ttyUSB0`, the group can be found via `stat /dev/ttyUSB0` (the Gid entry).
If the group is e.g. `dialout`, the permissions can be given with `sudo usermod -a -G dialout $USER`.
A restart is required for the changes to be effective.
```

```{note}
Since the LakeShore Model 218 has a male RS-232 connector, most USB-to-serial converters require an adapter. This adapter has
to be a [null modem](https://en.wikipedia.org/wiki/Null_modem), where the TX and RX lines are switched.
```

## Requirements

The Keithley satellite requires the `[visa]` component, which can be installed with:

::::{tab-set}
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[visa]
```

:::
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[visa]
```

:::
::::

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `port` | Serial port to connect to | String | - |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `TEMP_1` | Temperature 1 | Float | `LAST_VALUE` | 5s |
| `TEMP_2` | Temperature 2 | Float | `LAST_VALUE` | 5s |
| `TEMP_3` | Temperature 3 | Float | `LAST_VALUE` | 5s |
| `TEMP_4` | Temperature 4 | Float | `LAST_VALUE` | 5s |
| `TEMP_5` | Temperature 5 | Float | `LAST_VALUE` | 5s |
| `TEMP_6` | Temperature 6 | Float | `LAST_VALUE` | 5s |
| `TEMP_7` | Temperature 7 | Float | `LAST_VALUE` | 5s |
| `TEMP_8` | Temperature 8 | Float | `LAST_VALUE` | 5s |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_temp` | Temperature | Channel (Integer) | Float | any |
