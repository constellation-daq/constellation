"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides network helper routines.
"""

import socket
import ipaddress
import psutil
import argparse
import struct
from typing import Tuple, List

# From ip(7):  This boolean option enables the IP_ORIGDSTADDR ancillary message
#              in recvmsg(2), in which the kernel returns the original destina-
#              tion address of the datagram being received.  The ancillary mes-
#              sage contains a struct sockaddr_in.
# Not yet defined in socket lib.
IP_RECVORIGDSTADDR = 20

MAX_ANCILLARY_SIZE = 28  # sizeof(struct sockaddr_in6)
ANC_BUF_SIZE = socket.CMSG_SPACE(MAX_ANCILLARY_SIZE)


def get_broadcast_socket() -> socket.socket:
    """Create a broadcast socket and return it."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    # add destination information to ancillary message in recvmsg
    sock.setsockopt(socket.IPPROTO_IP, IP_RECVORIGDSTADDR, 1)

    # on socket layer (SOL_SOCKET), enable re-using address in case
    # already bound (REUSEPORT)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    # enable broadcasting
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    return sock


def decode_ancdata(ancdata: List[Tuple[int, int, bytes]]) -> Tuple[str, int]:
    """Decode ancillary message received via recvmsg and return destination ip+port."""
    for cmsg_level, cmsg_type, cmsg_data in ancdata:
        # Handling IPv4
        if cmsg_level == socket.SOL_IP and cmsg_type == IP_RECVORIGDSTADDR:
            family, port = struct.unpack("=HH", cmsg_data[0:4])
            port = socket.htons(port)

            if family != socket.AF_INET:
                raise TypeError(f"Unsupported socket type '{family}'")

            ip = socket.inet_ntop(family, cmsg_data[4:8])
            destination = (ip, port)
            return destination
    return "", -1


def get_addr(if_name: str) -> str | None:
    """Get the IPv4 address for the given interface name."""
    if_addrs = psutil.net_if_addrs().get(if_name) or []
    for if_addr in if_addrs:
        if if_addr.family == socket.AF_INET:
            return str(if_addr.address)
    return None


def get_netmask(if_name: str) -> str | None:
    """Get the IPv4 netmask for the given interface name."""
    if_addrs = psutil.net_if_addrs().get(if_name) or []
    for if_addr in if_addrs:
        if if_addr.family == socket.AF_INET:
            return str(if_addr.netmask)
    return None


def get_broadcast(interface: str) -> set[str]:
    """Determine broadcast(s) for interface(s) based on IPv4 ip address and netmask."""
    broadcasts = []
    for intf in socket.if_nameindex():
        if_name = intf[1]
        if_addr = get_addr(if_name)
        if_netmask = get_netmask(if_name)
        if if_name == interface or if_addr == interface or interface == "*":
            try:
                net = ipaddress.IPv4Network(f"{if_addr}/{if_netmask}", strict=False)
                broadcasts.append(str(net.broadcast_address))
            except ipaddress.AddressValueError:
                pass
    return set(broadcasts)


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
            if if_addr:
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
        if if_addr:
            interfaces.append(if_addr)
    return interfaces
