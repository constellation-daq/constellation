# Porting a EUDAQ Producer

This how-to goes through the steps necessary to port a EUDAQ Producer to the Constellation framework.
In Constellation terminology, a *Producer* is called a *Satellite*. As will become clear soon, both the finite state machine
that governs the life cycle of a satellite as well as the mechanism for logging information and transmitting data are very
similar from an interface perspective.

## Porting the Finite State Machine Transitions

Constellation satellites are built around a [finite state machine](../concepts/satellite.md) which works similar to EUDAQs
Producers, with the exception that the states have different names and a few more transitions between states are possible.
In order to port the functionality of the EUDAQ Producer, the code from its functions can directly be copied into a
corresponding Constellation satellite skeleton. The following table helps in finding the corresponding function name:

| EUDAQ Producer | Constellation Satellite | Notes
| ---------------| ----------------------- | -----
| `DoInitialise` | `initializing`          | Direct equivalent. In EUDAQ, this function only receives the "init" portion of the configuration, in Constellation the entire satellite configuration is provided at this stage.
| `DoConfigure`  | `launching`             | Direct equivalent. Powers up the attached instrument and configures it to a state where it is ready to enter data taking.
| -              | `landing`               | Landing is the opposite action as launching and should shut down the attached instrument in a controlled manner to bring it back into the INIT state. EUDAQ does not have a direct equivalent, but `DoReset` works in a similar fashion.
| -              | `reconfiguring`         | Reconfiguration is a concept specific to Constellation. It is an optional method which allows to alter a subset of configuration values without landing first. More information is available [here](./satellite_cxx.md).
| `DoStartRun`   | `starting`              | Direct equivalent. Starts a new data taking run.
| `DoStopRun`    | `stopping`              | Direct equivalent. Stops the run currently in progress.
| `RunLoop`      | `running`               | Equivalent with the difference that instead of keeping track of the running via a custom variable (like `m_running`) often found in EUDAQ Producers, Constellation provides a stop token. More information is available [here](./satellite_cxx.md).
| `DoReset`      | -                       | In EUDAQ, this command puts the producer back into the initial state. In contrast, resetting a satellite in Constellation means calling its `initializing` method again.
| `DoTerminate`  | Destructor              | The EUDAQ equivalent of the `terminate` action in Constellation is the `shutdown` command which can only be triggered when not running or in orbit. The corresponding code should be placed in the destructor of the satellite.
| `DoStatus`     | -                       | In Constellation no `DoStatus` equivalent exists because statistical metrics are transmitted differently, more similar to log messages. More information can be found [here](../concepts/statistics.md).

## Transmitting Data

Data in EUDAQ is transmitted as `RawEvent` objects. There is the possibility of adding multiple so-called data blocks to a single
event, as well as the option to store events as sub-events of others. The creation, outfitting and sending of a message in EUDAQ
follows roughly this pattern:

```cpp
// Create new event and set its ID
auto event = eudaq::Event::MakeUnique("MyDetectorEvent");
event->SetEventN(m_ev);
// Add data to the event
event->AddBlock(0, data);
// Possibly add a tag
event->SetTag("my_tag", std::to_string(my_value));
// Send the event
SendEvent(std::move(event));
```

Constellation data messages consist of a header with metadata such as the sending satellite and the continuous message sequence,
and any number of frames with binary data. These frames can be likened to the EUDAQ data blocks. A concept akin to sub-events
does not exits in Constellation. Knowing this, the above sequence would translate to the following satellite code:

```cpp
// Create new message, message sequence is handled by Constellation
auto msg = newDataMessage();
// Add data to the message
msg.addFrame(std::move(data));
// Possibly add a tag
msg.addTag("my_tag", my_value)
// Send the message
sendDataMessage(msg);
```

It should be noted that tag values in EUDAQ are limited to `std::string` while Constellation tags can hold any configuration
data type.

Further information on data transmission can be found in the how-to section on [Implementing a Satellite in C++](satellite_cxx.md).

## Adjusting the Logging Mechanism

In EUDAQ, log messages are sent via the logging macros `EUDAQ_DEBUG`, `EUDAQ_INFO`, `EUDAQ_WARN`, and `EUDAQ_ERROR` which
take a single string as argument. In Constellation, [similar log levels](../concepts/logging.md) exist and logging can
directly be translated to the corresponding macros. Instead of single strings, output streams are used which are more
flexible in converting different variable types to strings automatically. For example the EUDAQ log message:

```cpp
EUDAQ_DEBUG("Current temperature for channel " + std::to_string(ch) + " is " + std::to_string(t) + "C");
```

turns into the equivalent Constellation log of the same verbosity level:

```cpp
LOG(DEBUG) << "Current temperature for channel " << ch << " is " << t << "C";
```

## Error Handling

In addition to the logging macros, EUDAQ provides the `EUDAQ_THROW` macro which can be used to throw exceptions. Instead of
providing macros for this purpose, Constellation encourages the direct use of exceptions. This not only improves clarity but
it also allows for the use of different exceptions classes for different purposes. A EUDAQ error handling code such as

```cpp
auto power_output_percent = config->Get("power_output_percent", 0);
if(power_output_percent > 100) {
    EUDAQ_THROW("Cannot set output power to " + std::to_string(power_output_percent) + "%, 100% is the maximum!");
}
```

would become a Constellation exception of type `InvalidValueError`. This allows the message to be more informative and to
tell the user e.g. in which configuration file and at which position the invalid value can be found:

```cpp
auto power_output_percent = config.get<int>("power_output_percent");
if(power_output_percent > 100) {
    throw InvalidValueError(config, "power_output_percent", "Value too large, 100% is the maximum!");
}
```

Moreover, the attempt to read configuration keys without default value will automatically emit a `MissingKeyError` exception
if the respective key is not present in the configuration.
