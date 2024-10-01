#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""
from typing import Any

import datetime
import h5py  # type: ignore[import-untyped]
import numpy as np
import os
import pathlib

from constellation.core import __version__
from constellation.core.datareceiver import DataReceiver
from constellation.core.cdtp import CDTPMessage
from constellation.core.satellite import SatelliteArgumentParser
from constellation.core.base import EPILOG, setup_cli_logging


class H5DataWriter(DataReceiver):
    """Satellite which receives data via ZMQ and writes to HDF5."""

    def do_initializing(self, config: dict[str, Any]) -> str:
        """Initialize and configure the satellite."""
        super().do_initializing(config)
        # how often will the file be flushed? Negative values for 'at the end of
        # the run'
        self.flush_interval = self.config.setdefault("flush_interval", 10.0)
        return "Configured all values"

    def do_run(self, run_identifier: str) -> str:
        """Handle the data enqueued by the ZMQ Poller."""
        self.last_flush = datetime.datetime.now()
        return super().do_run(run_identifier)

    def _write_EOR(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write data to file"""
        grp = outfile[item.name].create_group("EOR")
        # add meta information as attributes
        grp.update(item.payload)
        self.log.info(
            "Wrote EOR packet from %s on run %s",
            item.name,
            self.run_identifier,
        )

    def _write_BOR(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write BOR to file"""
        if item.name not in outfile.keys():
            grp = outfile.create_group(item.name).create_group("BOR")
            # add payload dict information as attributes
            grp.update(item.payload)
            self.log.info(
                "Wrote BOR packet from %s on run %s",
                item.name,
                self.run_identifier,
            )

    def _write_data(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write data into HDF5 format

        Format: h5file -> Group (name) ->   BOR Dataset
                                            Single Concatenated Dataset
                                            EOR Dataset

        Writes data to file by concatenating item.payload to dataset inside group name.
        """
        # Check if group already exists.
        try:
            grp = outfile[item.name]
        except KeyError:
            # late joiners
            self.log.warning("%s sent data without BOR.", item.name)
            self.active_satellites.append(item.name)
            grp = outfile.create_group(item.name)

        if item.name not in self.active_satellites:
            self.log.warning(
                "%s sent data but is no longer assumed active (EOR received)",
                item.name,
            )

        title = f"data_{self.run_identifier}_{item.sequence_number:09}"

        if isinstance(item.payload, bytes):
            # interpret bytes as array of uint8 if nothing else was specified in the meta
            payload = np.frombuffer(item.payload, dtype=item.meta.get("dtype", np.uint8))
        elif isinstance(item.payload, list):
            payload = np.array(item.payload)
        elif item.payload is None:
            # empty payload -> empty array of bytes
            payload = np.array([], dtype=np.uint8)
        else:
            raise TypeError(f"Cannot write payload of type '{type(item.payload)}'")

        dset = grp.create_dataset(
            title,
            data=payload,
            chunks=True,
        )

        dset.attrs["CLASS"] = "DETECTOR_DATA"
        dset.attrs.update(item.meta)

        # time to flush data to file?
        if self.flush_interval > 0 and (datetime.datetime.now() - self.last_flush).total_seconds() > self.flush_interval:
            outfile.flush()
            self.last_flush = datetime.datetime.now()

    def _open_file(self, filename: pathlib.Path) -> h5py.File:
        """Open the hdf5 file and return the file object."""
        h5file = None
        if os.path.isfile(filename):
            self.log.critical("file already exists: %s", filename)
            raise RuntimeError(f"file already exists: {filename}")

        self.log.info("Creating file %s", filename)
        # Create directory path.
        directory = pathlib.Path(self.output_path)  # os.path.dirname(filename)
        try:
            os.makedirs(directory)
        except (FileExistsError, FileNotFoundError):
            self.log.info("Directory %s already exists", directory)
            pass
        except Exception as exception:
            raise RuntimeError(
                f"unable to create directory {directory}: \
                {type(exception)} {str(exception)}"
            ) from exception
        try:
            h5file = h5py.File(directory / filename, "w")
        except Exception as exception:
            self.log.critical("Unable to open %s: %s", filename, str(exception))
            raise RuntimeError(
                f"Unable to open {filename}: {str(exception)}",
            ) from exception
        self._add_metadata(h5file)
        return h5file

    def _close_file(self, outfile: h5py.File) -> None:
        """Close the filehandler"""
        outfile.close()

    def _add_metadata(self, outfile: h5py.File) -> None:
        """Add metadata such as version information to file."""
        grp = outfile.create_group(self.name)
        grp["constellation_version"] = __version__
        grp["date_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()


# -------------------------------------------------------------------------


def main(args: Any = None) -> None:
    """Start the Constellation data receiver satellite.

    Data will be written in HDF5 format.

    """
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)

    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = H5DataWriter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
