#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import re
from typing import Callable, Any, Tuple
from threading import Event
from concurrent.futures import ThreadPoolExecutor, Future
from enum import Enum
from datetime import datetime, timezone
from statemachine import StateMachine
from statemachine.exceptions import TransitionNotAllowed
from statemachine.states import States
from msgpack import Timestamp  # type: ignore[import-untyped]

from .cscp import CSCPMessage
from .error import debug_log, handle_error
from .base import BaseSatelliteFrame
from .commandmanager import cscp_requestable


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    NEW = 0x10
    # Initialized state with configuration but not (fully) applied
    INIT = 0x20
    # Prepared state where configuration is applied
    ORBIT = 0x30
    # Running state where DAQ is running
    RUN = 0x40
    # Safe fallback state if error is discovered during run
    SAFE = 0xE0
    # Error state if something went wrong
    ERROR = 0xF0
    #
    #  TRANSITIONAL STATES
    #
    initializing = 0x12
    launching = 0x23
    landing = 0x32
    reconfiguring = 0x33
    starting = 0x34
    stopping = 0x43
    interrupting = 0x0E
    # state if shutdown
    DEAD = 0xFF


class SatelliteFSM(StateMachine):
    """Manage the satellite's state and its transitions."""

    # Convert enum to states
    states = States.from_enum(
        SatelliteState,
        initial=SatelliteState.NEW,
        final=SatelliteState.DEAD,
        use_enum_instance=True,
    )

    # Define transitions
    # - NEW <=> INIT
    initialize = states.NEW.to(states.initializing) | states.INIT.to(states.initializing)
    initialize |= states.ERROR.to(states.initializing)
    initialize |= states.SAFE.to(states.initializing)

    initialized = states.initializing.to(states.INIT)

    # - INIT <=> ORBIT
    launch = states.INIT.to(states.launching)
    launched = states.launching.to(states.ORBIT)

    land = states.ORBIT.to(states.landing)
    landed = states.landing.to(states.INIT)

    # - ORBIT <=> RUN
    start = states.ORBIT.to(states.starting)
    started = states.starting.to(states.RUN)

    stop = states.RUN.to(states.stopping)
    stopped = states.stopping.to(states.ORBIT)

    reconfigure = states.ORBIT.to(states.reconfiguring)
    reconfigured = states.reconfiguring.to(states.ORBIT)

    # - safe
    interrupt = states.ORBIT.to(states.interrupting) | states.RUN.to(states.interrupting)
    interrupted = states.interrupting.to(states.SAFE)

    # - error
    failure = states.RUN.to(states.ERROR) | states.ORBIT.to(states.ERROR)
    failure |= states.INIT.to(states.ERROR) | states.SAFE.to(states.ERROR)
    failure |= states.ERROR.to(states.ERROR) | states.NEW.to(states.ERROR)

    failure |= states.initializing.to(states.ERROR)
    failure |= states.launching.to(states.ERROR)
    failure |= states.landing.to(states.ERROR)
    failure |= states.reconfiguring.to(states.ERROR)
    failure |= states.starting.to(states.ERROR)
    failure |= states.stopping.to(states.ERROR)
    failure |= states.interrupting.to(states.ERROR)

    # complete a transitional state
    complete = initialized | launched | landed | started
    complete |= stopped | reconfigured | interrupted

    # final state
    shutdown = states.INIT.to(states.DEAD)
    shutdown |= states.ERROR.to(states.DEAD)
    shutdown |= states.SAFE.to(states.DEAD)

    def __init__(self) -> None:
        # current status (i.e. state description)
        self.status = "Satellite not initialized yet."
        # flag indicated a finished state transition;
        # used by (and acknowledged by) Heartbeater.
        self.transitioned = False
        # timestamp for last state change
        self.last_changed = datetime.now(timezone.utc)
        super().__init__()

    def before_transition(self, status: str) -> None:
        """Set status before the state change."""
        self.status = status

    def after_transition(self) -> None:
        """Set flag indicating state change."""
        self.transitioned = True
        self.last_changed = datetime.now(timezone.utc)

    def write_diagram(self, filename: str) -> None:
        """Create a png with the FSM schematic."""
        from statemachine.contrib.diagram import DotGraphMachine

        graph = DotGraphMachine(self)
        dot = graph()
        dot.write_png(filename)


