# Data

Data is sent via the [CDTP protocol](../../../protocols/cdtp.md), which send data in messages that can contain several
*data frames* (also called message payload frames). Since data is sent as binary data, it is up to the implementation to
decide how to organize the data. Additionally, each message can contain metadata in form of tags (key-value pairs) in the
message header .

## Sending Data

The {cpp:class}`DataSender <constellation::data::DataSender>` class can be used to send data during a run over the network.
To use this class in a satellite, an instance of it has to be added as a member variable to the satellite.
In the satellite's `initialize` function, {cpp:func}`DataSender::initializing() <constellation::data::DataSender::initializing()>`
needs to be called at the end to read the framework internal configuration parameters.
In the satellite's `starting` function, {cpp:func}`DataSender::starting() <constellation::data::DataSender::starting()>`
needs to be called at the end to send the Begin-of-Run (BOR) message, which includes the satellite's configuration.
In the satellite's `stopping` function, {cpp:func}`DataSender::stopping() <constellation::data::DataSender::stopping()>`
needs to be called at the end to send the End-of-Run (EOR) message, which includes run metadata.
If the satellite supports reconfiguration in ORBIT, {cpp:func}`DataSender::reconfiguring() <constellation::data::DataSender::reconfiguring()>`
needs to be called at the end of the satellite's `reconfiguring` function.

### Sending Data during the Run

A new data message can be created with {cpp:func}`DataSender::newDataMessage() <constellation::data::DataSender::newDataMessage()>`.
Data can be added to the message as new data frame with {cpp:func}`DataMessage::addDataFrame() <constellation::data::DataSender::DataMessage::addDataFrame()>`,
which requires requires creating a {cpp:class}`PayloadBuffer <constellation::message::PayloadBuffer>`.
For the most common C++ ranges like `std::vector` or `std::array`, moving the object into the payload buffer with `std::move()` is sufficient.
Optionally tags can be added to the data message for additional meta information using {cpp:func}`DataMessage::addTag() <constellation::data::DataSender::DataMessage::addTag()>`.
Finally the message can be send using {cpp:func}`DataSender::sendDataMessage() <constellation::data::DataSender::sendDataMessage()>`,
which returns if the message was sent (or added to the send queue) successfully. This return value has to be checked, since
a return value of `false` indicates that the message could not be sent due to a slow receiver. In this case, one can either
discard the message, try to send it again or throw an exception to abort the run.

```{warning}
Sending data is not thread safe. If multiple threads need to access the sender, it needs to be protected with a mutex.
```


### Sending the End of Run

Arbitrary metadata can be attached to the run, which will be send in the EOR message. This might include things like a
firmware version of a detector. To set this run metadata, a {cpp:class}`Dictionary <constellation::config::Dictionary>` has
to be created and set via {cpp:func}`DataSender::setRunMetadata() <constellation::data::DataSender::setRunMetadata()>`.
This has to be done before {cpp:func}`DataSender::stopping() <constellation::data::DataSender::stopping()>` is called.

### `DataSender` Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_data_bor_timeout` | Unsigned integer | Timeout for BOR to be received in seconds | `10` |
| `_data_eor_timeout` | Unsigned integer | Timeout for EOR to be received in seconds | `10` |

## Receiving Data (Single Sender)

The {cpp:class}`SingleDataReceiver <constellation::data::SingleDataReceiver>` class can be used to receive data from a single
satellite during a run over the network. To use this class in a satellite, an instance of it has to be added as a member
variable to the satellite.
In the satellite's `initialize` function, {cpp:func}`SingleDataReceiver::initializing() <constellation::data::SingleDataReceiver::initializing()>`
needs to be called at the end to read the framework internal configuration parameters.
In the satellite's `launching` function, {cpp:func}`SingleDataReceiver::launching() <constellation::data::SingleDataReceiver::launching()>`
needs to be called at the end to find the sending satellite via CHIRP.
In the satellite's `starting` function, {cpp:func}`SingleDataReceiver::starting() <constellation::data::SingleDataReceiver::starting()>`
needs to be called to receive the Begin-of-Run (BOR) message, which includes the sending satellite's configuration.
In the satellite's `stopping` function, {cpp:func}`SingleDataReceiver::stopping() <constellation::data::SingleDataReceiver::stopping()>`
needs to be called at the beginning to prepare for receiving the End-of-Run (EOR) message.
If the satellite supports reconfiguration in ORBIT, {cpp:func}`SingleDataReceiver::reconfiguring() <constellation::data::SingleDataReceiver::reconfiguring()>`
needs to be called at the end of the satellite's `reconfiguring` function.

### Receiving Data during the Run

To receive data, use {cpp:func}`SingleDataReceiver::recvData() <constellation::data::SingleDataReceiver::recvData()>`. If a
message was received before the DATA timeout is reached, the message is returned, otherwise an empty optional.
The returned message is a {cpp:class}`CDTP1Message <constellation::message::CDTP1Message>`.
The metadata sent with message and the sequence number of the message is stored in the header, which can be retrieved from
the message via {cpp:func}`CDTP1Message::getHeader() <constellation::message::CDTP1Message::getHeader()>`.
The binary payload of the message get be retrieved via {cpp:func}`CDTP1Message::getPayload() <constellation::message::CDTP1Message::getPayload()>`,
which returns a `std::vector` of {cpp:class}`PayloadBuffer <constellation::message::PayloadBuffer>`. A `std::span` to the
binary data can be retrieved using {cpp:func}`PayloadBuffer::span() <constellation::message::PayloadBuffer::span()>`.

```{warning}
Receiving data is not thread safe. If multiple threads need to access the sender, it needs to be protected with a mutex.
```

### Receiving the End of Run

In the `stopping` function of the satellite, additional data might be received if the sending satellite has not transitioned
to `stopping` yet. This means that {cpp:func}`SingleDataReceiver::recvData() <constellation::data::SingleDataReceiver::recvData()>`
needs to be called until {cpp:func}`SingleDataReceiver::gotEOR() <constellation::data::SingleDataReceiver::gotEOR()>` returns
`true`. Then {cpp:func}`SingleDataReceiver::getEOR() <constellation::data::SingleDataReceiver::getEOR()>` has to be called to
retrieve the sending satellite's run metadata from the EOR message.

In summary, this might look like this example code:

```c++
// Prepare to receive EOR
data_receiver_.stopping()
// Continue to receive data until EOR is received
while(!data_receiver_.gotEOR()) {
    const auto message_opt = data_receiver_.recvData();
    if(message_opt.has_value()) {
        write_message_to_file(message_opt.value());
    }
}
// Get run metadata from EOR
const auto run_metadata = data_receiver.getEOR();
write_run_meta_to_file(run_meta);
```

### `SingleDataReceiver` Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_data_sender_name` | String | Canonical name of the sending satellite | - |
| `_data_chirp_timeout` | Unsigned integer | Timeout for sending satellite to be found in seconds | `10` |
| `_data_bor_timeout` | Unsigned integer | Timeout for BOR to be received in seconds | `10` |
| `_data_data_timeout` | Unsigned integer | Timeout for DATA to be received in seconds | `1` |
| `_data_eor_timeout` | Unsigned integer | Timeout for EOR to be received in seconds | `10` |

## Receiving Data (Multiple Senders)

Not possible yet, use a satellite for each sender.

## `constellation::data` Namespace

```{doxygennamespace} constellation::data
:members:
:protected-members:
:undoc-members:
```
