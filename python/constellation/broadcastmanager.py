import logging
import threading

import zmq
import time
import yaml

from typing import Optional, Callable
from .protocol import ServiceIdentifier, MessageType, SatelliteBeacon

# from .fsm import SatelliteState

""" Conecpt of broadcastmanager: services are not specified in the manner they handle the socket. For example: a service like HEARTBEAT offers other units the ability to
listen in on the hosts hb_port. Meanwhile a service like CONTROL offers other units the ability to send commands to the hosts cmd_port. It all depends on the callbacks hence why
DiscoverCallback is needed to specify action when receiving an OFFER. """


class RegisteredService:
    """Class to hold service description along with callback to service function."""

    def __init__(
        self,
        port,
        serviceid: ServiceIdentifier = None,
    ):
        """Initialize variables."""
        self._port = port
        self._serviceid = serviceid


class DiscoveredService:
    """Class to hold discovered service data."""

    def __init__(
        self,
        hostid,
        address,
        port,
        serviceid: ServiceIdentifier = None,
    ):
        """Initialize variables."""
        self._hostid = hostid
        self._address = address
        self._port = port
        self._serviceid = serviceid


class DiscoverCallback:
    """Class to hold callback to be used when discovering service."""

    def __init__(
        self,
        callback: Callable,
        serviceid: ServiceIdentifier,
    ):
        self._callback = callback
        self._serviceid = serviceid


class BroadcastManager:
    """Broadcast services and listen in on the network."""

    def __init__(
        self,
        host=None,
        config_file=None,
        group: str = "Default",
        registered_services: RegisteredService = [],
        discovered_services: DiscoveredService = [],
        discover_callbacks: DiscoverCallback = {},
        *args,
        context: Optional[zmq.Context] = None,
        **kwargs,
    ):
        super().__init__(*args, **kwargs)
        self.host = host
        self._stop_broadcasting = threading.Event()
        self.group = group
        self.discovered_services = discovered_services
        self.context = context or zmq.Context()
        self.beacon = SatelliteBeacon(self.host, self.group, self.context)

        # Set up threads and depart events for callbacks
        self._callback_lock = threading.Lock()
        self._callback_threads = {}
        self._depart_events = {}

        # Register callbacks for services
        self.discover_callbacks = discover_callbacks

        # Register offered services
        self.registered_services = registered_services
        for new_service in self.registered_services:
            self.beacon.broadcast_service(
                new_service._serviceid, MessageType.OFFER, new_service._port
            )
            time.sleep(1)

        # Option for configuring which units are allowed to be discovered
        self.config_file = config_file

        if self.config_file:
            with open(self.config_file, "r") as configFile:
                self.yaml_config = yaml.safe_load(configFile)
                self.whitelist = self.yaml_config["Whitelist"]

    def start(self):
        """Start broadcast manager."""
        self._run_thread = threading.Thread(target=self.run, daemon=True)
        self._run_thread.start()

    def stop(self):
        self._stop_broadcasting.set()
        self._run_thread.join()

    def get_serviceid(self):
        """Returns a list of all serviceid from offered services."""
        return [service._serviceid for service in self.registered_services]

    def find_registered_callback(self, serviceid):
        """Find the registered callback if it exists."""
        for callback in self.discover_callbacks:
            if serviceid == callback._serviceid:
                return callback
        logging.info("Could not find callback")

    def find_registered_service(self, serviceid):
        """Find the registered service if it exists."""
        for service in self.registered_services:
            if serviceid == service._serviceid:
                return service
        logging.info("Could not find service")

    def find_discovered_service(self, hostid, host_addr, port, serviceid):
        service = DiscoveredService(hostid, host_addr, port, serviceid)
        for discovered_service in self.discovered_services:
            if service == discovered_service:
                return discovered_service

    def broadcast_all(self):
        """Broadcast all registered services."""
        for service in self.registered_services:
            try:
                self.beacon.broadcast_service(
                    service._serviceid, MessageType.OFFER, service._port
                )
                time.sleep(1)
            except RuntimeError:
                pass

    def register_callback(self, serviceid, callback):
        """Register new callback for ServiceIdentifier."""
        if serviceid in self.discover_callbacks.keys():
            logging.info("Overwriting callback")
        self.discover_callbacks[serviceid] = callback

    def register_service(self, port, serviceid):
        """Register new offered service or overwrite existing service."""
        if serviceid in [service._serviceid for service in self.registered_services]:
            logging.info("Overwriting service")
        self.registered_services.append(RegisteredService(port, serviceid))
        self.beacon.broadcast_service(serviceid, MessageType.OFFER, port)

    def discover_service(self, hostid, address, port, serviceid):
        """Add newly discovered service to internal list and return a DiscoveredService."""
        new_service = DiscoveredService(hostid, address, port, serviceid)
        if new_service in self.discovered_services:
            logging.info("Service already discovered")
        else:
            self.discovered_services.append()
        return new_service

    def forget_service(self, hostid, address, port, serviceid):
        """Forget discovered service."""
        self.discovered_services.remove(
            DiscoveredService(hostid, address, port, serviceid)
        )

    def request_service(self, serviceid):
        """Request service of type ServiceIdentifier."""
        if serviceid not in self.discover_callbacks.keys():
            logging.warn("Serviceid does not have a registered callback")
        self.beacon.broadcast_service(serviceid, MessageType.REQUEST)

    def run(self):
        """Start listening in on broadcast"""

        while not self._stop_broadcasting.is_set():
            try:
                print("Doing stuff")  # TODO: remove
                msg = self.beacon.listen()
                print("Found stuff")
                if not msg:
                    continue

                type, group, hostid, serviceid, port, host_addr = msg

                if group != self.group:
                    continue

                # Check Message Type
                if type == MessageType.REQUEST:
                    registered_service = self.find_registered_service(serviceid)
                    if registered_service:
                        self.beacon.broadcast_service(
                            registered_service._serviceid,
                            MessageType.OFFER,
                            registered_service._port,
                        )
                        logging.info(
                            f"Broadcasting service {registered_service._serviceid}"
                        )
                    continue

                if type == MessageType.OFFER and hostid in self.whitelist:
                    discovered_service = self.discover_service(
                        hostid, host_addr, port, serviceid
                    )
                    logging.info(f"Received OFFER for service: {serviceid}")
                    registered_callback = self.find_registered_callback(serviceid)
                    if registered_callback:
                        with self._callback_lock:
                            try:
                                depart_event = threading.Event()
                                self._callback_threads[
                                    discovered_service
                                ] = threading.Thread(
                                    target=registered_callback._callback,
                                    args=(host_addr, port, depart_event),
                                    daemon=True,
                                )
                                self._callback_threads[discovered_service].start()
                                self._depart_events[discovered_service] = depart_event
                                logging.info("Found and started callback for service")
                            except Exception:
                                logging.error(
                                    f"Could not issue callback for service: {serviceid} on host with id: {hostid}"
                                )
                    continue

                if (
                    type == MessageType.DEPART
                    and port != 0
                    and (
                        depart_service := self.find_discovered_service(
                            hostid, host_addr, port, serviceid
                        )
                    )
                    in self.discovered_services
                ):
                    self.forget_service(hostid, port)
                    self._depart_events[depart_service].set()
                    self._callback_threads[depart_service].join()
                    continue

            except RuntimeError:
                # nothing to process
                pass
