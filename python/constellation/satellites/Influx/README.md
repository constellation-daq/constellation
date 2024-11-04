---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "InfluxDB"
subtitle: "Satellite writing metrics to InfluxDB"
---

## Description

This satellite listens to metrics send by other satellites and writes to a InfluxDB time series database.
Only metrics of type float, integer and boolean can be written to InfluxDB.

```{warning}
Currently, the satellites subscribes to all metrics. This can lead to performance penalties if there are debugging metrics
which are evaluated in a hot loop. Excluding and including metrics is a feature that will be added in the future.
```

## Requirements

The Influx satellite requires the `[influxdb]` component, which can be installed e.g. via:

```sh
pip install --no-build-isolation -e .[influxdb]
```

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `url` | InfluxDB URL | String | `http://localhost:8086` |
| `token` | Access token | String | - |
| `org` | Organization | String | - |
| `bucket` | Measurement bucket | String | `constellation` |
