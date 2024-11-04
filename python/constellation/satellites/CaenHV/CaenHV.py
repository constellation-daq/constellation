#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""
from functools import partial
from typing import Tuple, Any

from pycaenhv import CaenHVModule  # type: ignore[import-untyped]
from .lib_caen_ndt1470 import CaenNDT1470Manager

from constellation.core.satellite import Satellite, SatelliteArgumentParser
from constellation.core.cmdp import MetricsType
from constellation.core.fsm import SatelliteState
from constellation.core.configuration import Configuration
from constellation.core.commandmanager import cscp_requestable, CSCPMessage
from constellation.core.base import setup_cli_logging


class CaenHVSatellite(Satellite):
    """Satellite controlling a CAEN HV crate via `pycaenhv` library.

    Supported models include SY5527.

    """

    def do_initializing(self, configuration: Configuration) -> str:
        """Set up connection to HV module and configure settings."""
        self.log.info(
            "Received configuration with parameters: %s",
            ", ".join(configuration.get_keys()),
        )
        if getattr(self, "caen", None):
            # old connection
            self.log.info("Disconnecting from previously connected CAEN module")
            # avoid that metric thread tries to access object
            self.reset_scheduled_metrics()
            with self.caen as crate:
                crate.disconnect()
        # SY5527 and similar: pycaenhv
        system = configuration["system"]
        link = configuration["link"]
        link_arg = configuration["link_argument"]
        user = configuration.setdefault("username", "")
        pw = configuration.setdefault("password", "")
        metrics_poll_rate = configuration["metrics_poll_rate"]
        if "ndt1" in system.lower():
            self.caen: CaenNDT1470Manager | CaenHVModule = CaenNDT1470Manager()
        else:
            self.caen = CaenHVModule()
        self.log.info(
            "Connecting to %s via %s and %s using username '%s' and password '%s'",
            system,
            link,
            link_arg,
            user,
            pw,
        )
        self.caen.connect(system=system, link=link, argument=link_arg, user=user, password=pw)
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
                        if not ch.parameters[par].attributes["mode"] == "R/W":
                            continue
                        # construct configuration key
                        key = f"board{brdno}_ch{chno}_{par.lower()}"
                        self.log.trace("Checking configuration for key '%s'", key)
                        try:
                            # retrieve and set value
                            val = configuration[key]
                            if par in ["Pw"]:
                                # do not want to power up just yet
                                continue
                            # the board essentially only knows 'float'-type
                            # arguments except for 'Pw':
                            val = float(val)
                        except KeyError:
                            # nothing in the cfg, leave as it is
                            continue
                        except ValueError as e:
                            raise RuntimeError(f"Error in configuration for key {key}: {repr(e)}") from e
                        ch.parameters[par].value = val
                        self.log.debug(
                            "Configuring %s on board %s, ch %s with value '%s'",
                            par,
                            brdno,
                            chno,
                            val,
                        )

        # configure metrics sending
        self._configure_monitoring(metrics_poll_rate)
        return f"Connected to crate and configured {len(crate.boards)} boards"

    # def do_reconfigure(self, partial_config: Configuration) -> str:
    #    """Reconfigure the HV module by re-running initialization and launch."""
    #    FIXME: needs updating! See https://gitlab.desy.de/constellation/constellation/-/issues/153
    #    self.do_initializing(configuration)
    #    return self.do_launching(None)

    def do_launching(self) -> str:
        """Power up the HV."""
        nch = self._power_up()
        return f"Launched and powered {nch} channels."

    def do_interrupting(self) -> str:
        """Power down but do not disconnect (e.g. keep monitoring)."""
        self._power_down()
        return "Interrupted and stopped HV."

    def fail_gracefully(self):
        """Kill HV and disconnect."""
        if getattr(self, "caen", None):
            self._power_down()
            self.caen.disconnect()
        return "Powered down and disconnected from crate."

    def get_channel_value(self, board: int, channel: int, par: str) -> Any:
        """Return the value of a given channel parameter."""
        if not self._ready():
            return None
        try:
            with self.caen as crate:
                val = crate.boards[board].channels[channel].parameters[par].value
        except Exception as e:
            val = None
            self.log.exception(e)
        return val

    def get_channel_status(self, board: int, channel: int, par: str) -> str:
        """Return the status of a given channel."""
        if not self._ready():
            return ""
        try:
            with self.caen as crate:
                status = crate.boards[board].channels[channel].status
        except Exception as e:
            status = [f"{repr(e)}"]
            self.log.exception(e)
        return ", ".join(status)

    @cscp_requestable
    def get_parameter(self, request: CSCPMessage) -> Tuple[Any, None, None]:
        """Return the value of a parameter.

        Payload: dictionary with 'board', 'channel' and 'parameter' keys
        providing the respective values.

        """
        board = request.payload["board"]
        chno = int(request.payload["channel"])
        par = request.payload["parameter"]
        val = self.get_channel_value(board, chno, par)
        return val, None, None

    def _get_parameter_is_allowed(self, request: CSCPMessage) -> bool:
        return self._ready()

    @cscp_requestable
    def get_hv_status(self, request: CSCPMessage) -> Tuple[str, dict[str, str], None]:
        """Return the collected state of all channels.

        Payload: None.

        """
        res = {}
        errors = []
        npowered = 0
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                for ch in brd.channels:
                    status = ch.status
                    if status:
                        res[f"board{brdno}_ch{ch.index}"] = ", ".join(status)
                        if any(s.lower() in ["on"] for s in status):
                            npowered += 1
                        if any(s.lower() not in ["on"] for s in status):
                            errors.append(f"board{brdno}_ch{ch.index}")
        msg = f"All OK, {npowered} powered"
        if errors:
            msg = "{npowered} powered, additional bits set in: " + ", ".join(errors)
        return msg, res, None

    def _get_status_is_allowed(self, request: CSCPMessage) -> bool:
        return self._ready()

    @cscp_requestable
    def get_hw_config(self, request: CSCPMessage) -> Tuple[str, dict[str, str], None]:
        """Read and return the current hardware configuration.

        Payload: None

        Returns: dictionary with all R/W parameters and their current values.

        """
        if not self._ready():
            raise RuntimeError(f"Command not allowed in state '{self.fsm.current_state_value.name}'")
        res = {}
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                for chno, ch in enumerate(brd.channels):
                    for par in ch.parameter_names:
                        if not ch.parameters[par].attributes["mode"] == "R/W":
                            continue
                        # construct configuration key
                        key = f"board{brdno}_ch{chno}_{par.lower()}"
                        res[key] = ch.parameters[par].value
        return f"Read {len(res)} parameters", res, None

    @cscp_requestable
    def about(self, _request: CSCPMessage) -> Tuple[str, None, None]:
        """Get info about the Satellite"""
        # TODO extend with info on connected crate (FW release, etc)
        res = f"{__name__} "
        return res, None, None

    def _power_down(self) -> None:
        self.log.warning("Powering down all channels")
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                for ch in brd.channels:
                    self.log.debug(f"Powering down ch {ch.index} on board {brdno}")
                    ch.switch_off()
        self.log.info("All channels powered down.")

    def _configure_monitoring(self, metrics_poll_rate: int) -> None:
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
                            "",
                            MetricsType.LAST_VALUE,
                            metrics_poll_rate,
                            partial(
                                self.get_channel_value,
                                board=brdno,
                                channel=chno,
                                par=par,
                            ),
                        )
                        self.schedule_metric(
                            f"b{brdno}_ch{chno}_{par}_status",
                            "",
                            MetricsType.LAST_VALUE,
                            metrics_poll_rate,
                            partial(
                                self.get_channel_status,
                                board=brdno,
                                channel=chno,
                                par=par,
                            ),
                        )

    def _power_up(self) -> int:
        """Loop over channels and enable them according to configuration."""
        npowered = 0  # number of powered channels
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                self.log.info("Powering board %s", brd)
                for chno, ch in enumerate(brd.channels):
                    key = f"board{brdno}_ch{chno}_pw"
                    val = self.config.setdefault(key, "off")
                    if val.lower() in ["true", "on", "1", "enabled", "enable"]:
                        self.log.debug("Powering board %s channel %s", brdno, chno)
                        ch.switch_on()
                        npowered += 1
                    else:
                        if ch.is_powered():
                            self.log.info("Powering DOWN board %s channel %s", brdno, chno)
                            ch.switch_off()
        return npowered

    def _ready(self) -> bool:
        """From the FSM state, determine whether we are ready."""
        if self.fsm.current_state_value in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
            SatelliteState.initializing,
            SatelliteState.reconfiguring,
        ]:
            return False
        return True


# ---


def main(args=None):
    """The CAEN high-voltage Satellite for controlling a SY5527 HV crate."""
    parser = SatelliteArgumentParser(
        description=main.__doc__,
        epilog="This is a 3rd-party component of Constellation.",
    )

    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = CaenHVSatellite(**args)
    s.run_satellite()
