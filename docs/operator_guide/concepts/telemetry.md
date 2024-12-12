# Telemetry

Constellation comes with a builtin system for gathering telemetry to provide online monitoring during data taking.
A satellite can send register and send metrics for this purpose, which consist of a name, a unit and a metric type.

```{seealso}
Details about how to implement telemetry and metrics can be found in the
[application development guide](../../application_development/functionality/metrics.md).
```

## Metric Types

Four types of metrics can be defined, that define how the metric should be handled on receiving a new value:

- `LAST_VALUE`: display the last value
- `ACCUMULATE`: display the sum of all received values
- `AVERAGE`: display an average of the received values
- `RATE`: display the rate of the value

```{attention}
Currently, the `Influx` satellite cannot add this information to the database nor handles it locally. Using `LAST_VALUE` and
handling accumulation, averages and rates locally is currently recommended for now.
```
