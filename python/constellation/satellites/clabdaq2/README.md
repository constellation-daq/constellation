---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Red Pitaya Satellites for CLABDAQ2"
description: "Satellites running on custom FPGA firmware on RedPityas"
---

## Description

FIXME

This section will describe the functionality of the satellite and any relevant information about attached hardware and requirements thereof.

## Prerequisites

- The satellites must be run as `root` user to access the necessary registers to read data, set parameters on the FPGA and access GPIO pins.

## Parameters

FIXME

- `voltage`: Voltage value for the example device. Required.
- `current`: Current value for the example device. Required.
- `sample_period`: Time between executions of the voltage sampling/print-out in the example satellite. Required.

## Usage

FIXME

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point
