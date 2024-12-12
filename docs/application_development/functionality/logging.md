# Logging

```{seealso}
For the basic concepts behind logging in Constellation and the available log level check the
[chapter in the operator's guide](../../operator_guide/concepts/logging.md).
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

In C++, a set of macros are provided for different scenarios:

```cpp
// Logging a message with the given level:
LOG(INFO) << Received configuration";

// Logging a message only once (e.g. in a loop):
LOG_ONCE(WARNING) << "This message appears only once even if the statement "
                  << "is executed many times";

// Logging a message N times:
LOG_N(STATUS, 3) << "This message is logged at most 3 times.";

// Logging only when a condition evaluates to true:
LOG_IF(WARNING, parameter == 5) << "Parameter 5 is set "
                               << "- be careful when opening the box!";

// Logging a message only every Nth time:
LOG_NTH(STATUS, 100) << "This message is logged every 100th call to the logging macro.";
```

```{seealso}
There is also a possibility of setting up individual loggers with different topics as described in the
[framework reference](../../framework_reference/cxx/core/log.md).
```

:::
:::{tab-item} Python
:sync: python

In Python, the regular logging facility should be used. The `Satellite` class provides a `log` object which has the correct
logging levels and network connections already set up:

```python
self.log.status("Thanks for all the fish")
self.log.critical("Lost device connection")
self.log.warning("Temperature out of expected range")
self.log.info("Received configuration")
self.log.debug("Connected to device")
self.log.trace("Read 8 bytes from device")
```

:::
::::
