# Implementing a new Satellite in C++

This how-to guide will walk through the implementation of a new satellite, written in C++, step by step. The entire
procedure should not take too long, but this of course depends on the complexity of the satellite functionality.
It is recommended to have a peek into the overall [concept of satellites](/manual/concepts/satellite) in Constellation in
order to get an impression of which functionality of the application could fit into which state of the finite state machine.

```{note}
This how-to describes the procedure of implementing a new satellite for Constellation in C++. For Python look [here](/manual/howtos/satellite_py) and
for the microcontroller implementation, please refer to the [MicroSat project](https://gitlab.desy.de/constellation/microsat/).
```

## Sending or Receiving Data

The first decision that needs to be taken is whether the satellite will produce and transmit data, or if it will receive and
process data from other satellites.

## Implementing the FSM Transitions

In Constellation, actions such as device configuration and initialization are realized through so-called transitional states
which are entered by a command and exited as soon as their action is complete. A more detailed description on this can be found
in the [satellite section](/manual/concepts/satellite) of the framework concepts overview. The actions attached to these
transitional states are implemented by overriding the virtual methods provided by the `Satellite` base class.

For a new satellite, the following transitional state actions **should be implemented**:

* `void NewSatellite::initializing()`
* `void NewSatellite::launching()`
* `void NewSatellite::landing()`
* `void NewSatellite::starting()`
* `void NewSatellite::stopping()`

The following transitional state actions are optional:

* `void NewSatellite::reconfiguring()`: implements a fast partial reconfiguration of the satellite, see below for a detailed description.
* `void NewSatellite::interrupting()`: this is the transition to the `SAFE` state and defaults to `stopping` (if necessary because current state is `RUN`), followed by `landing`. If desired, this can be overwritten with a custom action.

For the steady state action for the `RUN` state, see below.

## Running and the Stop Token

The satellite's `RUN` state is governed by the `running` action, which - just as the transitional state actions above - is overridden from the `Satellite` base class.
The function will be called upon entering the `RUN` state (and after the `starting` action has completed) and is expected to finish as quickly as possible when the
`stop` command is received. The function comes with the `stop_token` parameter which should be used to check for a pending stop request, e.g. like:

```cpp
void NewSatellite::running(const std::stop_token& stop_token) {

    while(!stop_token.stop_requested()) {
        // Do work
    }
}
```

Any finalization of the measurement run should be performed in the `stopping` action rather than at the end of the `running` function, if possible.

## To Reconfigure or Not To Reconfigure

Reconfiguration (partial, fast update of individual parameters) is an optional transition from `ORBIT` to `ORBIT` state. It can
be useful to implement this to allow e.g. fast parameter scans which directly cycle from `RUN` to `ORBIT`, through reconfigure
and back to `RUN`:

```plantuml
@startuml
hide empty description

State ORBIT : Satellite powered
State RUN : Satellite taking data

ORBIT -[#blue,bold]l-> RUN : start
ORBIT -[#blue,bold]-> ORBIT : reconfigure
RUN -[#blue,bold]r-> ORBIT : stop
@enduml
```

without the necessity to land and complete re-initializing the satellite.

However, not all parameters or all hardware is suitable for this, so this transition is optional and needs to be explicitly
enabled in the constructor of the satellite:

```cpp
MySatellite(std::string_view name) : Satellite(name) {
   support_reconfigure();
}
```

and the corresponding transition function `reconfiguring()` needs to be implemented.

The payload of this method is a partial configuration which contains only the keys to be changed. The satellite
implementation should check for the validity of all keys and report in case invalid keys are found.

## Error Handling

Any error that prevents the satellite from functioning (or from functioning *properly*) should throw an exception to notify
the framework of the problem. The Constellation core library provides different exception types for this purpose:

* `CommunicationError` can be used to indicate a failed communication with attached hardware components.
* `MissingKeyError` should be thrown when a mandatory configuration key is absent.
* `InvalidValueError` should be used when a value read from the configuration is not valid.
* `SatelliteError` is a generic exception which can be used if none of the above match the situation.

The message provided with the exception should be as descriptive as possible. It will both be logged and will be used as
status message by the satellite.
