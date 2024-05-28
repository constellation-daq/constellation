#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""
from functools import partial

from ..core.satellite import Satellite, SatelliteArgumentParser
from ..core.fsm import SatelliteState
from ..core.commandmanager import cscp_requestable
from ..core.base import setup_cli_logging
import pycaenhv


class CaenHvSatellite(Satellite):
    """Satellite controlling a CAEN HV crate via `pycaenhv` library.

    Supported models include SY5527.

    """

    def do_initializing(self, configuration):
        """Set up connection to HV module and configure settings."""
        if getattr(self, "caen", None):
            # old connection
            self.caen.disconnect()
        # SY5527 and similar: pycaenhv
        self.caen = pycaenhv.CaenHVModule()
        system = configuration["system"]
        user = configuration["username"]
        pw = configuration["password"]
        link = configuration["link"]
        link_arg = configuration["link_argument"]
        self.caen.connect(system, link, link_arg, user, pw)
        if not self.caen.is_connected():
            raise RuntimeError("No connection to Caen HV crate established")

        # process configuration
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                # loop over boards
                self.log.info("Configuring board %s", brd)
                for chno, ch in enumerate(brd.channels):
                    # loop over channels
                    for par in ch.parameter_names:
                        # loop over parameters
                        # construct configuration key
                        key = f"board{brdno}_ch{chno}_{par.lower()}"
                        self.log.debug("Checking configuration for key '%s'", key)
                        try:
                            # retrieve and set value
                            val = configuration[key]
                            if par in ["Pw"]:
                                # do not want to power up just yet
                                continue
                            # the board essentially only knows 'float'-type
                            # arguments except for 'Pw':
                            val = float(val)
                            setattr(ch, par, val)
                            self.log.debug(
                                "Configuring %s on board %s, ch %s with value '%s'",
                                par,
                                brdno,
                                chno,
                                val,
                            )
                        except KeyError:
                            # nothing in the cfg, leave as it is
                            pass
                        except ValueError as e:
                            raise RuntimeError(
                                f"Error in configuration for key {key}: {repr(e)}"
                            )

        # configure metrics sending
        self._configure_monitoring()
        return f"Connected to crate and configured {len(crate.boards)} boards"

    def do_launching(self, payload):
        """Power up the HV."""
        nch = self._power_up()
        return f"Launched and powered {nch} channels."

    def do_interrupting(self, payload):
        """Power down but do not disconnect (e.g. keep monitoring)."""
        self._power_down()
        return "Interrupted and stopped HV."

    def fail_gracefully(self):
        """Kill HV and disconnect."""
        if getattr(self, "caen", None):
            self._power_down()
            self.caen.disconnect()
        return "Powered down and disconnected from crate."

    def get_channel_value(self, board: int, channel: int, par: str):
        """Return the value of a given channel parameter."""
        if self.fsm.current_state in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
        ]:
            return None
        try:
            with self.caen as crate:
                val = crate.boards[board].channels[channel].parameters[par]
        except Exception as e:
            val = None
            self.log.exception(e)
        return val

    @cscp_requestable
    def get_parameter(self, request):
        """Return the value of a parameter.

        Payload: dictionary with 'board', 'channel' and 'parameter' keys
        providing the respective values.

        """
        board = request.payload["board"]
        chno = int(request.payload["channel"])
        par = request.payload["parameter"]
        with self.caen as crate:
            val = crate.boards[board].channels[chno].parameters[par]
        return val, None, None

    @cscp_requestable
    def about(self, _request):
        """Get info about the Satellite"""
        res = "{__name__} "
        return res, None, None

    def _power_down(self):
        self.log.warning("Powering down all channels")
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                for ch in brd.channels:
                    ch.switch_off()
        self.log.info("All channels powered down.")

    def _configure_monitoring(self):
        """Schedule monitoring for certain parameters."""
        self.reset_scheduled_metrics()
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                # loop over boards
                self.log.info("Configuring monitoring for board %s", brd)
                for chno, ch in enumerate(brd.channels):
                    # loop over channels
                    for par in ["IMon", "VMon"]:
                        # add a callback using partial
                        self.schedule_metric(
                            f"b{brdno}_ch{chno}_{par}",
                            partial(
                                self.get_channel_value,
                                board=brdno,
                                channel=chno,
                                par=par,
                            ),
                            10.0,
                        )

    def _power_up(self):
        """Loop over channels and power them if they were configured such."""
        npowered = 0  # number of powered channels
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                self.log.info("Powering board %s", brd)
                for chno, ch in enumerate(brd.channels):
                    key = f"board{brdno}_ch{chno}_pw"
                    self.log.debug("Powering board %s channel %s", brdno, chno)
                    val = self.config.setdefault(key, "off")
                    if val.lower() in ["true", "on", "1", "enabled", "enable"]:
                        ch.switch_on()
                        npowered += 1
        return npowered


# ---


def main(args=None):
    """The CAEN high-voltage Satellite for controlling a SY5527 HV crate."""
    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )
    # this sets the defaults for our Satellite
    parser.set_defaults(
        name="CaenHVCrate",
        cmd_port=23901,
        mon_port=55501,
        hb_port=61201,
    )
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = CaenHvSatellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
