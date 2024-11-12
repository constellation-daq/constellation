<!-- markdownlint-disable MD041 -->
### Parameters inherited from `Satellite`

#### Heartbeating

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_allow_departure` | Bool | If `true`, regular departures of satellites will not cause an interrupt to SAFE mode | `true` |
| `_heartbeat_interval` | Unsigned integer | Interval in seconds between heartbeats to be sent to other Constellation components | `10` |

#### Transition Orchestration

Parameters to control [autonomous transition orchestration](../manual/concepts/autonomy.md) of satellites:

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_conditional_transition_timeout` | Integer | Timeout in seconds to wait for all conditions to be satisfied for conditional transitioning | `60` |
| `_require_initializing_after` | String or list of strings | Canonical name or list of canonical names of remote satellites for conditional transitioning in `initializing` state | - |
| `_require_launching_after` | String or list of strings | Canonical name or list of canonical names of remote satellites for conditional transitioning in `launching` state | - |
| `_require_landing_after` | String or list of strings | Canonical name or list of canonical names of remote satellites for conditional transitioning in `landing` state | - |
| `_require_starting_after` | String or list of strings | Canonical name or list of canonical names of remote satellites for conditional transitioning in `starting` state | - |
| `_require_stopping_after` | String or list of strings | Canonical name or list of canonical names of remote satellites for conditional transitioning in `stopping` state | - |
