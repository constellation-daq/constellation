# Constellation Data Transmission Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Data Transmission Protocol (CDTP) defines how data is transmitted from one sending host to one receiving host.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behaviour of the data message transmission between two hosts of the Constellation framework.
This protocol specifies how CDTP hosts are sending and receiving messages with cargo payload to and from other CDTP hosts.

This protocol defines three data message types which differ in purpose and message syntax:

* Begin-of-run messages indicate the start of a measurement undertaken by the sending CDTP host.
* Data messages contain sequential data recorded by the sending CDTP host.
* End-of-run messages mark the end of a measurement by the sending CMDP host.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behaviour will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE) defines the semantics of PUSH and PULL sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

A CDTP message SHALL be sent as multipart message and MUST consist of at least one frame and MAY consist of any larger number of frames.
The definitions of ‘frame’ and ‘multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

The first frame of the message frame MUST contain the message header.
OPTIONAL frames with message payload MAY be sent afterwards.

### Overall Behavior

A CDTP sender host SHALL advertise its CDTP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md).

Upon service discovery through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md), a CDTP receiver host SHOULD connect its PULL socket to the PUSH socket of one CDTP sender host as defined by [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE).

A CDTP receiver host SHALL notify the user about messages that it receives with an invalid header.

In case of network congestion, unsent messaged SHALL be buffered by the sending CDTP host and sent at a later time.
Upon reaching the high-water mark of buffered messages, the user MUST be notified and further sending of messages SHALL be blocked until action has been taken.

Any data message must be enclosed by the begin-of-run message and end-of-run message of the current measurement.
If a CDTP host receives a data message before the begin-of-run message, or if it receives a data message after the end-of-run message, the user MUST be notified and further reception of messages SHALL be blocked until action has been taken.

### Message Header

The message header frame MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
It SHALL contain two strings, followed by a 64-bit timestamp, a one-byte message type identifier, a 64-bit integer and a map.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘D’, ‘T’ and ‘P’, followed by the protocol version number, which SHALL be `%x01`.

The second string SHOULD contain the name of the sending CDTP host.

The timestamp SHALL follow the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification for timestamps and contain a 64-bit UNIX epoch timestamp in units of nanoseconds.
Possible values MAY be the time of sending the message or the time of generation of the payload at the sending CDTP host.

The message type identifier SHALL be either `%x00` (dubbed ‘DAT‘ for data), `%x01` (dubbed ‘BOR’ for begin-of-run), or `%x02` (dubbed ‘EOR’ for end-of-run).

The 64-bit integer SHALL contain the message sequence number of the sender, i.e. a monotonically incremented number that represents the number of messages sent since the beginning of the measurement.

The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

### Message Payload

For BOR type messages, a single payload frame MUST be attached containing a [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) encoded map containing the configuration of the CDTP sender host.
The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

For DATA type messages, any number of payload frames MAY be attached consisting of any binary data.
The interpretation and decoding of this data is not part of this protocol and left for user code implementations.

For EOR type messages, a single payload frame MUST be attached containing a [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) encoded map containing additional meta information of the measurement run.
The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
