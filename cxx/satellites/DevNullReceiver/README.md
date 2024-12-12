---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "DevNullReceiver"
description: "A satellite that receives data and drops it"
category: "Developer Tools"
---

## Building

The DevNullReceiver satellite has no additional dependencies.
The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_dev_null_receiver=true
```

## Parameters

None
