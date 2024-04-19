# Constellation Monitoring Distribution Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Monitoring Distribution Protocol (CMDP) defines how hosts distribute and receive auxiliary information such as log messages and statistics data for monitoring purposes.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behaviour of the monitoring message distribution between hosts of the Constellation framework.
This protocol specifies how CMDP hosts are

* sending and receiving multicast messages with auxiliary status information to and from other CMDP hosts,
* formatting message topics to allow identification of the message type as well as filtering, and
* subscribing to specific message topics to reduce the number of transmitted messages and consequently the network load.

This protocol defines two message types which differ in purpose and message syntax:

* Log messages are intended for the distribution of informational logging messages from a CMDP host
* Metrics data messages are intended for the distribution of machine-readable monitoring data from a CMDP host.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behaviour will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB) defines the semantics of PUB, XPUB, SUB and XSUB sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

A CMDP message MUST consist of three frames and SHALL be sent as multipart message.
The definitions of ‘frame’ and ‘multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

The message SHALL consist of the following frames, in this order:

* The message topic
* The message header
* The message payload

### Overall Behavior

A CMDP sending host SHALL advertise its CMDP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md).

Upon service discovery through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md), a CMDP host MAY subscribe to one or multiple topics as defined by [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB).

The first frame of the message SHALL consist of the message topic in order to allow outgoing message filtering as specified in [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB).
The sending CMDP host SHALL only send a message if the receiving CMDP host has a subscription to the topic of the message and SHALL discard all other messages.

The topic MUST start either with prefix `LOG` or with prefix `STAT` and SHOULD only contain upper case letters, slashes or digits.

A receiving CMDP host SHALL discard messages that it receives with an invalid topic or topic formatting.

In case of network congestion, unsent messaged SHALL be discarded by the sending CMDP host.

### Log Message Topic

The message topic frame SHALL start with topic prefix `LOG`, hereafter referred to as ‘log messages’, if it distributes informational text from the program flow of the sending CMDP host.
The `LOG` topic prefix MUST be followed by a trailing slash `/`.
This is hereafter referred to as ‘heading’.

The heading SHALL be followed by a log level.
The log level SHOULD be any of `CRITICAL`, `STATUS`, `WARNING`, `INFO`, `DEBUG` or `TRACE`.

The following lists provides guidance on the usage of these topics:

* The topic `LOG/TRACE` SHOULD be used for verbose information which allows to follow the call stack of the host program.
* The topic `LOG/DEBUG` SHOULD be used for information relevant to developers for debugging the host program.
* The topic `LOG/INFO` SHOULD contain information on regular events intended for end users of the host program.
* The topic `LOG/WARNING` SHOULD be used to notify the end user of the host program of unexpected events which require further investigation.
* The topic `LOG/STATUS` SHOULD be used to communicate important information about the host program to the end user with low frequency.
* The topic `LOG/CRITICAL` SHOULD be used to notify the end user about critical events which require immediate attention and MAY have triggered an automated response by the host program or other hosts.

The log message topic MAY be followed by a trailing slash `/` and a component identifier of the host program to allow further filtering of messages.
An example for a valid log message topic with component identifier is `LOG/TRACE/NETWORKING`.

### Metrics Data Topic

The message topic frame SHALL start with topic prefix `STAT` if it distributes metrics data messages from the sending host for the purpose of monitoring.
The `STAT` topic prefix MUST be followed by a trailing slash `/`.
This is hereafter referred to as ‘heading’.

The heading SHALL be followed by a metrics name which SHOULD be unique per sending CMDP host.
It is RECOMMENDED to use the same unique metrics name across different hosts for the same metrics data quantity.

An example for a valid metrics data message topic is `STAT/CPULOAD`.

### Message Header

The message header frame has the same format for metrics data and log messages and MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
It SHALL contain two strings, followed by a 64-bit timestamp and a map.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘M’, ‘D’ and ‘P’, followed by the protocol version number, which SHALL be `%x01`.

The second string SHOULD contain the name of the sending CMDP host.

The timestamp SHALL follow the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification for timestamps and contain a 64-bit UNIX epoch timestamp in units of nanoseconds.
Possible values MAY be the time of sending the message or the time of generation of the payload at the sending CMDP host.

The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

If the log level is `TRACE`, the map SHOULD contain key-value pairs with keys `thread`, `filename`, `lineno`, and `funcname` containing the values for the thread ID as integer, the filename as string, the line number as integer and the function name as string of the source code location of the CMDP sending host emitting the log message.

### Log Message Payload

The log message payload frame SHALL consist of the log message encoded as a UTF8 string.

### Metrics Data Payload

The metrics data payload frame MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
It SHALL contain, in this order

* The metrics value, which MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
* The metric type
* The unit of the metrics, as string

The metrics type SHALL be encoded as 1-OCTET integer variable with the following possible values:

* `0x1` - LAST_VALUE: Updating the value of this metrics replaces the previous value, only the last value SHOULD be displayed.
* `0x2` - ACCUMULATE: Every new value of this metrics SHOULD be added to the cached value.
* `0x3` - AVERAGE: The average value of the metrics SHOULD be calculated over a given time interval.
* `0x4` - RATE: The rate of the metrics SHOULD be calculate over a given time interval.

The metrics type MAY be implemented as enum type if appropriate.
