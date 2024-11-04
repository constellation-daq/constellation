# Adding Custom Satellite Commands

This how-to guide will walk through the process of adding and registering custom commands for a satellite.
This functionality can be used to expose additional functionality of the devices that is handled by the satellite to
controllers in the Constellation network.

## The Command Registry

The satellite command registry provides a facility where the satellite can register arbitrary commands and expose them
to any controller connecting to the satellite. These commands are defined per individual satellite type and are registered
with the framework libraries.

Controllers can query for commands using the `get_commands` request which returns a list of available commands and FSM
transitions along with their descriptions.

Custom commands cannot overwrite the standard commands provided by Constellation satellites and therefore must not reuse
their names.

## Registering The Command

::::{tab-set}
:::{tab-item} C++
:sync: cxx

In C++, commands are registered in the satellite constructor.
In this example, the following command is added to the satellite `MySatellite`:

```cpp
int MySatellite::get_channel_reading(int channel) {
  auto value = device_->read_channel(channel);
  return value / 10;
}
```

In the constructor of `MySatellite`, the command is registered with the command registry as follows:

```cpp
using constellation::protocol::CSCP;

MySatellite::MySatellite(std::string_view type, std::string_view name) : Satellite(type, name) {
    register_command("get_channel_reading",
                     "This command reads the current device value from the channel number provided as argument. Since this"
                     "will reset the corresponding channel, this can only be done before the run has started.",
                     {State::NEW, State::INIT, State::ORBIT},
                     &MySatellite::get_channel_reading,
                     this);
}
```

The arguments here are, in this order, the name of the command, its description, the allowed states this command can be
called in, the pointer to the command function and the pointer to this satellite instance. The individual parts of the
registration process are discussed below in detail.

:::
:::{tab-item} Python
:sync: python

In Python, commands are registered by placing the `@cscp_requestable` decorator above the method defining the command.
The method needs to have a specific signature:

```python
def COMMAND(self, request: cscp.CSCPMessage) -> tuple[str, Any, dict]:
```

The expected return values are:

- reply message (string)
- payload (any)
- map (dictionary) (e.g. for meta information)

Adding a custom command thus also requires `from constellation.core.cscp import CSCPMessage`.

In this example, the command `get_channel_reading(channel: int)` is added to a satellite:

```python
@cscp_requestable
def get_channel_reading(self, request: CSCPMessage) -> tuple[str, Any, dict]:
    """Read the value of the channel given by the first supplied argument."""
    paramList = request.payload
    channel = paramList[0]
    value = _device.read(channel)
    return str(value / 10), None, {}
```

The `@cscp_requestable` decorator registers the command with the command registry, and the comment block in the beginning is
the description of the command, available from the command registry. The command is called via
`constellation.MySatellite.get_channel_reading([1])` in the Controller, to read channel 1.

:::
::::

## Name and description

The name of the command is the handle with which it will be called from a controller. It should be short and descriptive and
only contain alphanumeric characters and underscores. The description should comprehensively describe the command, its
required arguments and the return value.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

In addition to this information, the number of required arguments as well as the allowed states are automatically appended
to the description reported by the satellite e.g. through its `get_commands` response. For the example command registered
above, the output could look like this:

```text
get_channel_reading:  This command reads the current device value from the channel number provided as argument. Since this
                      will reset the corresponding channel, this can only be done before the run has started.
                      This command requires 1 arguments.
                      This command can only be called in the following states: NEW, INIT, ORBIT
```

:::
::::

## Command arguments and return values

The command registry can handle commands with any number of arguments and can also provide return values from the called
functions back to the controller.

```{note}
All parameters and return values of functions in the command registry are encoded as configuration values and must therefore
be able to be converted to and from these. A detailed information of available types is available in the configuration
documentation.
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Arguments do not have to be specifically denominated when registering the command. The
command registry instead takes this information directly from the function declaration of the command to be called.

:::
:::{tab-item} Python
:sync: python

It should be noted that the parameters are given to the command as a list which is the `payload` of the `CSCPMessage` in the
custom command. Individual arguments need to be accessed via their list index.

:::
::::

```{warning}
It is discouraged to implement commands that change the configuration of the instrument or device since these changes take
direct effect and are not reflected in the satellite configuration.
```

## Allowed FSM states

Commands may change the internal state of the satellite e.g. by altering the setting of an attached device. It may therefore
be important for some commands to not run when the satellite state machine is in a given state.

A typical example would be a high-voltage power supply, and a custom command that allows reading the current compliance.
While reading this limit when the satellite is in its `ORBIT` or `RUN` state will produce the correct result, reading the
value in the `NEW` or `INIT` states where the power supply is not fully configured yet may yield wrong values.

The command registry allows limiting the states in which each of the commands can be executed and will report an invalid
command otherwise.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

In C++, the allowed states are provided as part of the command registration as described [above](#registering-the-command).

:::
:::{tab-item} Python
:sync: python

In Python, this is handled by adding a method with the signature

```python
def _COMMAND_is_allowed(self, request: cscp.CSCPMessage) -> bool:
```

to the satellite. If this exists, it will be called before the method of the custom command is called, to determine whether it is allowed or not. An example is shown below, limiting the usage of the `get_channel_reading` command to the states `INIT` and `ORBIT`:

```python
def _get_channel_reading_is_allowed(self, request: CSCPMessage) -> bool:
    """Allow in the states INIT and ORBIT, but not during RUN"""
    return self.fsm.current_state.id in ["INIT", "ORBIT"]
```

:::
::::
