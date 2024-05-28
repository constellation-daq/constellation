# Implementing a new Satellite in Python

This how-to guide will walk through the implementation of a new satellite, written in Python, step by step.
It is recommended to have a peek into the overall [concept of satellites](../concepts/satellite.md) in Constellation in
order to get an impression of which functionality of the application could fit into which state of the finite state machine.

```{note}
This how-to describes the procedure of implementing a new satellite for Constellation in Python. For C++ look [here](./satellite_cxx.md) and
for the microcontroller implementation, please refer to the [MicroSat project](https://gitlab.desy.de/constellation/microsat/).
```

## Implementing the FSM Transitions

In Constellation, actions such as device configuration and initialization are realized through so-called transitional states
which are entered by a command and exited as soon as their action is complete. A more detailed description on this can be found
in the [satellite section](../concepts/satellite.md) of the framework concepts overview. The actions attached to these
transitional states are implemented by overriding methods provided by the `Satellite` base class.

For a new satellite, the following transitional state actions **should be implemented**:

* `def do_initializing(self, config: Configuration)`
* `def do_launching(self, payload: any)`
* `def do_landing(self, payload: any)`
* `def do_starting(self, payload: any)`
* `def do_stopping(self, payload: any)`

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

## Logging

Logging from a satellite can be done by using `self.log.LOG_LEVEL("Message")`, where possible log levels are `debug`, `info`, `warning`, `error`, `critical`.
An example info level log message is shown below:

```python
self.log.info("Landing satellite; ramping down voltage")
```

The log messages are broadcast to listeners, and can be listened on by using e.g. `python -m constellation.core.monitoring`.

## Sending stats

To send metrics (e.g. readings from a sensor), the `schedule_metrics` method can be used. The method takes a metric name, a callable function, a polling interval, and a metric type as arguments.

* Callable function: Should return a single value, with the option of also returning a unit. If only a value is returned, no unit will be transmitted. If a tuple is returned, the first element is taken as the value, and the second as the unit. The callable needs to return a value (of any type) and a unit (of type string), and take no arguments. If you have a callable that requires arguments, consider using functools.partial to fill in the necessary information at scheduling time.
* Polling interval: a float value in seconds, indicating how often the metric is transmitted.
* Metric type: can be `LAST_VALUE`, `ACCUMULATE`, `AVERAGE`, or `RATE`. The default type is `LAST_VALUE`.

An example call is shown below:

```python
self.schedule_metric("Current", self.device.get_current, interval=5.0)
```

In this example, the callable `self.device.get_current` fetches the current from a power supply, and returns a tuple of value and unit via `return current, "A"`.

## Adding entry point installation for the satellite

To make the satellite immediately accessible via the command line, add a line with the desired name and the Python path to the module under the `[project.scripts]` header in `pyprojects.toml` file in the Constellation root directory. For the example satellite, this is done via the line

```TOML
ExampleSatellite = "constellation.satellites.example_satellite:main"
```

By running `pip install -e .` in the Constellation root directory after adding a line for the satellite makes it directly available to the command line, e.g. as `ExampleSatellite`.
