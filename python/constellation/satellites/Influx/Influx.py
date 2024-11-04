"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading

import influxdb_client
from influxdb_client.client.write_api import SYNCHRONOUS

from constellation.core.base import setup_cli_logging, EPILOG
from constellation.core.cmdp import Metric
from constellation.core.configuration import Configuration
from constellation.core.monitoring import StatListener
from constellation.core.satellite import Satellite, SatelliteArgumentParser


class Influx(Satellite, StatListener):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._influxdb_connected = False
        self._influxdb_lock = threading.Lock()

    def do_initializing(self, config: Configuration) -> None:
        with self._influxdb_lock:
            self._influxdb_connected = False

            url = config.setdefault("url", "http://localhost:8086")
            token = config["token"]
            org = config["org"]
            self.bucket = config.setdefault("bucket", "constellation")

            self.client = influxdb_client.InfluxDBClient(url=url, token=token, org=org)
            self.write_api = self.client.write_api(write_options=SYNCHRONOUS)

            # Test connection
            self.log.debug('Connecting to InfluxDB with org "%s" at %s', org, url)
            try:
                query_api = self.client.query_api()
                query_api.query(org=org, query="buckets()")
            except Exception as e:
                raise ConnectionError("Could not connect to InfluxDB") from e

            self._influxdb_connected = True
            self.log.info("Connected to InfluxDB")

    def metric_callback(self, metric: Metric) -> None:
        super().metric_callback(metric)

        with self._influxdb_lock:
            if self._influxdb_connected:
                metric_type = type(metric.value)
                record = None
                if metric_type in [float, int, bool]:
                    record = influxdb_client.Point(metric.sender).field(metric.name, metric.value)
                if record is not None:
                    self.log.trace("Writing metric %s to InfluxDB", metric.name)
                    self.write_api.write(bucket=self.bucket, record=record)
                else:
                    self.log.debug(f"Metric of type {metric_type} cannot be written to InfluxDB")


def main(args=None):
    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = Influx(**args)
    s.run_satellite()
