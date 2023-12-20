#!/usr/bin/env python3
from enum import Enum, auto
from statemachine import StateMachine
from statemachine.states import States


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    IDLE = auto()
    # Initialized state with configuration but not (fully) applied
    INIT = auto()
    # Prepared state where configuration is applied
    ORBIT = auto()
    # Running state where DAQ is running
    RUN = auto()
    # Safe fallback state if error is discovered during run
    SAFE = auto()
    # Error state if something went wrong
    ERROR = auto()


class SatelliteResponse(Enum):
    SUCCESS = auto()
    INVALID = auto()
    NOTIMPLEMENTED = auto()
    INCOMPLETE = auto()


class SatelliteFSM(StateMachine):
    """Manage the satellite's state and its transitions."""

    # Convert enum to states
    states = States.from_enum(SatelliteState, initial=SatelliteState.IDLE)

    # Define transitions
    # - IDLE <=> INIT
    load = states.IDLE.to(states.INIT)
    unload = states.INIT.to(states.IDLE)
    # - INIT <=> ORBIT
    launch = states.INIT.to(states.ORBIT)
    land = states.ORBIT.to(states.INIT)
    # - ORBIT <=> RUN
    start = states.ORBIT.to(states.RUN)
    stop = states.RUN.to(states.ORBIT)
    # - safe
    interrupt = states.ORBIT.to(states.SAFE) | states.RUN.to(states.SAFE)
    recover = states.SAFE.to(states.INIT)
    # - error
    failure = states.RUN.to(states.ERROR) | states.ORBIT.to(states.ERROR) | states.INIT.to(states.ERROR) | states.SAFE.to(states.ERROR)
    reset = states.ERROR.to(states.IDLE)

    def __init__(self):
        super().__init__()

    def write_diagram(self, filename):
        """Create a png with the FSM schematic."""
        from statemachine.contrib.diagram import DotGraphMachine
        graph = DotGraphMachine(self)
        dot = graph()
        dot.write_png(filename)
