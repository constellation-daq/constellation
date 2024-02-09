#!/usr/bin/env python3
import zmq
import logging
import msgpack
import time
import readline

from .fsm import SatelliteFSM
from .protocol import ServiceIdentifier
from .broadcastmanager import BroadcastManager


class TrivialController:
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, hosts):
        """Initialize values.

        Arguments:
        - hosts :: list of ip addr and ports for satellites to control
        """
        self._logger = logging.getLogger(__name__)

        self.sockets = []

        self.context = zmq.Context()
        for host in hosts:
            self.add_sat(host)

        self.broadcast_manager = BroadcastManager()
        self.broadcast_manager.register_callback(
            ServiceIdentifier.CONTROL, self.add_sat
        )
        self.broadcast_manager.request_service(ServiceIdentifier.CONTROL)

    def add_sat(self, host, port: int = None):
        """Add satellite socket to controller on port."""
        if "tcp://" not in host[:6]:
            host = "tcp://" + host
        if port:
            host = host + ":" + port
        socket = self.context.socket(zmq.REQ)
        socket.connect(host)
        self.sockets.append(socket)
        self._logger.info(f"connecting to {host}, ID {len(self.sockets)-1}...")

    def remove_sat(self, socket):
        socket.close()
        self.sockets.remove(socket)
        self._logger.info("Removed socket")

    def receive(self, socket):
        """Receive and parse data."""
        if socket.poll(
            1000, zmq.POLLIN
        ):  # NOTE: The choice of 1000 in poll is completely arbitrary
            response = socket.recv_multipart()
            d = msgpack.unpackb(response[1]) if len(response) > 1 else {}
            p = msgpack.unpackb(response[2]) if len(response) > 2 else {}
            return response[0].decode("utf-8"), d, p

        else:
            raise TimeoutError

    def command(self, cmd, idx=0, socket=None):
        """Send cmd and await response."""

        # prepare request header:
        rhead = {"time": time.time(), "sender": "FIXME"}
        rd = msgpack.packb(rhead)

        if socket:
            print("Sending command")
            socket.send_string(cmd, flags=zmq.SNDMORE)
            socket.send(rd)
            self._logger.info(f"ID{idx} send command {cmd}...")

            try:
                response, header, payload = self.receive(socket)
                self._logger.info(f"ID{idx} received response: {response}")
                if header:
                    self._logger.info(f"    header: {header}")
                if payload:
                    self._logger.info(f"    payload: {payload}")

            except TimeoutError:
                self._logger.error(
                    f"ID{idx} did not receive response. Command timed out. Disconnecting socket..."
                )
                self.remove_sat(socket)

        else:
            for i, sock in enumerate(self.sockets):
                sock.send_string(cmd, flags=zmq.SNDMORE)
                sock.send(rd)
                self._logger.info(f"ID{i} send command {cmd}...")

                try:
                    response, header, payload = self.receive(sock)
                    self._logger.info(f"ID{i} received response: {response}")
                    if header:
                        self._logger.info(f"    header: {header}")
                    if payload:
                        self._logger.info(f"    payload: {payload}")
                except TimeoutError:
                    self._logger.error(
                        f"ID{i} did not receive response. Command timed out. Disconnecting socket..."
                    )
                    self.remove_sat(sock)

    def run(self):
        """Run controller."""
        self.command("get_state")
        self.command("transition initialize")
        self.command("transition prepare")
        self.command("transition start_run")
        self.command("get_state")

    def run_from_cli(self):
        """Run commands from CLI."""
        print(
            'Possible commands: "exit", "get_state", "transition <transition>", "target <id no.>", "failure", "register <ip> <port>", "add <ip> <port>", "remove <id no.>"'
        )
        print(
            'Possible transitions: "initialize", "load", "unload", "launch", "land", "start", "stop", "recover", "reset"'
        )
        socket = None
        while True:
            user_input = input("Send command: ")
            if user_input == "exit":
                break
            elif user_input.startswith("target"):
                idx = int(user_input.split(" ")[1])
                if idx >= len(self.sockets):
                    self._logger.error(f"No host with ID {idx}")
                socket = self.sockets[idx]
                self._logger.info(f"target for next command: host ID {idx}")
            elif user_input.startswith("add"):
                socket_addr = str(user_input.split(" ")[1])
                port = str(user_input.split(" ")[2])
                host = socket_addr + ":" + port
                self.control_reg(host)
            elif user_input.startswith("remove"):
                idx = int(user_input.split(" ")[1])
                if idx >= len(self.sockets):
                    self._logger.error(f"No host with ID {idx}")
                self.remove_sat(self.sockets[idx])
            else:
                self.command(user_input, idx, socket) if socket else self.command(
                    user_input
                )
                socket = None


class SatelliteManager(TrivialController):
    """Satellite Manager class implementing CHIRP protocol"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


class CliCompleter(object):  # Custom completer
    def __init__(self, commands, transitions):
        self.commands = sorted(commands)
        self.transitions = sorted(transitions)

    # Returns the first word if there is a space, otherwise nothing
    def get_cur_before(self):
        idx = readline.get_begidx()
        full = readline.get_line_buffer()
        prefix = full[:idx]
        n = prefix.split()
        if len(n) > 0:
            return n[0]
        else:
            return ""

    def complete(self, text, state):
        cmd = self.get_cur_before()
        if cmd == "transition":
            return self.complete_transition(text, state)
        elif cmd != "":
            return None
        if text == "":  # Display all possibilities
            self.matches = self.commands[:]
        else:
            self.matches = [s for s in self.commands if s and s.startswith(text)]

        if state > len(self.matches):
            return None
        else:
            return self.matches[state]

    def complete_transition(self, text, state):
        matches = [s for s in self.transitions if s and s.startswith(text)]
        if state > len(matches):
            return None
        else:
            return matches[state]


def main():
    """Start a controller."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--satellite", "--sat", action="append")
    args = parser.parse_args()

    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )
    if not args.satellite:
        print("No satellites specified! Use '--satellite' to add one.")
        return
    # Set up simple tab completion
    commands = [
        "exit",
        "get_state",
        "transition ",
        "failure",
        "register ",
        "add ",
        "remove ",
    ]
    transitions = [t.name for t in SatelliteFSM.events]

    cliCompleter = CliCompleter(list(set(commands)), list(set(transitions)))
    readline.set_completer_delims(" \t\n;")
    readline.set_completer(cliCompleter.complete)
    readline.parse_and_bind("tab: complete")

    # start server with args
    ctrl = TrivialController(hosts=args.satellite)
    ctrl.run_from_cli()
    # ctrl.run()


if __name__ == "__main__":
    main()
