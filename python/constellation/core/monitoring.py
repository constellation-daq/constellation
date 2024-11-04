"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import logging
import zmq
import threading
import os
import pathlib
from queue import Empty
from functools import wraps
from datetime import datetime
from typing import Callable, cast, ParamSpec, TypeVar, Any
from queue import Queue
from logging.handlers import QueueHandler, QueueListener

from .base import (
    BaseSatelliteFrame,
    ConstellationArgumentParser,
    EPILOG,
    setup_cli_logging,
)
from .cmdp import CMDPTransmitter, Metric, MetricsType
from .chirp import CHIRPServiceIdentifier
from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService


P = ParamSpec("P")
B = TypeVar("B", bound=BaseSatelliteFrame)


def schedule_metric(unit: str, handling: MetricsType, interval: float) -> Callable[[Callable[P, Any]], Callable[P, Metric]]:
    """Schedule a function for callback at interval [s] and send Metric.

    The function should take no arguments and return a value [any]
    """

    def decorator(func: Callable[P, Any]) -> Callable[P, Metric]:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> Metric:
            val = func(*args, **kwargs)
            return Metric(
                name=func.__name__,
                unit=unit,
                handling=handling,
                value=val,
            )

        # mark function as chirp callback
        wrapper.metric_scheduled = interval  # type: ignore[attr-defined]
        return wrapper

    return decorator


def get_scheduled_metrics(cls: object) -> dict[str, dict[str, Any]]:
    """Loop over all class methods and return those marked as metric."""
    res = {}
    for func in dir(cls):
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "metric_scheduled") and hasattr(call, "__name__"):
                res[call.__name__] = {"function": call, "interval": getattr(call, "metric_scheduled")}
    return res


