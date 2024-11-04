---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Mariner"
subtitle: "Demonstrator satellite serving as prototype for new satellites"
---

## Description

This satellite does very little, it mostly serves as demonstrator for the different functionalities of satellites.
New Python satellites for Constellation may be created by copying and modifying the Mariner satellite.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `voltage` | Voltage value for the canopus star tracker | Floating Point Number | - |
| `current` | Current value for the canopus star tracker | Floating Point Number | - |
| `sample_period` | Time between executions of the voltage sampling/print-out | Floating Point Number | - |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `BRIGHTNESS` | Brightness | Integer | `LAST_VALUE` | 10s |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_attitude` | Get roll attidue | - | Integer | `INIT`, `ORBIT`, `RUN` |
