#!/usr/bin/env python3
from enum import Enum, auto
from statemachine import StateMachine
from statemachine.states import States


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    NEW = auto()
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


class SatelliteFSM(StateMachine):
    """Manage the satellite's state and its transitions."""

    # Convert enum to states
    states = States.from_enum(SatelliteState, initial=SatelliteState.NEW)

    # Define transitions
    # - NEW <=> INIT
    initialize = states.NEW.to(states.INIT) | states.INIT.to(states.INIT)
    initialize |= states.ERROR.to(states.INIT)

    # - INIT <=> ORBIT
    launch = states.INIT.to(states.ORBIT)
    land = states.ORBIT.to(states.INIT)
    # - ORBIT <=> RUN
    start = states.ORBIT.to(states.RUN)
    stop = states.RUN.to(states.ORBIT)
    reconfigure = states.ORBIT.to(states.ORBIT)
    # - safe
    interrupt = states.ORBIT.to(states.SAFE) | states.RUN.to(states.SAFE)
    recover = states.SAFE.to(states.INIT)
    # - error
    failure = states.RUN.to(states.ERROR) | states.ORBIT.to(states.ERROR)
    failure |= states.INIT.to(states.ERROR) | states.SAFE.to(states.ERROR)
    failure |= states.ERROR.to(states.ERROR) | states.NEW.to(states.ERROR)

    def __init__(self):
        self.status = "Satellite not initialized yet."
        super().__init__()

    def before_initialize(self, status):
        """Set status before the state change."""
        self.status = status

    def before_launch(self, status):
        """Set status before the state change."""
        self.status = status

    def before_land(self, status):
        """Set status before the state change."""
        self.status = status

    def before_start(self, status):
        """Set status before the state change."""
        self.status = status

    def before_stop(self, status):
        """Set status before the state change."""
        self.status = status

    def before_reconfigure(self, status):
        """Set status before the state change."""
        self.status = status

    def before_interrupt(self, status):
        """Set status before the state change."""
        self.status = status

    def before_recover(self, status):
        """Set status before the state change."""
        self.status = status

    def before_failure(self, status):
        """Set status before the state change."""
        self.status = status

    def write_diagram(self, filename):
        """Create a png with the FSM schematic."""
        from statemachine.contrib.diagram import DotGraphMachine

        graph = DotGraphMachine(self)
        dot = graph()
        dot.write_png(filename)
