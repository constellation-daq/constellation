"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides network helper routines.
"""

import socket
import ipaddress
import psutil
import argparse


def get_addr(if_name: str):
    """Get the IPv4 address for the given interface name."""
    if_addrs = psutil.net_if_addrs().get(if_name) or []
    for if_addr in if_addrs:
        if if_addr.family == socket.AF_INET:
            return if_addr.address


def get_netmask(if_name: str):
    """Get the IPv4 netmask for the given interface name."""
    if_addrs = psutil.net_if_addrs().get(if_name) or []
    for if_addr in if_addrs:
        if if_addr.family == socket.AF_INET:
            return if_addr.netmask


def get_broadcast(interface: str) -> str:
    """Determine broadcast(s) for interface(s) based on IPv4 ip address and netmask."""
    broadcasts = []
    for intf in socket.if_nameindex():
        if_name = intf[1]
        if_addr = get_addr(if_name)
        if_netmask = get_netmask(if_name)
        if if_name == interface or if_addr == interface or interface == "*":
            net = ipaddress.IPv4Network(f"{if_addr}/{if_netmask}", strict=False)
            broadcasts.append(str(net.broadcast_address))
    return broadcasts


def validate_interface(interface: str) -> str:
    """Validate that the provided interface exists.

    interface :: IPv4 ip address or name of an existing network interface.

    Returns IPv4 ip address of interface or '*' for any/all interfaces.

    Raises ValueError if the interface/ip does not exist.
    """
    # if using any/all interfaces:
    if interface == "*":
        return interface

    for intf in socket.if_nameindex():
        if_name = intf[1]
        if_addr = get_addr(if_name)
        if if_name == interface or if_addr == interface:
            return if_addr
    raise argparse.ArgumentTypeError(
        f"'{interface}' is neither an network interface name nor an interface's IPv4 ip address."
    )


def get_interfaces() -> list[str]:
    """Get all available network interfaces."""
    interfaces = ["*"]
    for intf in socket.if_nameindex():
        if_name = intf[1]
        if_addr = get_addr(if_name)
        interfaces.append(if_name)
        interfaces.append(if_addr)
    return interfaces
