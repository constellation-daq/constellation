# Receiving Data in C++

This how-to guide describes the concepts of receiving data from other satellites.
It is recommended to read through the  [implementation how-to](satellite_cxx.md) of satellites in C++ in
order to get a solid understanding of the state machine and data transmission mechanism.

## Receiving Data

The {cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>` base class provides functions to receive
data. To use it the inheritance can simply be changed from {cpp:class}`Satellite <constellation::satellite::Satellite>`.

To receive data, the {cpp:func}`receive_bor() <constellation::satellite::ReceiverSatellite::receive_bor()>`,
{cpp:func}`receive_data() <constellation::satellite::ReceiverSatellite::receive_data()>` and
{cpp:func}`receive_eor() <constellation::satellite::ReceiverSatellite::receive_eor()>` methods have to be implemented.
A {cpp:func}`running() <constellation::satellite::Satellite::running()>` method must not be implemented. Files on disk
should be opened in the {cpp:func}`starting() <constellation::satellite::Satellite::starting()>` method and closed in the
{cpp:func}`stopping() <constellation::satellite::Satellite::stopping()>` method.

While receiving data, the receiver should also store the tags in the of the messages header. They can be retrieved via
`data_message.getHeader().getTags()`. Data always arrives sequentially, so file locking is not required.