class SatelliteStateHandler(BaseSatelliteFrame):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)

        # instantiate state machine
        self.fsm = SatelliteFSM()

        # (transitional) state executor and event
        self._state_thread_evt: Event | None = None
        self._state_thread_exc: ThreadPoolExecutor = ThreadPoolExecutor(max_workers=1)
        self._state_thread_fut: Future | None = None  # type: ignore[type-arg]

    @debug_log
    @cscp_requestable
    def initialize(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate 'initialize' state transition via a CSCP request.

        Takes dictionary with configuration values as argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        if not isinstance(request.payload, dict):
            # missing payload
            raise TypeError("Payload must be a dictionary with configuration values")
        return self._transition("initialize", request, thread=False)

    @debug_log
    @cscp_requestable
    def launch(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate launch state transition via a CSCP request.

        No payload argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        return self._transition("launch", request, thread=False)

    @debug_log
    @cscp_requestable
    def land(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate landing state transition via a CSCP request.

        No payload argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        return self._transition("land", request, thread=False)

    @debug_log
    @cscp_requestable
    def start(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate start state transition via a CSCP request.

        Payload: run identifier [str].

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        if not isinstance(request.payload, str):
            # missing/wrong payload
            raise TypeError("Payload must be a run identification string")
        # Check that the run identifier is valid:
        if not re.match(r"^[\w-]+$", request.payload):
            raise ValueError("Run identifier contains invalid characters")
        return self._transition("start", request, thread=True)

    @debug_log
    @cscp_requestable
    def stop(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate stop state transition via a CSCP request.

        No payload argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        # NOTE This transition must not be threaded as it is intended to stop
        # the acquisition thread (which does not stop on its own). If
        # thread=True then it would be added as another worker thread and
        # potentially never started.
        return self._transition("stop", request, thread=False)

    @debug_log
    @cscp_requestable
    def reconfigure(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate reconfigure state transition via a CSCP request.

        Takes dictionary with configuration values as argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        if not hasattr(self, "do_reconfigure"):
            raise NotImplementedError("Reconfigure not supported: missing function 'do_reconfigure'")
        if not isinstance(request.payload, dict):
            # missing payload
            raise TypeError("Payload must be a dictionary with configuration values")
        return self._transition("reconfigure", request, thread=False)

    @debug_log
    @cscp_requestable
    def interrupt(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Initiate interrupt state transition via a CSCP request.

        No payload argument.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        return self._transition("interrupt", request, thread=False)

    @debug_log
    @cscp_requestable
    def failure(self, request: CSCPMessage) -> Tuple[str, str, None]:
        """Enter error state transition via a CSCP request.

        No payload argument.

        This is intended for debugging purposes only and should not be called in
        normal operation.

        """
        return self._transition("failure", request, thread=False)

    def _transition(self, target: str, request: CSCPMessage, thread: bool) -> Tuple[str, str, None]:
        """Prepare and enqeue a transition task.

        The task consists of the respective transition method and the request
        payload as argument.

        If thread is true, then the transition method will be run as a separate
        thread. If allowed by the FSM, threads can potentially be interrupted by
        a subsequent task such as 'RUN' can be stopped via the 'stop' command.

        Otherwise, the transition will be run in the main satellite thread and
        subsequent tasks are queued and blocked until the original transition
        has finished.

        """
        # call FSM transition, will throw exception if not allowed
        self.log.debug("State transition %s requested", target)
        getattr(self.fsm, target)(f"{target.capitalize()} called via CSCP request.")
        self.log.info("State transition %s initiated.", target)
        transit_fcn = getattr(self, f"_wrap_{target}")
        # add to the task queue to run from the main thread
        if thread:
            # task will be run in a separate thread
            self.task_queue.put((self._start_transition_thread, [transit_fcn, request.payload]))
        else:
            # task will be executed within the main satellite thread
            self.task_queue.put((self._start_transition, [transit_fcn, request.payload]))
        return "transitioning", target, None

    @debug_log
    def _start_transition(self, fcn: Callable[[Any], str], payload: Any) -> None:
        """Start a transition and advance FSM for transitional states."""
        res = fcn(payload)
        if not res:
            res = "Transition completed!"
        # try to advance the FSM for finishing transitional states
        try:
            prev = self.fsm.current_state_value.name
            self.fsm.complete(res)
            now = self.fsm.current_state_value.name
            self.log.info(f"State transition to steady state completed ({prev} -> {now}).")
        except TransitionNotAllowed:
            if self.fsm.current_state_value != SatelliteState.ERROR:
                # no need to do more than set the status, we are in a steady
                # operational state
                self.fsm.status = res

    @debug_log
    def _start_transition_thread(self, fcn: Callable[[Any], str], payload: Any) -> None:
        """Start a transition thread with the given fcn and arguments."""
        self._state_thread_evt = Event()
        self._state_thread_fut = self._state_thread_exc.submit(fcn, payload)
        # add a callback triggered when transition is complete
        self._state_thread_fut.add_done_callback(self._state_transition_thread_complete)

    @handle_error
    def _state_transition_thread_complete(self, fut: Future) -> None:  # type: ignore[type-arg]
        """Callback method when a transition thread is done."""
        self.log.trace("Transition thread completed and callback received.")
        # Get the thread's return value. This raises any exception thrown in the
        # thread, which will be handled by the @handle_error decorator to put us
        # into ERROR state.
        res = fut.result()
        if not res:
            res = "Transition completed!"
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_evt, Event), "Thread transition Event not set up correctly"
        if self._state_thread_evt.is_set():
            # Cancelled; do not advance state. This handles stopping RUN state
            # and avoids premature progression out of STOPPING
            self._state_thread_evt = None
            return
        # cleanup
        self._state_thread_evt = None
        # try to advance the FSM for finishing transitional states
        try:
            prev = self.fsm.current_state_value.name
            self.fsm.complete(res)
            now = self.fsm.current_state_value.name
            self.log.info(f"State transition to steady state completed ({prev} -> {now}).")
        except TransitionNotAllowed:
            if self.fsm.current_state_value != SatelliteState.ERROR:
                # no need to do more than set the status, we are in a steady
                # operational state
                self.fsm.status = res

    @cscp_requestable
    def get_state(self, _request: CSCPMessage | None = None) -> Tuple[str, int, dict[str, Any]]:
        """Return the current state of the Satellite.

        No payload argument.

        Payload of the response contains 'last_changed'
        """
        payload = self.fsm.current_state_value.value
        meta = {
            "last_changed": Timestamp.from_datetime(self.fsm.last_changed),
            "last_changed_iso": self.fsm.last_changed.isoformat(),
        }
        return self.fsm.current_state_value.name, payload, meta

    @cscp_requestable
    def get_status(self, _request: CSCPMessage | None = None) -> Tuple[str, None, None]:
        """Get a string describing the current status of the Satellite.

        No payload argument.

        """
        return self.fsm.status, None, None
