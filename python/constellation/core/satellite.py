#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""

import time
from queue import Empty
from typing import Any
import threading
import traceback
from concurrent.futures import Future

from .fsm import SatelliteState
from . import __version__
from .heartbeater import HeartbeatSender
from .heartbeatchecker import HeartbeatChecker

from .cscp import CSCPMessage
from .chirp import CHIRPServiceIdentifier
from .broadcastmanager import CHIRPBroadcaster
from .commandmanager import CommandReceiver, cscp_requestable
from .configuration import ConfigError, Configuration, make_lowercase
from .monitoring import MonitoringSender
from .error import debug_log, handle_error
from .base import EPILOG, ConstellationArgumentParser, setup_cli_logging


class Satellite(
    CommandReceiver,
    CHIRPBroadcaster,
    MonitoringSender,
    HeartbeatSender,
):
    """Base class for a Constellation Satellite."""

    def __init__(
        self,
        name: str,
        group: str,
        cmd_port: int,
        hb_port: int,
        mon_port: int,
        interface: str,
    ):
        """Set up class attributes."""
        super().__init__(
            name=name,
            group=group,
            cmd_port=cmd_port,
            hb_port=hb_port,
            mon_port=mon_port,
            interface=interface,
        )

        self.run_identifier: str = ""
        self.config = Configuration({})

        # give monitoring a chance to start up and catch early messages
        time.sleep(0.1)

        # set up background communication threads
        super()._add_com_thread()
        super()._start_com_threads()

        # register heartbeat checker
        self.hb_checker = HeartbeatChecker()

        # register broadcast manager
        self.register_offer(CHIRPServiceIdentifier.CONTROL, self.cmd_port)
        self.register_offer(CHIRPServiceIdentifier.HEARTBEAT, self.hb_port)
        self.register_offer(CHIRPServiceIdentifier.MONITORING, self.mon_port)
        self.broadcast_offers()

        # Add exception handling via threading.excepthook to allow the state
        # machine to reflect exceptions in the communication services threads.
        #
        # NOTE: This approach using a global state hook does not play well e.g.
        # with the default pytest configuration, however (see
        # https://github.com/pytest-dev/pytest/discussions/9193). Without
        # disabling the threadexception plugin, tests of the exception handling
        # will fail with pytest.
        threading.excepthook = self._thread_exception
        # greet
        self.log.info(f"Satellite {self.name}, version {__version__} ready to launch!")

    @debug_log
    @cscp_requestable
    def register(self, request: CSCPMessage) -> tuple[str, Any, dict[str, Any]]:
        """Register a heartbeat via CSCP request."""
        name, ip, port = request.payload.split()
        callback = self.hb_checker.register
        # add to the task queue
        self.task_queue.put((callback, [name, f"tcp://{ip}:{port}", self.context]))
        return "registering", name, {}

    def run_satellite(self) -> None:
        """Main Satellite event loop with task handler-routine.

        This routine sequenctially executes tasks queued by the CommandReceiver
        or the CHIRPBroadcaster. These tasks come in the form of callbacks to
        e.g. state transitions.

        """
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            # TODO: add check for heartbeatchecker: if any entries in hb.get_failed, trigger action
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    # TODO consider whether to go into error state if anything goes wrong here
                    self.log.exception(
                        "Caught exception handling task '%s' with args '%s': %s",
                        callback,
                        args,
                        repr(e),
                    )
            except Empty:
                # nothing to process
                pass
            except KeyboardInterrupt:
                self.log.warning("Satellite caught KeyboardInterrupt, shutting down.")
                # time to shut down
                break
            time.sleep(0.01)

    def reentry(self) -> None:
        """Orderly shutdown and destroy the Satellelite."""
        # can only exit from certain state, go into ERROR if not the case
        self.log.info("Satellite on reentry course for self-destruction.")
        if self.fsm.current_state_value not in [
            SatelliteState.NEW,
            SatelliteState.INIT,
            SatelliteState.SAFE,
            SatelliteState.ERROR,
        ]:
            self.fsm.failure("Performing controlled re-entry and self-destruction.")
            self._wrap_failure()
        super().reentry()

    # --------------------------- #
    # ----- satellite commands ----- #
    # --------------------------- #

    @handle_error
    @debug_log
    def _wrap_initialize(self, config: dict[str, Any]) -> str:
        """Wrapper for the 'initializing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # Verify that there are no running threads left. If there are and the
        # timeout is exceeded joining them, the raised TimeoutError exception
        # will take us into ERROR state.
        try:
            self._state_thread_evt.set()  # type: ignore[union-attr]
            self._state_thread_fut.result(2)  # type: ignore[union-attr]
        except AttributeError:
            # no threads left
            pass
        self._state_thread_evt = None
        self._state_thread_fut = None
        # prepare configuration
        config = make_lowercase(config)
        self.config = Configuration(config)
        # call device-specific user-routine
        try:
            init_msg: str = self.do_initializing(self.config)
        except ConfigError as e:
            msg = "Caught exception during initialization: "
            msg += f"missing a required configuration value {e}?"
            self.log.error(msg)
            raise RuntimeError(msg) from e
        if self.config.has_unused_values():
            for key in self.config.get_unused_keys():
                self.log.warning("Satellite ignored configuration value: '%s'", key)
            init_msg += " IGNORED parameters: "
            init_msg += ",".join(self.config.get_unused_keys())
        return init_msg

    @debug_log
    def do_initializing(self, config: Configuration) -> str:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """
        return "Initialized."

    @handle_error
    @debug_log
    def _wrap_launch(self, payload: Any) -> str:
        """Wrapper for the 'launching' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.hb_checker.start_all()
        return str(self.do_launching())

    @debug_log
    def do_launching(self) -> str:
        """Prepare Satellite for data acquisitions."""
        return "Launched."

    @handle_error
    @debug_log
    def _wrap_reconfigure(self, partial_config_dict: dict[str, Any]) -> str:
        """Wrapper for the 'reconfigure' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """

        partial_config = Configuration(partial_config_dict)

        # reconfigure is not necessarily implemented; it is not in the this base
        # class to allow checking for the exististance of the method to
        # determine the reaction to a `reconfigure` CSCP command.
        init_msg: str = self.do_reconfigure(partial_config)  # type: ignore[attr-defined]

        # update config
        self.config.update(partial_config_dict, partial_config.get_unused_keys())

        if partial_config.has_unused_values():
            for key in partial_config.get_unused_keys():
                self.log.warning("Satellite ignored configuration value: '%s'", key)
            init_msg += " IGNORED parameters: "
            init_msg += ",".join(self.config.get_unused_keys())
        return init_msg

    @handle_error
    @debug_log
    def _wrap_land(self, payload: Any) -> str:
        """Wrapper for the 'landing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.hb_checker.stop()
        res: str = self.do_landing()
        return res

    @debug_log
    def do_landing(self) -> str:
        """Return Satellite to Initialized state."""
        return "Landed."

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> str:
        """Wrapper for the 'stopping' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
        # wait for result, waiting until done
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_fut, Future)
        res_run: str = self._state_thread_fut.result(timeout=None)
        self.log.debug("RUN thread finished, continue with STOPPING.")
        res: str = self.do_stopping()
        return f"{res_run}; {res}"

    @debug_log
    def do_stopping(self) -> str:
        """Stop the data acquisition."""
        return "Acquisition stopped."

    @handle_error
    @debug_log
    def _wrap_start(self, run_identifier: str) -> str:
        """Wrapper for the 'run' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.run_identifier = run_identifier
        self.log.info(f"Starting run '{run_identifier}'")
        res: str = self.do_starting(run_identifier)
        # complete transitional state
        self.fsm.complete(res)
        # continue to execute DAQ in this thread
        res = self.do_run(run_identifier)
        return res

    @debug_log
    def do_starting(self, run_identifier: str) -> str:
        """Final preparation for acquisition."""
        return "Finished preparations, starting."

    @debug_log
    def do_run(self, run_identifier: str) -> str:
        """The acquisition event loop.

        This method will be started by the Satellite and run in a thread. It
        therefore needs to monitor the self.stop_running Event and close itself
        down if the Event is set.

        NOTE: This method is not inherently thread-safe as it runs in the
        context of the Satellite and can modify data accessible to the main
        thread. However, the state machine can effectively act as a lock and
        prevent competing access to the same objects while in RUNNING state as
        long as care is taken in the implementation.

        The state machine itself uses the RTC model by default (see
        https://python-statemachine.readthedocs.io/en/latest/processing_model.html?highlight=thread)
        which should make the transitions themselves safe.

        """
        # the stop_running Event will be set from outside the thread when it is
        # time to close down.
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_evt, threading.Event), "Transition thread Event not set up correctly"
        while not self._state_thread_evt.is_set():
            time.sleep(0.2)
        return "Finished acquisition."

    @debug_log
    def _wrap_failure(self, *_args: Any, **_kwargs: Any) -> str:
        """Wrapper for the 'ERROR' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        try:
            # Stop heartbeat checking
            self.hb_checker.stop()
            # stop state thread
            if self._state_thread_evt:
                self._state_thread_evt.set()
                if self._state_thread_fut:
                    try:
                        self._state_thread_fut.result(timeout=1)
                    except TimeoutError:
                        self.log.error("Timeout while joining state thread, continuing.")
            res: str = self.fail_gracefully()
            # close heartbeat checker
            self.hb_checker.close()
            return res
        # NOTE: we cannot have a non-handled exception disallow the state
        # transition to failure state!
        except Exception as e:
            self.log.exception(e)
            return "Exception caught during failure handling, see logs for details."

    @debug_log
    def fail_gracefully(self) -> str:
        """Method called when reaching 'ERROR' state."""
        return "Failed gracefully."

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        """Wrapper for the 'interrupting' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        res_run: str = ""
        if self._state_thread_evt:
            self._state_thread_evt.set()
            # wait for result, will block until user code finishes
            # assert for mypy static type analysis
            assert isinstance(self._state_thread_fut, Future)
            res_run = self._state_thread_fut.result(timeout=None)
            self._state_thread_evt = None
        self.log.debug("RUN thread finished, continue with INTERRUPTING.")
        self.hb_checker.stop()
        res: str = self.do_interrupting()
        return f"{res_run}; {res}"

    @debug_log
    def do_interrupting(self) -> str:
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
        self.do_stopping()
        self.do_landing()
        return "Interrupted."

    def _thread_exception(self, args: Any) -> None:
        """Handle exceptions in threads.

        Change state to FAULT.

        Intended to be installed as threading.excepthook.

        """
        tb = "".join(traceback.format_tb(args.exc_traceback))
        self.log.fatal(
            f"caught {args.exc_type} with value \
            {args.exc_value} in thread {args.thread} and traceback {tb}."
        )
        self._wrap_failure()
        # change internal state
        self.fsm.failure(
            f"Thread {args.thread} failed. Caught exception {args.exc_type} \
            with value {args.exc_value}."
        )

    # -------------------------- #
    # ----- device methods ----- #
    # -------------------------- #

    @cscp_requestable
    def get_version(self, _request: CSCPMessage | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get Constellation version.

        No payload argument.

        Additional version information may be included in the meta map (final
        return value).

        """
        return __version__, None, {}

    @cscp_requestable
    def get_run_id(self, _request: CSCPMessage | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get current/last known run identifier.

        No payload argument.

        """
        return self.run_identifier, None, {}

    @cscp_requestable
    def get_config(self, _request: CSCPMessage | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get current satellite configuration.

        No payload argument.

        """
        cfg_dict = self.config.get_dict()
        return f"{len(cfg_dict)} configuration keys, dictionary attached in payload", cfg_dict, {}


# -------------------------------------------------------------------------


class SatelliteArgumentParser(ConstellationArgumentParser):
    """Customized Argument parser providing common Satellite options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--cmd-port",
            "--command-port",
            "--cscp",
            type=int,
            help="The port to listen on for commands sent via the "
            "Constellation Satellite Control Protocol (default: %(default)s).",
        )
        self.network.add_argument(
            "--mon-port",
            "--monitoring-port",
            "--cmdp",
            type=int,
            help="The port to provide data via the "
            "Constellation Monitoring Distribution Protocol (default: %(default)s).",
        )
        self.network.add_argument(
            "--hb-port",
            "--heartbeat-port",
            "--chp",
            type=int,
            help="The port for sending heartbeats via the " "Constellation Heartbeat Protocol (default: %(default)s).",
        )


def main(args: Any = None) -> None:
    """Start a Demo Satellite server.

    This Satellite only implements rudimentary functionality but can be used to
    test basic communication.

    """
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = setup_cli_logging(args["name"], args.pop("log_level"))

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Satellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
