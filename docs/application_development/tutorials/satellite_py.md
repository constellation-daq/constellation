# Implementing a Satellite (Python)

This tutorial will walk through the implementation of a new satellite, written in Python, step by step.
It is recommended to have a peek into the overall [concept of satellites](../../operator_guide/concepts/satellite.md)
in Constellation in order to get an impression of which functionality of the application could fit into which state of the
finite state machine.

```{seealso}
This how-to describes the procedure of implementing a new satellite for Constellation in Python. For C++ look [here](./satellite_cxx.md)
and for the microcontroller implementation, please refer to the [MicroSat project](https://gitlab.desy.de/constellation/microsat/).
```

## Implementing the FSM Transitions

In Constellation, actions such as device configuration and initialization are realized through so-called transitional states
which are entered by a command and exited as soon as their action is complete. The actions attached to these transitional
states are implemented by overriding methods provided by the `Satellite` base class.

For a new satellite, the following transitional state actions **should be implemented**:

* `def do_initializing(self, config: Configuration)`
* `def do_launching(self)`
* `def do_landing(self)`
* `def do_starting(self, run_identifier: str)`
* `def do_stopping(self)`

The following transitional state actions are optional:

* `def do_interrupting(self)`: this is the transition to the `SAFE` state and defaults to `do_stopping` (if necessary because current state is `RUN`), followed by `do_landing`. If desired, this can be overwritten with a custom action.

For the steady state action for the `RUN` state, see below.

## Running and the Stop Event

The satellite's `RUN` state is governed by the `do_run` action, which - just as the transitional state actions above - is overridden from the `Satellite` base class.
The function will be called upon entering the `RUN` state (and after the `do_starting` action has completed) and is expected to finish as quickly as possible when the
`stop_running` Event is set. An example run loop is shown below:

```python
def do_run(self, payload: any) -> str:
    # the stop_running Event will be set from outside the thread when it is
    # time to close down.
    while not self._state_thread_evt.is_set():
        # Do work
    return "Finished acquisition."
```

Any finalization of the measurement run should be performed in the `do_stopping` action rather than at the end of the `do_run` function, if possible.

## Installation of satellite

To make the satellite accessible via the command line, it creates a main function in `__main__.py`. Those functions are
besides the satellite name mostly the same and can be copied e.g. from the Mariner satellite.

Subsequently, all Python files have to be added to a `meson.build` file, where again the file from the Mariner satellite can be used as a template.
The folder which includes the satellite files then needs to be included the `meson.build` file in
`python/constellation/satellites`.

To create an executable for the satellite, the main function can be added to `[project.scripts]` in the `pyprojects.toml`
file in the root directory. For the Mariner satellite, this is done via the line:

```TOML
SatelliteMariner = "constellation.satellites.Mariner.__main__:main"
```

By running `pip install --no-build-isolation -e .` in the root directory after adding a line for the satellite, the satellite
is available to the command line, e.g. as `SatelliteMariner`.
