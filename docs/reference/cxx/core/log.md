# Logging

This page documents the logging facilities for the Constellation C++ implementation.

## Logging Macros

The following macros should be used for any logging. They ensure proper evaluation of logging levels and additional
conditions before the stream is evaluated.

```{doxygenfile} constellation/core/log/log.hpp
:sections: define
```

## `constellation::log` Namespace

```{doxygennamespace} constellation::log
:content-only:
:members:
:protected-members:
:undoc-members:
```
