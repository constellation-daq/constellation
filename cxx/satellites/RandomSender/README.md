---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "RandomSender"
subtitle: "A satellite that sends random data"
---

## Building

The RandomSender satellite has no additional dependencies.
It is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_random_sender=enabled
```

## Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `seed` | Unsigned 8-bit integer | Seed for the random engine | Random |
| `frame_size` | Unsigned integer | Size of a data frame in bytes | `1024` |
| `number_of_frames` | Unsigned integer | Number of data frames per data message | `1` |

### Framework Parameters

Inherited from `DataSender`. (FIXME link to docs)
