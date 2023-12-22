#!/usr/bin/env python3
import zmq
import logging
import msgpack
import time
import readline


class TrivialController:
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, hosts):
        """Initialize values.

        Arguments:
        - hosts :: list of ip addr and ports for satellites to control
        """
        self._logger = logging.getLogger(__name__)

        self.sockets = []

        context = zmq.Context()
        for host in hosts:
            socket = context.socket(zmq.REQ)
            if "tcp://" not in host[:6]:
                host = "tcp://" + host
            socket.connect(host)
            self.sockets.append(socket)
            self._logger.info(f"connecting to {host}, ID {len(self.sockets)-1}...")

    def receive(self, socket):
        """Receive and parse data."""
        response = socket.recv_multipart()
        d = msgpack.unpackb(response[1]) if len(response) > 1 else {}
        p = msgpack.unpackb(response[2]) if len(response) > 2 else {}
        return response[0].decode("utf-8"), d, p

    def command(self, cmd, idx=0, socket=None):
        """Send cmd and await response."""

        # prepare request header:
        rhead = {"time": time.time(), "sender": "FIXME"}
        rd = msgpack.packb(rhead)

        if socket:
            socket.send_string(cmd, flags=zmq.SNDMORE)
            socket.send(rd)
            self._logger.info(f"ID{idx} send command {cmd}...")
            response, header, payload = self.receive(socket)
            self._logger.info(f"ID{idx} received response: {response}")
            if header:
                self._logger.info(f"    header: {header}")
            if payload:
                self._logger.info(f"    payload: {payload}")

        else:
            for i, sock in enumerate(self.sockets):
                sock.send_string(cmd, flags=zmq.SNDMORE)
                sock.send(rd)
                self._logger.info(f"ID{i} send command {cmd}...")
                response, header, payload = self.receive(sock)
                self._logger.info(f"ID{i} received response: {response}")
                if header:
                    self._logger.info(f"    header: {header}")
                if payload:
                    self._logger.info(f"    payload: {payload}")

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
            'Possible commands: "exit", "get_state", "transition <transition>", "failure", "register <ip> <port>"'
        )
        print(
            'Possible transitions: "load", "unload", "launch", "land", "start", "stop", "recover", "reset"'
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
            else:
                self.command(user_input, idx, socket) if socket else self.command(
                    user_input
                )
                socket = None


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
    commands = ["exit", "get_state", "transition ", "failure", "register "]
    transitions = [
        "load",
        "unload",
        "launch",
        "land",
        "start",
        "stop",
        "recover",
        "reset",
    ]

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
