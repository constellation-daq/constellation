# CHIRP

## Notes on sockets

Since CHIRP requires a fixed port and we might have multiple programs running CHIRP on one machine, it is important to ensure that the port is not blocked by one program. Networking libraries like ZeroMQ and NNG do this by default when binding to a wildcard address. To ensure that a socket can be used by more than one program, the `SO_REUSEADDR` socket option has to be enabled. Further, to send broadcasts the `SO_BROADCAST` socket option has to be enabled.

## Notes on broadcast addresses

CHIRP uses an UDP broadcast over port 7123, which means that it send a message to all participants in a network. However, "all" in this context depends the broadcast address.

For example, if you have a device with a fixed IP (e.g. 192.168.1.17) in a subnet (e.g. 255.255.255.0), the general broadcast address (255.255.255.255) does not work. Instead, the broadcast address for the specified subnet has to be used (e.g. 192.168.1.255). On Linux, the broadcast IP for a specific network interface can found for example by running `ip a`, it is the IP shown after `brd`.

To opposite to the broadcast address is the "any" address, which accepts incoming traffic from any IP. In general it can be deduced from the broadcast address by replacing all 255s with 0s. However, the default any address (0.0.0.0) is enough since message filtering has to be done anyway.

If no network (with DHCP) is available, the default broadcast address (255.255.255.255) does not work. As a workaround, the default any address (0.0.0.0) can be used to broadcast over localhost.

## CHIRP Manager

To run the CHIRP manager, run:

```sh
./build/cxx/constellation/tools/chirp_manager [CONSTELLATION_GROUP] [NAME] [BRD_ADDR] [ANY_ADDR]
```

The following commands are available:

- `list_registered_services`: list of services registered by the user in the manager
- `list_discovered_services [SERVICE]`: list of services discovered by the manager and are in the same group
- `register_service [SERVICE] [PORT]`: register a service in the manager
- `register_callback [SERVICE]`: register a discover callback for a service that prints the discovered service
- `request [SERVICE]`: send a CHIRP request for a given service
- `unregister_service [SERVICE] [PORT]`: unregister a service in the manager
- `unregister_callback [SERVICE]`: unregister a discover callback for a service
- `reset`: unregister all services and callbacks, and forget discovered services
- `quit`

## `constellation::chirp` Namespace

```{doxygennamespace} constellation::chirp
:content-only:
:members:
:protected-members:
:undoc-members:
```
