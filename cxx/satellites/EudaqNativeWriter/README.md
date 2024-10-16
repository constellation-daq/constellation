---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "EudaqNativeWriter"
subtitle: "A satellite that receives data and writes it to EUDAQ2 native binary formatted files"
---

This satellite receives data from other satellites and encodes them in the EUDAQ2 native binary file format.
The satellite can be used for compatibility when using analysis software that interfaces EUDAQ2 for decoding data.

Since Constellation and EUDAQ2 data transmission formats are slightly different, there are two BOR tags which *sending*
satellites should set in order to ensure the data can be correctly decoded:

* The sending satellite should set the `eudaq_event` BOR tag to a string describing the EUDAQ2 event type to be used for
  encoding, for example `TluRawDataEvent` for data sent by the AIDA TLU. If this tag is not set, the satellite falls back
  to using the *name* portion of the canonical name of the sender. This can be used to improve interoperability of
  satellites which do not set these tags explicitly.
* EUDAQ2 knows data blocks per event as well as sub-events, while Constellation data messages always consist of a header
  followed by any number of frames. In order to encode this data correctly to EUDAQ2 native binary format, the sending
  satellite should specify if the Constellation data frames should be interpreted as as individual *data blocks* or as
  *sub-events* in the resulting EUDAQ2 native binary event by setting the BOR tag `frames_as_blocks` to either `true` or
  `false`, respectively. If the tag is not provided, this satellite defaults to interpreting them as sub-events, repeating
  the message header for all of the attached frames.

```{note}
It should be noted that this satellite requires the sending satellites to receive data from to be configured via the
`_data_transmitters` parameter, just as any Constellation receiver satellite deriving from the
[`ReceiverSatellite`](../reference/cxx/satellite/satellite.md#receiversatellite-configuration-parameters)
class.
```

Messages from all sending satellites are written into the output file sequentially in the order in which they arrive.
Output files are stored under the path provided via the `output_path` parameter and are named `data_<run_identifier>.raw`
where `<run_identifier>` is the identifier of the corresponding run.

## Building

This satellite does not have any external dependencies and is therefore built by default. Should this not be desired can the
build be deactivated via

```sh
meson configure build -Dsatellite_eudaq_native_writer=false
```

## Parameters

The following parameters are read and interpreted by this satellite in addition to the framework parameters of the
[`ReceiverSatellite`](../reference/cxx/satellite/satellite.md#receiversatellite-configuration-parameters) class.

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `allow_overwriting` | Bool | Switch whether overwriting files is allowed or not. If set to `false` and a file exists already, this satellite will go into `ERROR` state. | `false` |
| `output_path` | String | Base path to which to write output files to. | - |
