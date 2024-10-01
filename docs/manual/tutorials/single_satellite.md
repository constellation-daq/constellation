# Starting & Controlling a Satellite

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a
Python controller can control a C++ satellite and vice versa. The following tutorial describes how to start a single
satellite and how to discover it and change its state with a controller.

## Starting a Satellite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

The `satellite` executable starts a new Constellation satellite and requires three command line arguments:

- `--type, -t`: Type of satellite. This corresponds to the class name of the satellite implementation.
- `--name, -n`: Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`: This is the name of the Constellation group this satellite should be a part of.

A call with all three parameters provided could e.g. look like follows:

```sh
./build/cxx/constellation/exec/satellite -t Sputnik -n TheFirstSatellite -g MyLabPlanet
```

:::
:::{tab-item} Python
:sync: python

Before starting a Python satellite, the virtual environment created during the installation of the framework might need to be
reactivated using `source venv/bin/activate`.

The satellite is then directly started via its Python module. Additional parameters can be supplied, such as:

- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

Starting the example satellite implementation provided with the framework would therefore look like follows:

```sh
python -m constellation.satellites.example --name TheFirstSatellite --group MyLabPlanet
```

:::
::::

## Controlling the Satellite

A controller running in the same Constellation group is needed in order to control the satellite started in the first part
of this tutorial. This section shows different options to perform this task.

### Starting a Controller

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

The Python implementation of Constellation provides a powerful command line interface controller using IPython. This can be installed with the `cli` component (see [Installing from Source](../install.md#installing-the-constellation-package)).

The controller can be started via its Python module via `python -m constellation.core.controller`, but an entry point is also created on installation which allows starting directly via the command `Controller`. It is possible to pass the controller some (optional) arguments, for example:

- `--name`. A name for the controller (default: cli_controller)
- `--group`. The constellation group to which the controller should belong (default: constellation)
- `--log-level`. The logging verbosity for the controller's log messages (default: info)

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

```sh
Controller --group myLabPlanet
```

The interactive command line provides the `constellation` object which holds all information about connected satellites and
allows their control. Getting a dictionary containing the satellites could e.g. be performed by running:

```python
In [1]: constellation.satellites
Out[1]: {'Sputnik.TheFirstSatellite': <constellation.core.controller.SatelliteCommLink at 0x700590f015b0>}
```

In order to obtain more information on a specific satellite, it can be directly addressed via its type and name
and a command can be sent. The response is then printed on the terminal:

```python
In [2]: constellation.Sputnik.TheFirstSatellite.get_name()
Out[2]: {'msg': 'sputnik.thefirstsatellite', 'payload': None}
```

The controller supports tab completion, and suggestions for possible commands are displayed typing e.g.
`constellation.Sputnik.TheFirstSatellite.` and hitting the tab key.

Since this is an interactive IPython console, of course also loops are possible and could look like this with two satellites
connected:

```python
In [3]: for sat in constellation.satellites.values():
   ...:     print(sat.get_name())
   ...:
{'msg': 'sputnik.thefirstsatellite', 'payload': None}
{'msg': 'sputnik.thesecondsatellite', 'payload': None}
```

:::
::::

### Sending Commands to the Satellite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

Commands can either be sent to individual satellites, all satellites of a given type, or the entire constellation.
All available commands for the `constellation` object are available via tab completion.

To initialize the satellite, it needs to be sent an initialize command, with a dictionary of config options as an argument.
In the following example, this dictionary is empty (`{}`) and directly passed to the command.

```python
In [1]: constellation.Sputnik.TheFirstSatellite.initialize({})
Out[1]:
{'msg': 'transition initialize is being initiated', 'payload': None}
```

All satellites can be initialized together by sending the command to the entire constellation:

```python
In [1]: constellation.initialize({})
Out[1]:
{'Sputnik.TheFirstSatellite': {'msg': 'transition initialize is being initiated', 'payload': None},
 'Sputnik.TheSecondSatellite': {'msg': 'transition initialize is being initiated', 'payload': None}}
```

Whether the satellites have actually changed their state can be checked by retrieving the current state of an individual
satellite or the entire constellation via:

```python
In [2]: constellation.Sputnik.TheFirstSatellite.get_state()
Out[2]:
{'msg': 'init', 'payload': None}
In [3]: constellation.get_state()
Out[3]:
{'Sputnik.TheFirstSatellite': {'msg': 'init', 'payload': None},
 'Sputnik.TheSecondSatellite': {'msg': 'init', 'payload': None}}
```

Similarly, all satellite states can be called. A full list of available commands, along with a description of the finite
state machine can be found in the [concepts chapter on satellites](../concepts/satellite).

:::
::::

### Loading a Configuration File

Constellation configuration files are TOML files with the configuration key-value pairs for all satellites. The individual
satellite configurations are sent to their satellites together with the `initialize` command as dictionary. Their basic
structure and syntax is the following:

```toml
[satellites]
# General settings which apply to all satellites
confidentiality = "TOPSECRET"

[satellites.Mariner]
# Settings which apply to all satellites of type "Mariner"
sample_period = 3.0

[satellites.Mariner.Nine]
# Settings which only apply to the satellite with name "Mariner.Nine"
voltage = 5
current = 0.1
```


::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

When starting the controller, a configuration file can be passed as optional command line argument:

```sh
Controller --group myLabPlanet --config myconfiguration.toml
```

The configuration is then available as `cfg` object on the interactive command line and can be passed to the `initialize`
function:

```python
In [1]: constellation.initialize(cfg)
Out[1]:
{'Sputnik.TheSecondSatellite': {'msg': 'transition initialize is being initiated', 'payload': None},
 'Sputnik.TheFirstSatellite': {'msg': 'transition initialize is being initiated', 'payload': None}}
```

Alternatively, the configuration can be read and parsed in an already running interactive command line session using the
`load_config` function. The resulting dictionary can be directly passed to the `initialize` method, the distribution of
dictionaries to the individual satellites is taken care of by the controller.

```python
In [1]: cfg = load_config("myconfiguration.toml")
In [2]: constellation.initialize(cfg)
Out[2]:
{'Sputnik.TheSecondSatellite': {'msg': 'transition initialize is being initiated', 'payload': None},
 'Sputnik.TheFirstSatellite': {'msg': 'transition initialize is being initiated', 'payload': None}}
```

:::
::::


### Closing the Controller

Controllers in Constellation do not possess state and can be closed and restarted at the discretion of the user without
affecting the state of the satellites.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO.

:::
:::{tab-item} Python
:sync: python

The IPython CLI controller can be disconnected from the constellation using the command `quit` or by pressing Ctrl+D.

:::
::::