class MonitoringSender(BaseSatelliteFrame):
    """Sender mixin class for Constellation Monitoring Distribution Protocol.

    Any method of inheriting classes that has the @schedule_metric decorator,
    will be regularly polled for new values and a corresponding Metric be sent
    on the monitoring port.

    """

    def __init__(self, name: str, mon_port: int, interface: str, **kwds: Any):
        """Set up logging and metrics transmitters."""
        super().__init__(name=name, interface=interface, **kwds)

        # Create monitoring socket and bind interface
        socket = self.context.socket(zmq.PUB)
        if not mon_port:
            self.mon_port = socket.bind_to_random_port(f"tcp://{interface}")
        else:
            socket.bind(f"tcp://{interface}:{mon_port}")
            self.mon_port = mon_port

        self._mon_tm = CMDPTransmitter(self.name, socket)

        # Set up ZMQ logging
        # ROOT logger needs to have a level set (initializes with level=NOSET)
        # The root level should be the lowest level that we want to see on any
        # handler, even streamed via ZMQ.
        logger = logging.getLogger()
        logger.setLevel("DEBUG")
        # NOTE: Logger object is a singleton and setup is only necessary once
        # for the given name.
        self._zmq_log_handler = ZeroMQSocketLogHandler(self._mon_tm)
        self.log.addHandler(self._zmq_log_handler)

        # dict to keep scheduled intervals for fcn polling
        self._metrics_callbacks = get_scheduled_metrics(self)

    def schedule_metric(
        self,
        name: str,
        unit: str,
        handling: MetricsType,
        interval: float,
        callback: Callable[..., Any],
    ) -> None:
        """Schedule a callback at regular intervals.

        The callable needs to return a value [any] and a unit [str] and take no
        arguments. If you have a callable that requires arguments, consider
        using functools.partial to fill in the necessary information at
        scheduling time.

        """

        def wrapper() -> Metric:
            val = callback()
            return Metric(
                name=name,
                unit=unit,
                handling=handling,
                value=val,
            )

        self._metrics_callbacks[name] = {"function": wrapper, "interval": interval}

    def send_metric(self, metric: Metric) -> None:
        """Send a single metric via ZMQ."""
        self._mon_tm.send_metric(metric)

    def reset_scheduled_metrics(self) -> None:
        """Reset all previously scheduled metrics.

        Will only schedule metrics provided via decorator.

        """
        self._metrics_callbacks = get_scheduled_metrics(self)

    def _add_com_thread(self) -> None:
        """Add the metric sender thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_sender"] = threading.Thread(target=self._send_metrics, daemon=True)
        self.log.debug("Metric sender thread prepared and added to the pool.")

    def _send_metrics(self) -> None:
        """Metrics sender loop."""
        last_update: dict[str, datetime] = {}
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            for metric_name, param in self._metrics_callbacks.items():
                update = False
                try:
                    last = last_update[metric_name]
                    if (datetime.now() - last).total_seconds() > param["interval"]:
                        update = True
                except KeyError:
                    update = True
                if update:
                    try:
                        # Do not send if None type (e.g. currently unavailable)
                        metric = param["function"]()
                        if metric.value is not None:
                            self.send_metric(metric)
                        else:
                            self.log.debug(f"Not sending metric {metric_name}: currently None")
                    except Exception as e:
                        self.log.error(f"Could not retrieve metric {metric_name}: {repr(e)}")
                    last_update[metric_name] = datetime.now()

            time.sleep(0.1)
        self.log.info("Monitoring metrics thread shutting down.")
        # clean up
        self.close()

    def close(self) -> None:
        """Close the ZMQ socket."""
        self.log.removeHandler(self._zmq_log_handler)
        self._mon_tm.close()


class ZeroMQSocketLogHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPTransmitter):
        super().__init__(cast(Queue, transmitter))  # type: ignore[type-arg]

    def enqueue(self, record: logging.LogRecord) -> None:
        self.queue.send(record)  # type: ignore[attr-defined]

    def close(self) -> None:
        if not self.queue.closed:  # type: ignore[attr-defined]
            self.queue.close()  # type: ignore[attr-defined]


class ZeroMQSocketLogListener(QueueListener):
    """This listener receives messages from a CMDPTransmitter.

    NOTE that the corresponding socket should only subscribe to LOG messages!

    """

    def __init__(self, transmitter: CMDPTransmitter, /, *handlers: Any, **kwargs: Any):
        super().__init__(cast(Queue, transmitter), *handlers, **kwargs)  # type: ignore[type-arg]
        self._stop_recv = threading.Event()

    def dequeue(self, block: bool) -> logging.LogRecord:
        # FIXME it is quite likely that this blocking call causes errors when
        # shutting down as the ZMQ context is removed before this call ends.
        record = None
        while not record and not self._stop_recv.is_set():
            try:
                record = self.queue.recv()  # type: ignore[attr-defined]
            except zmq.ZMQError:
                pass
        if self._stop_recv.is_set():
            # close down
            return self._sentinel  # type: ignore[no-any-return, attr-defined]
        return cast(logging.LogRecord, record)

    def stop(self) -> None:
        """Close socket and stop thread."""
        super().stop()
        self.queue.close()  # type: ignore[attr-defined]

    def enqueue_sentinel(self) -> None:
        self._stop_recv.set()


class StatListener(CHIRPBroadcaster):
    """Simple listener class to receive metrics from a Constellation."""

    def __init__(self, name: str, group: str, interface: str, **kwds: Any):
        """Initialize values.

        Arguments:
        - name ::  name of this Monitor
        - group ::  group of controller
        - interface :: the interface to connect to
        """
        super().__init__(name=name, group=group, interface=interface, **kwds)

        # Set up the metric poller which will monitor all ZMQ metric subscription sockets
        self._metric_sockets: dict[str, zmq.Socket] = {}  # type: ignore[type-arg]
        self._metric_poller = zmq.Poller()
        self._metric_poller_lock = threading.Lock()

        self.request(CHIRPServiceIdentifier.MONITORING)

    def metric_callback(self, metric: Metric) -> None:
        """Metric callback."""
        self.log.debug(f"Received metric {metric.name} from {metric.sender}: {metric.value} {metric.unit}")

    @chirp_callback(CHIRPServiceIdentifier.MONITORING)
    def _add_satellite_callback(self, service: DiscoveredService) -> None:
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    def _add_satellite(self, service: DiscoveredService) -> None:
        address = "tcp://" + service.address + ":" + str(service.port)
        uuid = str(service.host_uuid)
        self.log.debug("Connecting to %s, address %s...", uuid, address)

        # create socket for metrics
        socket = self.context.socket(zmq.SUB)
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "STAT/")
        self._metric_sockets[uuid] = socket
        self._metric_poller.register(socket, zmq.POLLIN)

    def _remove_satellite(self, service: DiscoveredService) -> None:
        uuid = str(service.host_uuid)
        self.log.debug("Departure of %s.", service.host_uuid)
        try:
            with self._metric_poller_lock:
                socket = self._metric_sockets.pop(uuid)
                self._metric_poller.unregister(socket)
                socket.close()
        except KeyError:
            pass

    def _add_com_thread(self) -> None:
        """Add the metric receiver thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_receiver"] = threading.Thread(target=self._receive_metrics, daemon=True)
        self.log.debug("Metric receiver thread prepared and added to the pool.")

    def _receive_metrics(self) -> None:
        """Main loop to receive metrics."""
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"
        # set up transmitter for decoding metrics
        transmitter = CMDPTransmitter("", None)

        while not self._com_thread_evt.is_set():
            with self._metric_poller_lock:
                sockets_ready = dict(self._metric_poller.poll(timeout=250))
                if sockets_ready:
                    for socket in sockets_ready.keys():
                        binmsg = socket.recv_multipart()
                        metric = transmitter.decode_metric(binmsg[0].decode("utf-8"), binmsg)
                        self.metric_callback(metric)
                    continue
            # If no sockets are connected, the poller returns immediately -> sleep to prevent hot loop
            time.sleep(250e-3)

    def _metrics_listening_shutdown(self) -> None:
        with self._metric_poller_lock:
            for _uuid, socket in self._metric_sockets.items():
                self._metric_poller.unregister(socket)
                socket.close()
        self._metric_sockets = {}

    def reentry(self) -> None:
        self._metrics_listening_shutdown()
        super().reentry()

    def run_listener(self) -> None:
        self._add_com_thread()
        self._start_com_threads()
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            try:
                time.sleep(250e-3)
            except KeyboardInterrupt:
                self.log.warning("Listener caught KeyboardInterrupt, shutting down.")
                break


