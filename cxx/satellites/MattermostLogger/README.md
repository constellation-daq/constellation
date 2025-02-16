---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "MattermostLogger"
description: "TODO"
category: "Monitoring"
---

## Description

TODO

## Building

The MattermostLogger requires [`cpr`](https://github.com/libcpr/cpr), which is downloaded on demand.
The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_mattermost_logger=true
```

## Parameters

| Parameter  | Description | Type | Default Value |
|------------|-------------|------|---------------|
| `webhook_url` | TODO | string | - |
| `log_level` | TODO | string | `WARNING` |
