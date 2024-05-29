---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "PI Motor Stage Controller"
description: "Satellite steering a PI Motion Stages Controller"
---

## Description


## Parameters

The following parameters need to be specified in the configuration file:

* `controller_name`: PI Stage Controller to be used, e.g. `C-884`
* `controller_ip`: IP Address of the controller, e.g. `192.168.22.123`
* `stages`: List of attached stages. The order represents the channels of the controller. Example: `['M-111.1DG', 'M-122.2DD', 'NOSTAGE', 'NOSTAGE']`
* `refmodes`: List of referencing modes for the stages, e.g. `['FNL', 'FRF']`


## Usage

A minimal configuration would be:

```ini
[satellites.PIStages.dut]
```
