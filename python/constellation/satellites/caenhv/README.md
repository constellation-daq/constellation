---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "CAEN high-voltage crate Satellite"
description: "Satellite controlling a CAEN high-voltage crate such as the SY5527 and its modules ."
---

## Description

This Satellite allows to control CAEN high-voltage crates such as the SY5527 and the inserted modules.

For communication, the CAEN communication library, `CAENHVWrapper` as well as the Python bindings provided by [pycaenhv](https://github.com/vasoto/pycaenhv.git) need to be installed.

## Parameters

The following parameters need to be specified in the configuration file:

* `system`: the type of crate connected, e.g. `"SY5527"`.
* `link`: the type of connection, e.g. `"TCPIP"`.
* `link_argument` : additional information for the connection, e.g. the ip address `"192.168.8.2"`.
* `user` : the user name to connect with.
* `password` : the password to connect with.

Other channel parameters are structured like this:

* `board[BNUM]_ch[CHNUM]_[PARNAME]`: where `[BNUM]`is the board number, `[CHNUM]` the channel number and `[PARNAME]`the parameter to be configured. The exact parameter name depends on the model of the board in use. For the A7435SN, this would be `V0Set`, `I0Set`, `V1Set`, `I1Set`, `RUp`, `RDWn`, `Trip`, `SVMax`, `VMon`, `IMon`, `Status`, `Pw`, `POn`, `PDwn`, `ImRange`, `TripInt`, `TripExt`, `ZCDetect`, and `ZCAdjust`.

## Usage

A minimal configuration would be:

```ini
[satellites.caenhvsatellite.sy5527]
# Device-specific system settings for the SY5527-controlling Satellite
system="SY5527"
link="TCPIP"
link_argument="192.168.8.2"
username="myuser"
password="mypassword!"

board1_ch1_V0Set = 1
board1_ch1_pw = "on"
```

To start the Satellite, run

``` shell
SatelliteCaenHvCrate
```

or

``` shell
SatelliteCaenHvCrate --help
```

to get a list of the available command-line arguments.
