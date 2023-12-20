#!/usr/bin/env python3
"""This module provides a base class for a Constellation Satellite."""

from functools import wraps
import traceback
import time
import threading
import logging

import zmq
import msgpack
from statemachine.exceptions import TransitionNotAllowed

from .fsm import SatelliteFSM, SatelliteResponse
from .heartbeater import Heartbeater
from .heartbeatchecker import HeartbeatChecker
from .log_and_stats import DataType, getLoggerAndStats
from ._version import version

def handle_error(func):
    """Catch and handle exceptions in function calls."""
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        try:
            return func(self, *args, **kwargs)
        except TransitionNotAllowed as exc:
            err_msg = f"Unable to execute {func.__name__} to {exc.event}: "
            err_msg += f"Not possible in {exc.state.name} state."
            raise RuntimeError(err_msg) from exc
        except Exception as exc:
            err_msg = f"Unable to execute {func.__name__}: {exc}"
            self.logger.error(err_msg + traceback.format_exc())
            raise RuntimeError(err_msg) from exc
    return wrapper


def debug_log(func):
    """Add debug messages to function call."""
    @wraps(func)
    def wrapper(self, *args, **kwargs):
        self.logger.debug(f"-> Entering {func.__name__} with args: {args}")
        output = func(self, *args, **kwargs)
        self.logger.debug(f"<- Exiting {func.__name__} with output: {output}")
        return output
    return wrapper


class IncompleteCommand(Exception):
    pass


