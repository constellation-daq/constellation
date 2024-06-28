# Implementing a new Satellite in C++

This how-to guide will walk through the implementation of a new satellite, written in C++, step by step. The entire
procedure should not take too long, but this of course depends on the complexity of the satellite functionality.
It is recommended to have a peek into the overall [concept of satellites](../concepts/satellite.md) in Constellation in
order to get an impression of which functionality of the application could fit into which state of the finite state machine.

```{note}
This how-to describes the procedure of implementing a new Constellation satellite in C++. For Python look [here](./satellite_py.md) and
for the microcontroller implementation, please refer to the [MicroSat project](https://gitlab.desy.de/constellation/microsat/).
```

## Sending or Receiving Data

The first decision that needs to be taken is whether the satellite will produce and transmit data, or if it will receive and
process data from other satellites.

## Implementing the FSM Transitions

In Constellation, actions such as device configuration and initialization are realized through so-called transitional states
which are entered by a command and exited as soon as their action is complete. A more detailed description on this can be found
in the [satellite section](../concepts/satellite.md) of the framework concepts overview. The actions attached to these
transitional states are implemented by overriding the virtual methods provided by the `Satellite` base class.

For a new satellite, the following transitional state actions **should be implemented**:

* `void ExampleSatellite::initializing(config::Configuration& config)`
* `void ExampleSatellite::launching()`
* `void ExampleSatellite::landing()`
* `void ExampleSatellite::starting(std::string_view run_identifier)`
* `void ExampleSatellite::stopping()`

The following transitional state actions are optional:

* `void ExampleSatellite::reconfiguring(const config::Configuration& config)`: implements a fast partial reconfiguration of the satellite, see below for a detailed description.
* `void ExampleSatellite::interrupting()`: this is the transition to the `SAFE` state and defaults to `stopping` (if necessary because current state is `RUN`), followed by `landing`. If desired, this can be overwritten with a custom action.

For the steady state action for the `RUN` state, see below.

## Running and the Stop Token

The satellite's `RUN` state is governed by the `running` action, which - just as the transitional state actions above - is overridden from the `Satellite` base class.
The function will be called upon entering the `RUN` state (and after the `starting` action has completed) and is expected to finish as quickly as possible when the
`stop` command is received. The function comes with the `stop_token` parameter which should be used to check for a pending stop request, e.g. like:

```cpp
void ExampleSatellite::running(const std::stop_token& stop_token) {

    while(!stop_token.stop_requested()) {
        // Do work
    }

    // No heavy lifting should be performed here once a stop has been requested
}
```

```{note}
Any finalization of the measurement run should be performed in the `stopping` action rather than at the end of the `running` function, if possible:
```

```cpp
void ExampleSatellite::stopping() {
    // Perform cleanup action here
}
```


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
ExampleSatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
   support_reconfigure();
}
```

and the corresponding transition function `reconfiguring(const config::Configuration& config)` needs to be implemented.

The payload of this method is a partial configuration which contains only the keys to be changed. The satellite
implementation should check for the validity of all keys and report in case invalid keys are found.

## Error Handling

Any error that prevents the satellite from functioning (or from functioning *properly*) should throw an exception to notify
the framework of the problem. The Constellation core library provides different exception types for this purpose.

### Generic Errors

* `SatelliteError` is a generic exception which can be used if none of the other available exception types match the situation.
* `CommunicationError` can be used to indicate a failed communication with attached hardware components.

### Configuration Errors

* `MissingKeyError` should be thrown when a mandatory configuration key is absent.
* `InvalidValueError` should be used when a value read from the configuration is not valid.

The message provided with the exception should be as descriptive as possible. It will both be logged and will be used as
status message by the satellite.

## Building the Satellite

Constellation uses the [Meson build system](https://mesonbuild.com/) and setting up a `meson.build` file is required for the
code to by compiled. The file should contain the following sections and variable definitions:

* First, the type this satellite identifies as should be defined by setting `satellite_type = 'Example'`. This will be the type
  by which new satellites are invoked and which will become part of the canonical name of each instance, e.g. `Example.MySat`.
* Then, potential dependencies of this satellite should be resolved

  ```meson
  my_dep = dependency('TheLibrary')
  ```

  More details on Meson dependencies can be found [elsewhere](https://mesonbuild.com/Dependencies.html).

* The source files which need to be compiled for this satellite should be listed in the `satellite_sources` variable:

  ```meson
  satellite_sources = files(
    'PrototypeSatellite.cpp',
  )
  ```

* Constellation automatically generates the shared library for the satellite as well as an executable. This is done by the
  remainder of the build file, which can be copied verbatim apart from the potential dependency to be added.

  Setup of satellite configuration, inclusion of the C++ generator file, generation of final sources:

  ```meson
  satellite_cfg_data = configuration_data()
  satellite_cfg_data.set('SATELLITE_CLASS', satellite_type)
  satellite_generator = configure_file(
    input: satellite_generator_template,
    output: 'generator.cpp',
    configuration: satellite_cfg_data,
  )
  satellite_main = configure_file(
    input: satellite_main_template,
    output: 'main.cpp',
    configuration: satellite_cfg_data,
  )
  ```

  Compilation targets, a shared library for the satellite and the corresponding executable. Here, the dependency stored in
  `my_dep` has to be added to the list of library dependencies:

  ```meson
  shared_library(satellite_type,
    sources: [satellite_generator, satellite_sources],
    dependencies: [core_dep, satellite_dep, my_dep],
    gnu_symbol_visibility: 'hidden',
    install_dir: satellite_libdir,
    install_rpath: constellation_rpath,
  )

  executable('satellite' + satellite_type,
    sources: [satellite_main],
    dependencies: [exec_dep],
    install: true,
    install_rpath: constellation_rpath,
  )
  ```
