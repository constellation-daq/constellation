<!-- markdownlint-disable MD041 -->
### Parameters inherited from `TransmitterSatellite`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_bor_timeout` | Unsigned integer | Timeout in seconds to send the BOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. A possible reason for failure is that no receiver satellite connected to this satellite and is receiving data. | 10 |
| `_eor_timeout` | Unsigned integer |  Timeout in seconds to send the EOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
| `_data_timeout` | Unsigned integer | Timeout in seconds to send the data message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
