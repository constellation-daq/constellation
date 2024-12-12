---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "CaenHV"
description: "Satellite controlling a CAEN high-voltage crate such as the SY5527 and its modules"
---

## Description

This Satellite allows to control CAEN high-voltage crates such as the SY5527 and the inserted modules.

## Requirements

The CaenHV satellite requires the `[caenhv]` component, which can be installed with:

::::{tab-set}
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[caenhv]
```

:::
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[caenhv]
```

:::
::::

## Parameters

The following parameters need to be specified in the configuration file. System and connection parameters are required.

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `system` | The type of crate connected, e.g. `"SY5527"` | String | - |
| `link` | The type of connection, e.g. `"TCPIP"` or `"USB"` | String | - |
| `link_argument` | Additional information for the connection, e.g. the ip address `"192.168.8.2"` | String | - |
| `user` | The user name to connect with | String | - |
| `password` | The password to connect with | String | - |
| `metrics_poll_interval` | How often the metrics are polled, in seconds | - | - |
| `board[BNUM]_ch[CHNUM]_[PARNAME]` | Parameters for individual channels where `[BNUM]`is the board number, `[CHNUM]` the channel number and `[PARNAME]`the parameter to be configured. | - | - |

The available parameter names for `board[BNUM]_ch[CHNUM]_[PARNAME]` depend on the model of the board in use. For the A7435SN, this would be `V0Set`, `I0Set`, `V1Set`, `I1Set`, `RUp`, `RDWn`, `Trip`, `SVMax`, `VMon`, `IMon`, `Status`, `Pw`, `POn`, `TripInt`, `TripExt`, `ZCDetect`, and `ZCAdjust`

### Usage

A minimal configuration would be:

```ini
[satellites.CaenHV.sy5527]
# Device-specific system settings for the SY5527-controlling Satellite
system="SY5527"
link="TCPIP"
link_argument="192.168.8.2"
username="myuser"
password="mypassword!"
metrics_poll_interval=30

board1_ch1_V0Set = 1
board1_ch1_pw = "on"
```
