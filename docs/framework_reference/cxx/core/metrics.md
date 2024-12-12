# Telemetry

## Macros for Sensing Metrics

The following macros should be used for triggering sending metrics manually.

```{doxygenfile} constellation/core/metrics/stat.hpp
:sections: define
```

```{note}
The value within the `STAT` macros can be a function. This way the function retrieving the value is only executed when the
conditions are met and there is at least one subscriber for the metric.
```

## `constellation::metrics` Namespace

```{doxygennamespace} constellation::metrics
:content-only:
:members:
:protected-members:
:undoc-members:
```
