---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "RawFileWriter"
subtitle: "A satellite writing incoming data to a raw file"
---

```{warning}
This satellite is for testing and benchmarking purposes only. The file format does not include all metadata and might change
at any time. Please use (FIXME) instead if data needs to be used.
```

## Building

The RawFileWriter satellite has no additional dependencies.
It is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_raw_file_writer=enabled
```

## File Format

The data receiver all writes first the size of the object in bytes as an unsigned 32-bit integer, and then the object itself.
The first object is the satellite config from the BOR messages as MessagePack dictionary. Then the data messages get written,
however only the payload meaning that any metadata in the CDTP header is dropped. The last object written is the run metadata
from the EOR message as MessagePack dictionary.

## Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `output_directory` | String | Output directory where to store run data | Current path |

### Framework Parameters

Inherited from `SingleDataReceiver`. (FIXME link to docs)
