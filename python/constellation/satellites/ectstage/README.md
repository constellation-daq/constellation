---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "eCT Stage"
subtitle: "Satellite controlling the ThorLab LT300C linear and PRM1/MZ8 rotation stages"
---

## Description

This satellite allows the control of the ThorLab LT300C linear and PRM1/MZ8 rotation stages via the `pylablib` python package

## Configuration File

a `.toml` configuration file is needed to configure the satellite. The following parameters need to be specified in the configuration file. System and connection parameters are required.

### Stage Parameters
Declare as `[{stage_axis}]` eg: `[x]`,`[y]`,`[z]`,`[r]`. All parameters must be defined in config file.

| Parameter       | Description                                                            | Type      | Default Value [+] | Safety Limit                   |
|-----------------|------------------------------------------------------------------------|-----------|-------------------|--------------------------------|
| `port`          | Serial port name (eg:`"/dev/ttyUSB0"`)                                 | string    | -                 | -                              |
| `chan`          | Channel number if multiple stages are moved via same serial connection | number    | `0`               | -                              |
| `velocity`      | Velocity of the stage movement in mm/s                                 | int/float | -                 | `5`                            |
| `acceleration`  | Velocity of the stage movement in mm/s^2                               | int/float | -                 | 15                             |
| `home_position` | Start Position of all new runs in mm                                   | int/float | -                 | `0` to `290` for linear stages |

[+]Use the given default value if unsure of value

### Run Parameters
Declare as `[run]`. All parameters must be defined in config file.

| Parameter          | Description                            | Type                                   | Default Value [+] | Safety Limit                   |
|--------------------|----------------------------------------|------- --------------------------------|------------------ |--------------------------------|
| `active_axes`      | Axes/stages that must be initialised   | list of axis names eg: `["x","y"]`     | -                 | -                              |
| `pos_{stage_axis}` | move to position                       | single int/float OR three-value vector | `0`               | `0` to `290` for linear stages |

[+]Use the given default value if unsure of value

* If `pos_{stage_axis}` is a single value, the stage will move to this position, take data and go back home.
If `pos_{stage_axis}` is a three-vector eg: `[val_1,val_2,val_3]` the stage will move between `val_1` and `val_2` in steps of `val_3`.

A minimal configuration would be:

```ini
[x]
port = "/dev/ttyUSB0"
chan = 0
# in mm
velocity = 2
acceleration = 10
home_position = 10

[y]
port = "/dev/ttyUSB1"
chan = 0
# in mm
velocity = 2
acceleration = 10
home_position = 10

[z]
port = "/dev/ttyUSB3"
chan = 0
# in mm
velocity = 2
acceleration = 10
home_position = 10

[r]
port = "/dev/ttyUSB4"
chan = 0
# in deg
home_position = 180

[run]
active_axes = ["x","y","z","r"]

# in mm
pos_x = [10,30,10]
pos_y = [10,30,10]
pos_z = [10,30,10]
# in deg
pos_r = [175,185,10]
```

## Usage
To start the Satellite, run

``` shell
SatelliteECTstage
```

or

``` shell
SatelliteECTstage --help
```

to get a list of the available command-line arguments.
