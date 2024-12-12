"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This is the entry point for the keithley satellite.
"""

from constellation.core.base import setup_cli_logging, EPILOG
from constellation.core.satellite import SatelliteArgumentParser

from .LakeShore218 import LakeShore218


def main(args=None):
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = LakeShore218(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
