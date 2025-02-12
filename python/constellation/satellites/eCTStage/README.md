---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "eCT Stage"
subtitle: "Satellite controlling the ThorLab LT300C linear and PRM1/MZ8 rotation stages"
---

## Description

This satellite allows the control of the ThorLab LT300C linear and PRM1/MZ8 rotation stages via the `pylablib` python package


## Prerequisites

Python package `pylablib` is required to steer the linear and rotational stages. Install using command:

```shell
pip install pylablib
```


## Initialization Parameters

To initialize `SatelliteECTstage`, one parameter is required.

| Parameter       | Description                                                                                                        | Type      |
|-----------------|--------------------------------------------------------------------------------------------------------------------|-----------|
| `config_file`   | Configuration file (`.toml`) containing all required variable. See Configuration File section for more information | string    |  


## Configuration File

a `.toml` configuration file is needed to configure the satellite. The following parameters need to be specified in the configuration file. System and connection parameters are required.


### Stage Parameters

Declare as `[{stage_axis}]` eg: `[x]`,`[y]`,`[z]`,`[r]`. All parameters must be defined in config file.

| Parameter       | Description                                                            | Type      | Default Value [+] | Safety Limit                   |
|-----------------|------------------------------------------------------------------------|-----------|-------------------|--------------------------------|
| `serial_no`     | Serial number of the stage. Used to make sure the correct stage is moving | 8-digit number| -            | -                              |
| `channel`          | Channel number if multiple stages are moved via same serial connection | number    | `0`               | -                              |
| `velocity`      | Velocity of the stage movement in `mm/s`                               | int/float | `1`               | max=`10`                       |
| `acceleration`  | Acceleration of the stage movement in `mm/s^2`                         | int/float | `1`               | max=`10`                       |
| `start_position`| Start Position of all new runs in `mm`. See "Modes of Operations"      | int/float | -                 | `0` to `299` for linear stages |

[+]Use the given default value if unsure of value


### Run Parameters

Declare as `[run]`. All parameters must be defined in config file.

| Parameter               | Description                                  | Type                                   | Safety Limit                   |
|-------------------------|----------------------------------------------|----------------------------------------|--------------------------------|
| `active_axes`           | Axes/stages that must be initialised         | list of axis names eg: `["x","y"]`     | -                              |
| `position_{stage_axis}`      | in `mm`. See "Modes of Operations"           | three-vector list  (int/float)         | `0` to `299` for linear stages |
| `stop_time_per_point_s` | wait time at each point (measurement window) in `s` | float                           | -                              |
| `readout_freq_s`        | stage position sending frequency (in `s`) to data writer | float                      | -                              |
| `save_config`           | (optional) If True, the configuration file will be saved in `path/to/constellation/data` folder    | bool| -           |


### Modes of Operation

* If `pos_{stage_axis}` is undefined, the stage will move to the home position of `{stage_axis}` and take data.

* If `pos_{stage_axis}` is a three-vector eg: `[val_1,val_2,val_3]` the stage will move between `val_1` and `val_2` in steps of `val_3`.


A minimal configuration would be:

```ini
[x]
serial_no = 45455454
channel = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[y]
serial_no = 45455454
channel = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[z]
serial_no = 45455454
channel = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[r]
serial_no = 45455454
channel = 0
# in deg
velocity = 8
acceleration = 8
start_position = 180

[run]
active_axes = ["x","y","z","r"]
stop_time_per_point_s = 3
readout_freq_s = 1e-3

# in mm
position_x = [10,30,10]
position_y = [10,30,10]
position_z = [10,20,10]
# in deg
position_r = [175,185,10]

save_config = true
```


## Usage

To start the Satellite, run

``` shell
SatelliteECTstage --group {group_name} --name {instance_name}
```

or

``` shell
SatelliteECTstage --help
```


## Main Satellite Functions

* `initialize(ConfigurationFile)`
  * Configure the Satellite and ThorLab stages
  * In order of execution:
    * load conf file and stages
    * verify if stage returned serial number and user given serial number of the stage match
    * set velocity and acceleration

* `launch()`:
  * moves to start position

* `start(run_id)`:
  * moves to start position
  * then loads `run(run_id)` which does the main stage movements while acquiring data

* `stop()`:
  * ends data run
  * stops stage movement immediately
  * Only works inside of start-run loop
    (NOTE: For emergency stop while outside run loop, use stage_stop())

* `reconfigure(ConfigurationFile)`
  * disconnects the stages and reinitializes them with values from the renewed config file
  * rest: follows initialize function and then relaunches the stages to start position

* `land()`:
  * closes the connection to the stage

## Custom Commands

In alphabetical order

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `blink` | Blink test stages                                                              | `axis` | - | `INIT`, `ORBIT`, `RUN` |
| `disable_axis` | Disable axis. Once disabled, the stage cannot be moved until re-enabled | `axis` | - | `INIT`, `ORBIT`, `RUN` |
| `disconnect` | Disconnects the communication via the serial port to the stages. If `axis==None`: applies to all stages | `axis=None` (optional) | - | `INIT` |
| `enable_axis` | Enables axis                                                             | `axis` | - | `INIT`, `ORBIT`, `RUN` |
| `get_full_status` | Returns stage full status. If `axis==None`: applies to all stages    | `axis=None` (optional) | Str: Full status of stages | `INIT`, `ORBIT`, `RUN` |
| `get_full_info` | Returns stage information including status, serial port communication information. If `axis==None`: applies to all stages    | `axis=None` (optional) | Str: Full info of stages | `INIT`, `ORBIT`, `RUN` |
| `get_position` | Get stage position. If `axis==None`: applies to all stages    | `axis=None` (optional) | Str: Full info of stages | `INIT`, `ORBIT`, `RUN` |
| `get_status` | Returns minimal stage status. If `axis==None`: applies to all stages    | `axis=None` (optional) | Str: Full info of stages | `INIT`, `ORBIT`, `RUN` |
| `get_vel_acc_params` | Get stage max, min velocities and acceleration. If `axis==None`: applies to all stages    | `axis=None` (optional) | Str:min velocity, max velocity and acceleration | `INIT`, `ORBIT`, `RUN` |
| `go_home` | Goes back to the stage defined home position. Ideally should be `0` mm for linear stages and `0` deg for rotational stage. If `axis==None`: applies to all stages    | `axis=None` (optional) | - | `INIT`, `ORBIT`, `RUN` |
| `stage_stop` | Stops stages. **Only works outside of main loop. For emergency stop while within run loop, `use stop()`.** If `axis==None`: applies to all stages    | `axis=None` (optional) | - | `INIT`, `ORBIT` |
