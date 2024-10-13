---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Katherine"
subtitle: "Satellite to control the Timepix3 readout system Katherine"
---

## Description

This satellite controls the [Katherine readout system](https://iopscience.iop.org/article/10.1088/1748-0221/12/11/C11001)
for Timepix3 detectors. The readout system is connected to the controlling computer via Gigabit ethernet and consequently
supports maximum hit rates of up to 16MHits/s.

The satellite allows to operate the Timepix3 detector in any of its matrix operation modes. i.e. the combined time-of-arrival
and time-over-threshold mode (`TOA_TOT`), the time-of-arrival mode (`TOA`) or the counting mode with integrated TOT
(`EVT_ITOT`). The detector can be operated either in data-driven mode (`sequential_mode = false`) or frame-based mode. In the
latter case, the number of frames to acquire has to be configured via `no_frames`.

## Building

The Katherine satellite requires the [`libkatherine`](https://github.com/petrmanek/libkatherine) library. For Meson versions
starting with `1.3.0` it can be installed automatically, otherwise it can be installed via

```sh
git clone https://github.com/petrmanek/libkatherine.git
cd libkatherine
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/opt # Adjust install path if wanted
ninja install
```

The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dpkg_config_path="/opt/share/pkgconfig" -Dsatellite_katherine=true
```

## Chip Settings

The chip settings are read from two separate files which need to be provided, the DACS file and the pixel configuration file:

### DAC Settings

The DAC file should contain two columns with the DAC number and its assigned value in decimal notation:

```text
<dac_nr> <dac_value>
```

The following DACs are available:

```text
dac_nr   default   name                   valid range
 1          128    TPX3_IBIAS_PREAMP_ON       [0-255]
 2            8    TPX3_IBIAS_PREAMP_OFF       [0-15]
 3          128    TPX3_VPREAMP_NCAS          [0-255]
 4          128    TPX3_IBIAS_IKRUM           [0-255]
 5          128    TPX3_VFBK                  [0-255]
 6          256    TPX3_VTHRESH_FINE          [0-512]
 7            8    TPX3_VTHRESH_COARSE         [0-15]
 8          128    TPX3_IBIAS_DISCS1_ON       [0-255]
 9            8    TPX3_IBIAS_DISCS1_OFF       [0-15]
10          128    TPX3_IBIAS_DISCS2_ON       [0-255]
11            8    TPX3_IBIAS_DISCS2_OFF       [0-15]
12          128    TPX3_IBIAS_PIXELDAC        [0-255]
13          128    TPX3_IBIAS_TPBUFIN         [0-255]
14          128    TPX3_IBIAS_TPBUFOUT        [0-255]
15          128    TPX3_VTP_COARSE            [0-255]
16          256    TPX3_VTP_FINE              [0-512]
17          128    TPX3_IBIAS_CP_PLL          [0-255]
18          128    TPX3_PLL_VCNTRL            [0-255]
```

The fine and coarse threshold values can be converted to a linearized threshold value via:

```text
threshold = coarse * 160 + (fine - 352)
# with: 352 <= fine < 511

coarse = floor(threshold / 160)
fine = (threshold % 160) + 352
```

### Pixel Configuration / Trim DACs

The individual per-pixel trimming DAC settings are read from a file with the following layout:

```text
<column> <row> <trim> <mask> <testpulse>
```

Where `<trim>` is a 4-bit trim DAC value, `<mask>` is a Boolean for masking the pixel, and `<testpulse>` a Boolean to enable
sending test pulses to this pixel.

## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required unless
noted otherwise.

| Parameter     | Description | Type | Default Value |
|---------------|-------------|------|---------------|
| `ip_address`        | IP address of the Katherine system | String | |
| `chip_id`           | ID of the chip to be operated. Optional setting, this will only be checked when provided. If it does not match, an error is reported. | String | |
| `dacs_file`         | Path to file with DAC settings     | String | |
| `px_config_file`    | Path to file with individual pixel trim settings | String | |
| `positive_polarity` | Threshold polarity switch | Bool | `true`    |
| `sequential_mode`   | Switch to enable/disable sequential mode | Bool | `false`   |
| `op_mode`           | Operation mode, can be `TOA_TOT`, `TOA`, `EVT_ITOT` or `MASK` | | `TOA_TOT` |
| `op_mode`           | Operation mode, can be `TOA_TOT`, `TOA`, `EVT_ITOT` or `MASK` | String | `TOA_TOT` |
| `shutter_mode`      | Shutter operation mode, can be `POS_EXT`, `NEG_EXT`, `POS_EXT_TIMER`, `NEG_EXT_TIMER` or `AUTO` | String | `AUTO` |
| `shutter_width`     | Width of the shutter window, only relevant for shutter modes `POS_EXT_TIMER` and `NEG_EXT_TIMER` | Int | |
| `pixel_buffer`      | Depth of the pixel buffer. This many pixel hits are accumulate before sending. The default corresponds to one full Timepix3 frame | Int  | 65536     |
| `data_buffer`       | Depth of the data buffer in units of Timepix3 packets (6 bytes each). Data is either decoded or sent directly from this buffer. The default corresponds to 200 MB memory. | Int  | 34952533 |
| `no_frames`         | Number of frames to acquire. Needs to be set to 1 for data-driven mode (`sequential_mode = false`) | Int | 1 |

### Configuration Example

The following example configuration could be used to control a Katherine readout system satellite with a Timepix3 detector.
The satellite is configured to connect to the system at `192.168.1.182` and to use the provided local files for DAC settings
and pixel matrix configuration. The pixel buffer is set to 300 pixel hits, all other settings are left at their defaults:

```ini
[Satellites.Katherine.TPX3]
ip_address = "192.168.1.182"
dacs_file = "E2-W0005_dacs.txt"
px_config_file = "E2-W0005_trimdacs.txt"
pixel_buffer = 300
```

## Custom Commands

The following custom commands are exposed to the command interface by this satellite:

| Command     | Description | Arguments | Return Value | Allowed States |
|-------------|-------------|-----------|--------------|----------------|
| `get_temperature_readout` | Read the current temperature from the Katherine readout board | - | Temperature in degree Celsius, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_temperature_sensor` | Read the current temperature from the temperature sensor | - | Temperature in degree Celsius, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_adc_voltage` | Read the voltage from the ADC channel provided as parameter | channel number, `int` | channel reading, `double` | `INIT`, `ORBIT`, `RUN` |
| `get_chip_id` | Read the chip ID of the attached sensor | - | Chip ID, `string` | `INIT`, `ORBIT`, `RUN` |
| `get_hw_info` | Read hardware revision and other information from the device | - | List of strings with HW type, revision, serial number and firmware version | `INIT`, `ORBIT`, `RUN` |
| `get_link_status` | Read chip communication link status from the device | - | List of strings with line mask, data rate and chip presence flag | `INIT`, `ORBIT`, `RUN` |

## Miscellaneous Information

### Timepix3 Packet Headers and Header Filters

Packets coming from the chip have a header identifying the type of information contained.
The following table relates them:

```text
packet               header           filter bit
AnalogPeriphery      0000 (0x0)       0
OutputBlockConfig    0001 (0x1)       1
PLLConfig            0010 (0x2)       2
GeneralConfig        0011 (0x3)       3
Timer                0100 (0x4)       4
                     0101 (0x5)       5
Trigger (SPIDR)      0110 (0x6)       6
ControlOperation     0111 (0x7)       7            <- Marks e.g. EndOfCommand
LoadConfigMatrix     1000 (0x8)       8
ReadConfigMatrix     1001 (0x9)       9
ReadMatrixSequential 1010 (0xA)       10           <- Pixel data
ReadMatrixDataDriven 1011 (0xB)       11           <- Pixel data
LoadCTPR             1100 (0xC)       12
ReadCTPR             1101 (0xD)       13
ResetSequential      1110 (0xE)       14
StopMatrixReadout    1111 (0xF)       15
```