class Satellite:
    """Base class for a Constellation Satellite."""

    def __init__(self, name: str, cmd_port: int, hb_port: int, log_port: int):
        """Set up class attributes."""
        self.name = name
        self.context = zmq.Context()

        # set up python logging
        self.logger, self.stats = getLoggerAndStats(self.name, self.context, log_port)

        # state machine
        self.fsm = SatelliteFSM()
        # Adding this class as observer allows class-internal methods such as
        # on_start to be Actions to be performed on a Transition of the state
        # machine. This will ensure that the state does not change before we
        # have performed all necessary steps.
        self.fsm.add_observer(self)

        # set up the command channel
        self.cmd_sock = self.context.socket(zmq.REP)
        self.cmd_sock.bind(f"tcp://*:{cmd_port}")
        self.logger.info(f"Satellite listening on command port {cmd_port}")

        # register and start heartbeater
        self.heartbeater = Heartbeater(self.get_state,
                                       f"tcp://*:{hb_port}",
                                       context=self.context)
        self.heartbeater.start()

        # register heartbeat checker
        self.hb_checker = HeartbeatChecker()

        # acquisition thread
        self._stop_running = None
        self._running_thread = None

        # Add exception handling via threading.excepthook to allow the state
        # machine to reflect exceptions in the receiver thread.
        #
        # NOTE: This approach using a global state hook does not play well e.g.
        # with the default pytest configuration, however (see
        # https://github.com/pytest-dev/pytest/discussions/9193). Without
        # disabling the threadexception plugin, tests of the exception handling
        # will fail with pytest.
        threading.excepthook = self._thread_exception
        # greet
        self.logger.info(f"Satellite {self.name}, version {self.version} ready to launch!")

    def run_satellite(self):
        """Main command handler-routine"""
        while True:

            # TODO: add check for heartbeatchecker: if any entries in hb.get_failed, trigger action

            i = 0 # TODO remove this
            try:
                cmdmsg = self.cmd_sock.recv_multipart(flags=zmq.NOBLOCK)
            except zmq.ZMQError:
                # TODO remove this vvvvv
                self.logger.debug("Received no msg via ZMQ, sleeping... [example for frequent debug msg]")
                i += 1
                self.stats.sendStats("tries", DataType.INT, i)
                # TODO remove this ^^^^
                time.sleep(0.1)
                continue
            except KeyboardInterrupt:
                break

            # prepare response header:
            rhead = {'time': time.time(), 'sender': "FIXME" }
            rd = msgpack.packb(rhead)

            try:
                cmd = cmdmsg[0].decode('UTF-8')
                self.logger.info(f'Received CMD "{cmd}"')

                header = msgpack.unpackb(cmdmsg[1])
                self.logger.info(f'Header: {header}')

                if cmd.lower() == "get_state":
                    self.cmd_sock.send_string(str(SatelliteResponse.SUCCESS), flags=zmq.SNDMORE)
                    self.cmd_sock.send(rd, flags=zmq.SNDMORE)
                    self.cmd_sock.send(msgpack.packb({'state': self.get_state()}))

                elif cmd.lower().startswith("transition"):
                    target = cmd.split()[1].capitalize()
                    # try to call the corresponding state change method
                    # NOTE: this even allows to change state to Failure
                    try:
                        if len(cmdmsg) <= 2:
                            getattr(self, target)()
                        else:
                            # TODO unpack/parse the remaining parameters, if needed
                            getattr(self, target)(cmdmsg[2:])
                    except AttributeError:
                        raise RuntimeError(f"Unknown state '{target}'")
                    except Exception as e:
                        raise RuntimeError(
                            f"State change to '{target}' with {len(cmdmsg)-1} arguments caused exception {e}"
                        )
                    self.cmd_sock.send_string(str(SatelliteResponse.SUCCESS), flags=zmq.SNDMORE)
                    self.cmd_sock.send(rd)
                elif cmd.lower().startswith("register"):
                    try:
                        _, name, ip, port = cmd.split()
                        self.hb_checker.register(name, f"tcp://{ip}:{port}", self.context)
                        self.cmd_sock.send_string("OK")
                    except Exception as exc:
                        err_msg = f"Unable to register heartbeat in '{cmd}': {exc}"
                        self.cmd_sock.send_string(err_msg)
                else:
                    raise RuntimeError(f"Unknown command '{cmd}'")

            except IncompleteCommand as err:
                self.cmd_sock.send_string(str(SatelliteResponse.INCOMPLETE), flags=zmq.SNDMORE)
                self.cmd_sock.send(rd, flags=zmq.SNDMORE)
                self.cmd_sock.send(msgpack.packb({'message': f"{err}"}))

            except Exception as exc:
                self.cmd_sock.send_string(str(SatelliteResponse.INVALID), flags=zmq.SNDMORE)
                self.cmd_sock.send(rd, flags=zmq.SNDMORE)
                self.cmd_sock.send(msgpack.packb({'message': f"Unable to execute {cmd}: {exc}"}))

            time.sleep(1)
        # on exit: stop heartbeater
        self.heartbeater.stop()

    # --------------------------- #
    # ----- satellite commands ----- #
    # --------------------------- #

    @handle_error
    @debug_log
    def Load(self):
        """Initialize.

        Actual actions will be performed by the callback method 'on_load'
        as long as the transition is allowed.
        """
        self.fsm.load()
        self.logger.info( "Satellite Initialized.")

    @handle_error
    def on_load(self):
        """Callback method for the 'initialize' transition of the FSM.

        Set and check config, maybe initialize device.
        """
        pass

    @handle_error
    @debug_log
    def Unload(self):
        """Deinitialize Satellite.

        Actual Satellite-specific actions will be performed by the callback
        method 'on_unload' as long as the transition is allowed.

        """
        self.fsm.unload()
        self.logger.info( "Satellite Deinitialized.")

    @handle_error
    @debug_log
    def on_unload(self):
        """Callback method for the 'deinitialize' transition of the FSM.

        Undo actions from 'do_initialize'
        """
        pass

    @handle_error
    @debug_log
    def Launch(self):
        """Prepare Satellite for data acquistions.

        Actual actions will be performed by the callback method 'on_launch'
        aslong as the transition is allowed.
        """
        self.fsm.launch()
        self.hb_checker.start()

        self.logger.info( "Satellite Prepared. Acquistion ready.")

    @handle_error
    def on_launch(self):
        """Callback method for the 'launch' transition of the FSM.
        """
        pass

    @handle_error
    @debug_log
    def Land(self):
        """Return Satellite to Initialized state.

        Actual actions will be performed by the callback method 'on_land'
        aslong as the transition is allowed.
        """
        self.fsm.land()
        self.hb_checker.stop()
        self.logger.info( "Satellite Unprepared. Back in Initialized state.")

    @handle_error
    def on_land(self):
        """Callback method for the 'unprepare' transition of the FSM.
        """
        pass

    @handle_error
    @debug_log
    def Start(self):
        """Start data acquisition.

        Actual Satellite-specific actions will be performed by the callback
        method 'on_start' as long as the transition is allowed.

        """
        self.fsm.start()
        # start thread running during acquistion
        self._stop_running = threading.Event()
        self._running_thread = threading.Thread(target=self.do_run)
        self._running_thread.start()
        self.logger.info( "Satellite Running. Acquistion taking place.")

    @handle_error
    def on_start(self):
        """Callback method for the 'start_run' transition of the FSM.
        """
        pass

    @handle_error
    @debug_log
    def Stop(self):
        """Stop data acquisition.

        Actual actions will be performed by the callback method 'on_stop_run'
        aslong as the transition is allowed.
        """
        self.fsm.stop()
        self.logger.info( "Satellite stopped Acquistion.")

    @handle_error
    def on_stop(self):
        """Callback method for the 'stop_run' transition of the FSM.

        This method needs to the stop acquisition on the other thread.
        """

    def do_run(self):
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
        while not self._stop_running.is_set():
            time.sleep(0.2)

    @handle_error
    @debug_log
    def Failure(self):
        """Failure on Satellite.

        Actual actions will be performed by the callback method 'on_failure'
        aslong as the transition is allowed.
        """
        # Stop running if running
        self._stop_running.set()
        if self._running_thread and self._running_thread.is_alive():
            # TODO select a sensible timeout value, add a warning if not succeeded
            self._running_thread.join(5.0)
        # Stop heartbeat checking
        self.hb_checker.stop()
        self.fsm.failure()
        self.logger.error("There is a failure.")

    @handle_error
    def on_failure(self):
        """Callback method for the 'on_failure' transition of the FSM

        This method should implement Satellite-specific actions in case of
        failure.

        """
        pass

    @handle_error
    @debug_log
    def Interrupt(self):
        """Interrupt data acquisition and move to Safe state.

        Actual actions will be performed by the callback method 'on_interrupt'
        aslong as the transition is allowed.
        """
        self.fsm.interrupt()
        # Stop running if running
        self._stop_running.set()
        if self._running_thread and self._running_thread.is_alive():
            # TODO consider to set a timeout and fail if device takes too long
            self._running_thread.join()
        self.logger.warning("Transitioned to Safe state.")

    @handle_error
    def on_interrupt(self):
        """Callback method for the 'on_interrupt' transition of the FSM.

        This method needs to the stop acquisition on the other thread.
        """
        pass

    @handle_error
    @debug_log
    def Recover(self):
        """Transition to Initialized state.

        Actual actions will be performed by the callback method 'on_recover'
        aslong as the transition is allowed.
        """
        self.fsm.recover()
        self.hb_checker.stop()
        self.logger.info( "Recovered from Safe state.")

    @handle_error
    def on_recover(self):
        """Callback method for the 'on_recover' transition of the FSM.
        """
        pass

    @handle_error
    @debug_log
    def Reset(self):
        """Transition to Idle state from Error state.

        Actual actions will be performed by the callback method 'on_reset'
        aslong as the transition is allowed.
        """
        self.fsm.reset()
        self.logger.info( "Reset from Error state. Back in Idle state.")

    @handle_error
    def on_reset(self):
        """Callback method for the 'on_reset' transition of the FSM.
        """
        pass

    def _thread_exception(self, args):
        """Handle exceptions in threads.

        Change state to FAULT.

        Intended to be installed as threading.excepthook.

        """
        tb = "".join(traceback.format_tb(args.exc_traceback))
        self.logger.fatal(
            f"caught {args.exc_type} with value \
            {args.exc_value} in thread {args.thread} and traceback {tb}."
        )
        # indicate to remaining thread to stop
        if self._stop_running:
            self._stop_running.set()
        # change internal state
        self.fsm.failure(
            f"Thread {args.thread} failed. Caught exception {args.exc_type} \
            with value {args.exc_value}."
        )

    # -------------------------- #
    # ----- device methods ----- #
    # -------------------------- #

    def get_state(self) -> str:
        return self.fsm.current_state.id

    @property
    def version(self):
        """Get Constellation version."""
        return version


# -------------------------------------------------------------------------


def main(args=None):
    """Start the base Satellite server."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--log-port", type=int, default=5556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--name", type=str, default="satellite_demo")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger()
    formatter = logging.Formatter('%(asctime)s | %(name)s |  %(levelname)s: %(message)s')
    # global level should be the lowest level that we want to see on any
    # handler, even streamed via ZMQ
    logger.setLevel(0)

    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Satellite(args.name, args.cmd_port, args.hb_port, args.log_port)
    s.run_satellite()


if __name__ == "__main__":
    main()
