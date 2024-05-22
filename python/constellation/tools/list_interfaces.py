#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides a tool to list available network interfaces.
"""

import socket
import ipaddress
import argparse
from constellation.core.network import get_addr, get_netmask

EPILOG = "This command is part of the Constellation tool set."


def main():
    """Command listing all available network interfaces and their broadcast address."""
    parser = argparse.ArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.parse_args()

    print("\nAvailable network interfaces:")
    print("------\n")
    print(f"IDX:  {'address':<15} {'broadcast':<15} {'netmask':<15} (name)")
    for intf in socket.if_nameindex():
        if_index = intf[0]
        if_name = intf[1]
        if_addr = get_addr(if_name) or "<none>"
        if_netmask = get_netmask(if_name) or "<none>"
        try:
            net = ipaddress.IPv4Network(f"{if_addr}/{if_netmask}", strict=False)
            if_bc = str(net.broadcast_address)
        except ipaddress.AddressValueError:
            if_bc = "<none>"
        print(f"{if_index:<4}: {if_addr:<15} {if_bc:<15} {if_netmask:<15} ({if_name})")


if __name__ == "__main__":
    main()
