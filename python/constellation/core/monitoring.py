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
from uuid import UUID
from queue import Empty
from functools import wraps
from datetime import datetime
from logging.handlers import QueueHandler, QueueListener

from .base import BaseSatelliteFrame, ConstellationArgumentParser, EPILOG
from .cmdp import CMDPTransmitter, Metric, MetricsType
from .chirp import CHIRPServiceIdentifier
from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService


def schedule_metric(handling: MetricsType, interval: float):
    """Schedule a function for callback at interval [s] and send Metric.

    The function should take no arguments and return a value [any] and a unit
    [str].

    """

    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            res = func(*args, **kwargs)
            if isinstance(res, tuple):
                val, unit = res
            else:
                val = res
                unit = ""
            m = Metric(
                name=func.__name__,
                unit=unit,
                handling=handling,
                value=val,
            )
            return m

        # mark function as chirp callback
        wrapper.metric_scheduled = interval
        return wrapper

    return decorator


def get_scheduled_metrics(cls):
    """Loop over all class methods and return those marked as metric."""
    res = {}
    for func in dir(cls):
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "metric_scheduled"):
                val = getattr(call, "metric_scheduled")
                if isinstance(val, float):
                    # safeguard for tests only: a mock context would end up here
                    res[call.__name__] = {"function": call, "interval": val}
    return res


