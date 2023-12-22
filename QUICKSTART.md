# Quick Start into Constellation

## Python
To install the package, run the following command inside the root of the cloned repository:

    pip install -e .

To run a Satellite, check out the help page first:

    $ python -m constellation.satellite --help
    usage: satellite.py [-h] [--log-level LOG_LEVEL] [--cmd-port CMD_PORT] [--log-port LOG_PORT] [--hb-port HB_PORT] [--name NAME]

    Start the base Satellite server.

    options:
      -h, --help            show this help message and exit
      --log-level LOG_LEVEL
      --cmd-port CMD_PORT
      --log-port LOG_PORT
      --hb-port HB_PORT
      --name NAME

The `-cmd-port` argument specifies the port, on which the Satellite waits for its commands.

The basic Satellite does not do much, however. The `DataSender` and
`DataReceiver` classes, which inherit from the `Satellite` base class, are
therefore more interesting to run:

    python -m constellation.datasender --help

The `DataSender` supports a `--data-port` argument, specifying the port on which
data will be made available on. The `DataReceiver`
(`constellation.datareceiver`) can listen to several senders. Each sender must
be registered. This is currently done by a call to `DataReceiver.recv_from()`,
see e.g. [this example](https://gitlab.desy.de/constellation/constellation/blob/06e958a56c4f57cc1de34b00cfd8d5aca746b1e6/python/constellation/datareceiver.py#L259).

New Satellites should inherit either from the base `Satellite` class, or from
derived classes such as the `DataSender` or `DataReceiver`. See the latter for
an example of what such a derived class could look like.

Once started, connect a Controller by running:

    python -m constellation.controller --satellite localhost:23999

Adjust the above interface to the `--satellite` argument according to which
Satellite to connect to. Multiple Satellites can be specified by adding further
`--satellite` arguments.

The Controller gives a short summary, what commands are supported.

