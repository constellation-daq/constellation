# Starting & Controlling a Satellite

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a
Python controller can control a C++ satellite and vice versa. The following tutorial describes how to start a single
satellite and how to discover it and change its state with a controller.

## Starting a Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

The `satellite` executable starts a new Constellation satellite and requires three command line arguments:

- `--type, -t`: Type of satellite. This corresponds to the class name of the satellite implementation.
- `--name, -n`: Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`: This is the name of the Constellation group this satellite should be a part of.

A call with all three parameters provided could e.g. look like follows:

```sh
./build/cxx/constellation/exec/satellite -t Prototype -n TheFirstSatellite -g MyLabPlanet
```

:::

:::{tab-item} Python
:sync: keyP

Before starting a Python satellite, the virtual environment created during the installation of the framework might need to be
reactivated using `source venv/bin/activate`.

The satellite is then directly started via its Python module. Additional parameters can be supplied, such as:

- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

Starting the example satellite implementation provided with the framework would therefore look like follows:

```sh
python -m constellation.satellites.example_satellite --name TheFirstSatellite --group MyLabPlanet
```

:::

::::

## Controlling the Satellite

A controller running in the same Constellation group is needed in order to control the satellite started in the first part
of this tutorial. This section shows different options to perform this task.

### Starting a Controller

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO

:::

:::{tab-item} Python
:sync: keyP

The Python implementation of Constellation provides a powerful command line interface controller using IPython. This package
needs to be installed (`pip install ipython`) in the virtual environment that was created during the installation of the
framework and can be reactivated using `source venv/bin/activate`.

The controller can be started via its Python module via `python -m constellation.core.controller`, but an entry point is also created on installation which allows starting via the command `Controller`. It is possible to pass the controller some (optional) arguments, for example:

- `--name`. A name for the controller (default: cli_controller)
- `--group`. The constellation group to which the controller should belong (default: constellation)
- `--log-level`. The logging verbosity for the controller's log messages (default: info)

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

```sh
Controller --group myLabPlanet
```

The interactive command line provides the `constellation` object which holds all information about connected satellites and
allows their control. Getting a list containing the satellites could e.g. be performed by running:

```python
In [1]: constellation.satellites
Out[1]:
[<__main__.SatelliteCommLink at 0x78d5bba4fcd0>]
```

In order to obtain more - and less cryptic - information on a specific satellite, it can be directly addressed in the list
and a command can be sent. The response is then printed on the terminal:

```python
In [2]: print(constellation.satellites[0].get_name())
Out[2]:
{'msg': 'prototype.thefirstsatellite', 'payload': None}
```

Since this is an interactive IPython console, of course also loops are possible and could look like this with two satellites
connected:

```python
In [3]: for sat in constellation.satellites:
   ...:     print(sat.get_name())
   ...:
{'msg': 'prototype.thefirstsatellite', 'payload': None}
{'msg': 'prototype.thesecondsatellite', 'payload': None}
```

:::

::::

### Sending Commands to the Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO

:::

:::{tab-item} Python
:sync: keyP

Commands can either be sent to individual satellites, all satellites of a given type, or the entire constellation.
All available commands for the `constellation` object are available via tab completion.

To initialize the satellite, it needs to be sent an initialize command, with a dictionary of config options as an argument.
In the following example, this dictionary is empty (`{}`) and directly passed to the command.

```python
In [1]: constellation.satellites[0].initialize({})
Out[1]:
{'msg': 'transition initialize is being initiated', 'payload': None}
```

Whether the satellite has actually changed its state can be checked by retrieving the current state via:

```python
In [2]: constellation.satellites[0].get_state()
Out[2]:
{'msg': 'init', 'payload': None}
```

Similarly, all satellite states can be called. A full list of available commands, along with a description of the finite
state machine can be found in the [concepts chapter on satellites](../concepts/satellite).


#### Using a configuration file

A configuration file can be loaded into the initialiser by using the flag `--config` when launching the controller. This loads the provided configuration into a dictionary named `cfg`, which can be used e.g. when calling `initialize`. An example using the `example_satellite` is shown below, along with the used example configuration file.

```python
Controller --config python/constellation/satellites/example_config.conf
In [1]: constellation.Example_Satellite.initialize(cfg["satellites"]["example_satellite"]["device1"])
```

```TOML
[satellites.example_satellite.device1]
voltage = 5
current = 0.1
sample_period = 3.0
```

:::

::::

### Closing the Controller

Controllers in Constellation do not posses state and can be closed and restarted at the discretion of the user without
affecting the state of the satellites.

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO.

:::

:::{tab-item} Python
:sync: keyP

The IPython CLI controller can be disconnected from the constellation using the command `exit()`.

:::

::::