class MonitoringSender(BaseSatelliteFrame):
    """Sender mixin class for Constellation Monitoring Distribution Protocol.

    Any method of inheriting classes that has the @schedule_metric decorator,
    will be regularly polled for new values and a corresponding Metric be sent
    on the monitoring port.

    """

    def __init__(self, name: str, mon_port: int, interface: str, **kwds):
        """Set up logging and metrics transmitters."""
        super().__init__(name=name, interface=interface, **kwds)

        # Create monitoring socket and bind interface
        socket = self.context.socket(zmq.PUB)
        socket.bind(f"tcp://{interface}:{mon_port}")
        self._mon_tm = CMDPTransmitter(name, socket)

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
        callback: callable,
        interval: float,
        handling: MetricsType = MetricsType.LAST_VALUE,
    ):
        """Schedule a callback at regular intervals.

        The callable needs to return a value [any] and a unit [str] and take no
        arguments. If you have a callable that requires arguments, consider
        using functools.partial to fill in the necessary information at
        scheduling time.

        """

        def wrapper():
            res = callback()
            if isinstance(res, tuple):
                val, unit = res
            else:
                val = res
                unit = ""
            m = Metric(
                name=name,
                unit=unit,
                handling=handling,
                value=val,
            )
            return m

        self._metrics_callbacks[name] = {"function": wrapper, "interval": interval}

    def send_metric(self, metric: Metric):
        """Send a single metric via ZMQ."""
        return self._mon_tm.send_metric(metric)

    def _add_com_thread(self):
        """Add the metric sender thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_sender"] = threading.Thread(
            target=self._send_metrics, daemon=True
        )
        self.log.debug("Metric sender thread prepared and added to the pool.")

    def _send_metrics(self):
        """Metrics sender loop."""
        last_update = {}
        while not self._com_thread_evt.is_set():
            for metric, param in self._metrics_callbacks.items():
                update = False
                try:
                    last = last_update[metric]
                    if (datetime.now() - last).total_seconds() > param["interval"]:
                        update = True
                except KeyError:
                    update = True
                if update:
                    try:
                        self.send_metric(param["function"]())
                    except Exception as e:
                        self.log.error(
                            "Could not retrieve metric %s: %s", metric, repr(e)
                        )
                    last_update[metric] = datetime.now()

            time.sleep(0.1)
        self.log.info("Monitoring metrics thread shutting down.")
        # clean up
        self.close()

    def close(self):
        """Close the ZMQ socket."""
        self.log.removeHandler(self._zmq_log_handler)
        self._mon_tm.close()


class ZeroMQSocketLogHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPTransmitter):
        super().__init__(transmitter)

    def enqueue(self, record):
        self.queue.send(record)

    def close(self):
        if not self.queue.closed:
            self.queue.close()


class ZeroMQSocketLogListener(QueueListener):
    """This listener receives messages from a CMDPTransmitter.

    NOTE that the corresponding socket should only subscribe to LOG messages!

    """

    def __init__(self, transmitter, /, *handlers, **kwargs):
        super().__init__(transmitter, *handlers, **kwargs)
        self._stop_recv = threading.Event()

    def dequeue(self, block):
        # FIXME it is quite likely that this blocking call causes errors when
        # shutting down as the ZMQ context is removed before this call ends.
        record = None
        while not record and not self._stop_recv.is_set():
            try:
                record = self.queue.recv()
            except zmq.ZMQError:
                pass
        if self._stop_recv.is_set():
            # close down
            return self._sentinel
        return record

    def stop(self):
        """Close socket and stop thread."""
        super().stop()
        self.queue.close()

    def enqueue_sentinel(self):
        self._stop_recv.set()


class MonitoringListener(CHIRPBroadcaster):
    """Simple monitor class to receive logs and metrics from a Constellation."""

    def __init__(self, name: str, group: str, interface: str, output_path: str = None):
        """Initialize values.

        Arguments:
        - name ::  name of this Monitor
        - group ::  group of controller
        - interface :: the interface to connect to
        - output_path :: the directory to write logs and metric data to
        """
        super().__init__(name=name, group=group, interface=interface)

        self._log_listeners: dict[str, ZeroMQSocketLogListener] = {}
        self._metric_sockets: dict[UUID, zmq.socket] = {}

        # create output directories and configure file writer logger
        if output_path:
            self.output_path = pathlib.Path(output_path)
            try:
                os.makedirs(self.output_path)
                self.log.info("Created path %s", output_path)
            except FileExistsError:
                pass
            try:
                os.mkdir(self.output_path / "logs")
                os.mkdir(self.output_path / "stats")
            except FileExistsError:
                pass
            handler = logging.handlers.RotatingFileHandler(
                self.output_path / f"logs/{group}.log",
                maxBytes=10**7,
                backupCount=10,
            )
            formatter = logging.Formatter(
                "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            )
            handler.setFormatter(formatter)
            handler.setLevel(logging.DEBUG)
            self.log.addHandler(handler)
        else:
            self.output_path = None

        super()._add_com_thread()
        super()._start_com_threads()

        self.request(CHIRPServiceIdentifier.MONITORING)

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        self._task_handler_thread.start()
        self._metrics_receiver_shutdown = threading.Event()
        # Set up the metric poller which will monitor all ZMQ metric
        # subscription sockets
        self.poller = zmq.Poller()
        self._poller_lock = threading.Lock()

    @chirp_callback(CHIRPServiceIdentifier.MONITORING)
    def _add_satellite_callback(self, service: DiscoveredService):
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    def _add_satellite(self, service: DiscoveredService):
        address = "tcp://" + service.address + ":" + str(service.port)
        uuid = str(service.host_uuid)
        self.log.debug(
            "Connecting to %s, address %s...",
            uuid,
            address,
        )
        # create socket for logs
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

        # create socket for metrics
        socket = self.context.socket(zmq.SUB)
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "STATS/")
        self._metric_sockets[uuid] = socket
        self.poller.register(socket, zmq.POLLIN)

    def _remove_satellite(self, service: DiscoveredService):
        # departure
        uuid = str(service.host_uuid)
        self.log.debug(
            "Departure of %s.",
            service.host_uuid,
        )
        try:
            listener = self._log_listeners.pop(uuid)
            listener.stop()
        except KeyError:
            pass
        try:
            with self._poller_lock:
                socket = self._metric_sockets.pop(uuid)
                self.poller.unregister(socket)
                socket.close()
        except KeyError:
            pass

    def receive_metrics(self):
        """Main loop to receive metrics."""
        # set up transmitter for decoding metrics
        transmitter = CMDPTransmitter(None, None)

        while not self._metrics_receiver_shutdown.is_set():
            try:
                with self._poller_lock:
                    sockets_ready = dict(self.poller.poll(timeout=250))
                    for socket in sockets_ready.keys():
                        binmsg = socket.recv_multipart()
                        m = transmitter.decode_metric(binmsg[0].decode("utf-8"), binmsg)
                        if self.output_path:
                            # append to file
                            fname = f"stats/{m.sender}_{m.name.lower()}.csv"
                            path = self.output_path / fname
                            ts = m.time.to_unix()
                            with open(path, "a") as csv:
                                csv.write(f"{ts}, {m.value}, '{m.unit}'\n")
                        else:
                            print(m)
            except KeyboardInterrupt:
                break

    def _run_task_handler(self):
        """Event loop for task handler-routine"""
        while not self._task_handler_event.is_set():
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

    def reentry(self):
        """Shutdown Monitor."""
        self._metrics_receiver_shutdown.set()
        for _uuid, listener in self._log_listeners.items():
            listener.stop()
        with self._poller_lock:
            for _uuid, socket in self._metric_sockets.items():
                self.poller.unregister(socket)
                socket.close()
        super().reentry()


def main(args=None):
    """Start a simple log listener service."""
    import coloredlogs

    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-o",
        "--output-path",
        type=str,
        help="The path to write log and metric data to.",
    )
    # set the default arguments
    parser.set_defaults(name="basic_monitor")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = logging.getLogger(args["name"])
    log_level = args.pop("log_level")
    coloredlogs.install(level=log_level.upper(), logger=logger)

    mon = MonitoringListener(**args)
    mon.receive_metrics()


if __name__ == "__main__":
    main()
