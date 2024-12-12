# Satellite

## Data

Data is sent via the [CDTP protocol](../../../protocols/cdtp.md), which send data in messages that can contain several
*data frames* (also called message payload frames). Since data is sent as binary data, it is up to the implementation to
decide how to organize the data. Additionally, each message can contain metadata in form of tags (key-value pairs) in the
message header.

### Transmitting Data

The {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` class can be used to send data during
a run over the network. It sends the Begin-of-Run (BOR) message in the `starting` state and the End-of-Run (EOR) message in
the `stopping` state. This is achieved in the {cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by
dynamically casting it to a {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>`.

```{warning}
Sending data is not thread safe. If multiple threads need to access the sender, it needs to be protected with a mutex.
```

#### `TransmitterSatellite` configuration parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_bor_timeout` | Unsigned integer | Timeout for the BOR message to be successfully sent, in seconds | `10` |
| `_eor_timeout` | Unsigned integer | Timeout for the EOR message to be successfully sent, in seconds | `10` |
| `_data_timeout` | Unsigned integer | Timeout for a data message to be successfully sent, in seconds | `10` |

### Receiving Data

The {cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>` class can be used to send data during
a run over the network. It requires a given list of data transmitters, which is given by the `_data_transmitters` config
parameter. Similar to the {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` mentioned
above, executing transmitter transitions in addition to user transitions is achieve in the
{cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by dynamically casting it to a
{cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>`.

#### `ReceiverSatellite` configuration parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_eor_timeout` | Unsigned integer | Timeout for the EOR message to be received in seconds | `10` |
| `_data_transmitters` | List of strings | Canonical names of transmitters to connect to | - |

## `constellation::satellite` Namespace

```{doxygennamespace} constellation::satellite
:content-only:
:members:
:protected-members:
:undoc-members:
```
