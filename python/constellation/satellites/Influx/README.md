---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Influx"
description: "Satellite writing metrics to InfluxDB"
category: "Monitoring"
---

## Description

This satellite listens to metrics send by other satellites and writes to a InfluxDB time series database.
Only metrics of type float, integer and boolean can be written to InfluxDB.

```{warning}
Currently, the satellites subscribes to all metrics. This can lead to performance penalties if there are debugging metrics
which are evaluated in a hot loop. Excluding and including metrics is a feature that will be added in the future.
```

## Requirements

The Influx satellite requires the `[influx]` component, which can be installed with:

::::{tab-set}
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[influx]
```

:::
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[influx]
```

:::
::::

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `url` | InfluxDB URL | String | `http://localhost:8086` |
| `token` | Access token | String | - |
| `org` | Organization | String | - |
| `bucket` | Measurement bucket | String | `constellation` |