class MonitoringListener(StatListener):
    """Simple monitor class to receive logs and metrics from a Constellation."""

    def __init__(self, *args, **kwargs):  # type: ignore[no-untyped-def]
        super().__init__(*args, **kwargs)

        self._log_listeners: dict[str, ZeroMQSocketLogListener] = {}

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(target=self._run_task_handler, daemon=True)
        self._task_handler_thread.start()

    def _add_satellite(self, service: DiscoveredService) -> None:
        # add for metrics
        super()._add_satellite(service)
        # create socket for logging
        address = "tcp://" + service.address + ":" + str(service.port)
        uuid = str(service.host_uuid)
        socket = self.context.socket(zmq.SUB)
        # add timeout to avoid deadlocks
        socket.setsockopt(zmq.RCVTIMEO, 250)
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "LOG/")
        listener = ZeroMQSocketLogListener(
            CMDPTransmitter(self.name, socket),
            *self.log.handlers,
            respect_handler_level=True,  # handlers can have different log lvls
        )
        self._log_listeners[uuid] = listener
        listener.start()

    def _remove_satellite(self, service: DiscoveredService) -> None:
        # remove for metrics
        super()._remove_satellite(service)
        # remove from logging
        uuid = str(service.host_uuid)
        try:
            listener = self._log_listeners.pop(uuid)
            listener.stop()
        except KeyError:
            pass

    def _run_task_handler(self) -> None:
        """Event loop for task handler-routine"""
        while self._task_handler_event and not self._task_handler_event.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    self.log.exception(e)
            except Empty:
                # nothing to process
                pass

    def _log_listening_shutdown(self) -> None:
        for _uuid, listener in self._log_listeners.items():
            listener.stop()
        self._log_listeners = {}

    def reentry(self) -> None:
        self._log_listening_shutdown()
        super().reentry()


class FileMonitoringListener(MonitoringListener):
    def __init__(self, name: str, group: str, interface: str, output_path: str):
        self.output_path = pathlib.Path(output_path)
        try:
            os.makedirs(self.output_path)
        except FileExistsError:
            pass
        try:
            os.mkdir(self.output_path / "logs")
            os.mkdir(self.output_path / "stats")
        except FileExistsError:
            pass

        super().__init__(name, group, interface)

        handler = logging.handlers.RotatingFileHandler(
            self.output_path / "logs" / (group + ".log"),
            maxBytes=10**7,
            backupCount=10,
        )
        formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
        handler.setFormatter(formatter)
        handler.setLevel(logging.DEBUG)
        self.log.addHandler(handler)

    def metric_callback(self, metric: Metric) -> None:
        super().metric_callback(metric)
        fname = f"stats/{metric.sender}.{metric.name.lower()}.csv"
        path = self.output_path / fname
        ts = metric.time.to_unix()
        with open(path, "a") as csv:
            csv.write(f"{ts}, {metric.value}, '{metric.unit}'\n")


def main(args: Any = None) -> None:
    """Start a simple log listener service."""
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-o",
        "--output-path",
        type=str,
        help="The path to write log and metric data to.",
    )
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    mon: MonitoringListener

    # create output directories and configure file writer logger
    output_path: str | None = args.pop("output_path")
    if output_path is not None:
        mon = FileMonitoringListener(**args, output_path=output_path)
    else:
        mon = MonitoringListener(**args)

    mon.run_listener()


if __name__ == "__main__":
    main()
