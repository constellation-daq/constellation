# Communication Protocols

Constellation is built around a set of communication protocols among its constituents. These protocols are well-defined and have been defined
early on and serve as platform- and implementation-independent architecture of the framework, meaning that new implementations of e.g. a satellite can be written in any language.
The communication channels are independent of each other and follow clear communication patterns such as publish/subscribe for one-to-many distribution of information
or request/reply for a client-server-based communication.

Most of the protocols are TCP/IP communication based on the [ZeroMQ messaging library](https://zeromq.org/) and build upon the ZeroMQ Message Transport Protocol.
The protocols are documented in RFC-style documents, including an [ABNF description](https://en.wikipedia.org/wiki/Augmented_Backus%E2%80%93Naur_form)
where relevant, and can be found in the appendix of this manual.

The five Constellation communication protocols are described in the following, ordered by significance of the information passed.

## Heartbeating

## Command & Controlling

Commands from controller instances to satellites are transmitted via the Constellation Satellite Control Protocol (CSCP). It
resembles a client-server architecture with the typical request-reply pattern. Here, the satellite acts as the server while
the controller assumes the role of the client.

## Data Transmission

Data are transferred within a Constellation network using the Constellation Data Transmission Protocol (CDTP). It uses point-to-point
connections via TCP/IP, which allow the bandwidth of the network connection to be utilized as efficiently as possible.
The message format transmitted via CDTP is a lightweight combination of a header frame with metadata, a message sequence
number and optional tags, as well as payload frames which differ depending on the message type. CDTP knows three different
message types, indicated by a flag in the header frame:

* `BOR` - Begin of Run: This message is sent automatically at the start of a new measurement, i.e. upon entering the `RUN`
  state of the finite state machine of the sending satellite. It marks the start of a measurement in time and its payload
  frame contains the entire satellite configuration as well as other metadata such as the current run ID. The message
  sequence ID of the `BOR` message is always 0, representing the first message of a new run.
* `DATA`: This is the standard message type of CDTP which consists of the header frame and any number of payload frames
  containing undecoded raw data of the respective instrument. The message sequence counts up with every message, providing
  additional possibility for checking data integrity offline.
* `EOR` - End of Run: This message is sent automatically at the end of a measurement, i.e. upon leaving of the `RUN` state
  by the sending satellite.

Only `DATA` messages can be transmitted by satellite implementations, both `BOR` and `EOR` messages are handled automatically.

## Monitoring

The distribution of log messages and performance metrics within Constellation is handled by the Constellation Monitoring Distribution Protocol (CMDP).
The protocol is built around publisher and subscriber sockets which allow one-to-many distribution of messages. Subscriptions to logging levels
or metrics are completely independent of the current FSM state and can be performed at any time. This means that hosts listening and displaying
e.g. log messages can be ended and restarted and the subscriptions can be changed while the Constellation is running undisturbed.
The protocol features message filtering for data efficiency and minimal bandwidth usage. This means that a host only sends messages over the
protocol for which a subscription is present.

The same protocol is used for log messages and performance metrics. The following log levels are defined:

* `TRACE` messages are be used for very verbose information which allows to follow the program flow for development purposes. This concerns, for example, low-level code for network communication or internal states of the finite state machine. The messages of this level also contain additional information about the code location of the program where the message has been logged from.
* `DEBUG` messages contain information mostly relevant to developers for debugging the program.
* `INFO` messages are of interest to end users and should contain information on the program flow of the component from a functional perspective. This comprises, e.g. reports on the progress of configuring devices.
* `WARNING` messages indicate unexpected events which require further investigation by the user.
* `STATUS` messages are used communicate important information on a low frequency such as successful state transitions.
* `CRITICAL` messages notify the end user about critical events which require immediate attention. These events may also have triggered an automated response and state change by the sending host.

The CMDP protocol support subtopics, which are appended to the log level. This allows to select only the relevant slice of information from an otherwise very verbose
log level and therefore reduce the network bandwidth required. An example would be selecting only the `TRACE` messages relevant for network communication by
subscribing to the topic `TRACE/NETWORKING`.

Apart from log messages, CMDP is also used to transmit performance metrics such as the current trigger rate, the number of recorded events, temperature or CPU loads.
These messages are published under their respective topics and subscribers can choose the variables they want to follow.

## Network Discovery

A common nuisance in volatile networking environments with devices appearing and disappearing is the discovery of available devices and services.
While some established protocols exist for the purpose of finding services on a local network, such as zeroconf or avahi, these come with significant
downsides such as missing standard implementations, being limited to individual platforms, or a large and complex set of features not required for the
purpose of Constellation.

Hence, the Constellation Host Identification & Reconnaissance Protocol (CHIRP) has been devised. It is a IPv4 protocol intended to be used on local
networks only which uses a set of defined beacons sent as broadcasts over UDP/IP to announce or request services.
The beacon message contains a unique identified for the host and its Constellation group, the relevant service as well as IP address and port of the service.
Three such beacons exist:

* `OFFER`: A beacon of this type indicates that the sending host is offering the service at the provided endpoint.
* `REQUEST`: This beacon solicits offers of the respective service from other hosts.
* `DEPART`: A departing beacon is sent when a host ceases to offer the respective service.

Each service the participating Constellation host offers is registered with its CHIRP service. Upon startup of the program, a `OFFER` beacon is sent
for each of the registered services.
The `REQUEST` beacon allows hosts to join late, i.e. after the initial `OFFER` beacons have been distributed. This means that at any time of the
framework operation, new hosts can join and request information on a particular service from the already running Constellation participants.
A clean shutdown of services is possible with the `DEPART` beacon which will prompt other hosts to disconnect.
