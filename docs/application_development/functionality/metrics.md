# Sending Metrics

```{seealso}
For the basic concepts behind telemetry in Constellation and the available metric types check the
[chapter in the operator’s guide](../../operator_guide/concepts/telemetry.md).
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Metrics have to be registered before use. For satellite this can be done via:

```c++
register_metric("NAME", "unit", MetricsValue::LAST_VALUE, "description");
```

A metric can then be send using the `STAT` macros:

```c++
STAT("NAME", get_metric_value());
```

```{seealso}
There are also macros available to only send metrics every *nth* iteration or at most every *t* seconds. These macros are
described in detail in the [framework reference](../../framework_reference/cxx/core/metrics.md).
```

Metrics that are evaluated regularly from a lambda can also be registered:

```c++
register_timed_metric("NAME", "unit", MetricsValue::LAST_VALUE, "description", 10s,
                      [this]() -> std::optional<double> {
    return allowed_to_get_metric() ? std::optional(get_metric_value()) : std::nullopt;
});
```

The lambda can return either the value directly, or an optional, where the empty value means that the metric can currently
not be sent. To check if the satellite is in a given state the `getState()` method can be used, or the following helper:

```c++
register_timed_metric("NAME", "unit", MetricsValue::LAST_VALUE, "description", 10s,
                      {CSCP::State::ORBIT, CSCP::State::RUN},
                      [this]() { return get_metric_value(); }
);
```

Note that timed metrics can still be triggered manually if desired.

:::
:::{tab-item} Python
:sync: python

To send metrics (e.g. readings from a sensor), the `schedule_metric` method can be used. The method takes a metric name,
the unit, a polling interval, a metric type and a callable function as arguments. The name of the metric will always be
converted to full caps.

* Metric type: can be `LAST_VALUE`, `ACCUMULATE`, `AVERAGE`, or `RATE`.
* Polling interval: a float value in seconds, indicating how often the metric is transmitted.
* Callable function: Should return a single value (of any type), and take no arguments. If you have a callable that requires
  arguments, consider using `functools.partial` to fill in the necessary information at scheduling time. If the callback
  returns `None`, no metric will be published.

An example call is shown below:

```python
self.schedule_metric("Current", "A", MetricsType.LAST_VALUE, 10, self.device.get_current)
```

In this example, the callable `self.device.get_current` fetches the current from a power supply every 10 seconds, and returns
the value `return current`. The same can also be achieved with a function decorator:

```python
@schedule_metric("A", MetricsType.LAST_VALUE, 10)
def Current(self) -> Any:
    if self.device.can_read_current():
        return self.device.get_current()
    return None
```

Note that in this case, if `None` is returned, no metric is sent. The name of the metric is taken from the function name.

```{attention}
Metrics registered with the function decorator are evaluated even if the satellite is not initialized yet. This means that it
might be necessary to check the existence of a variable using `hasattr(self, "device")` first.
```

:::
::::
