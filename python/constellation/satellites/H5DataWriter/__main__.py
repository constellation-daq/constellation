"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Provides the entry point for the H5DataWriter satellite
"""

from constellation.core.base import setup_cli_logging, EPILOG
from constellation.core.satellite import SatelliteArgumentParser

from .H5DataWriter import H5DataWriter


def main(args=None):
    # Get a dict of the parsed arguments
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # Start satellite with remaining args
    s = H5DataWriter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
