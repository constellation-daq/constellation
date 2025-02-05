#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Stage movement in X,Y,Z,theta(R) for electronCT
author: Malinda de Silva (@desilvam)

"""

import time
import numpy as np
import toml
from pathlib import Path
import shutil
import subprocess

from threading import Lock
from threading import Thread
from threading import Event

from constellation.core.satellite import SatelliteArgumentParser
from constellation.core.configuration import Configuration
from constellation.core.fsm import SatelliteState

from constellation.core.base import setup_cli_logging
from constellation.core.base import EPILOG

from constellation.core.cscp import CSCPMessage
from constellation.core.commandmanager import cscp_requestable

from constellation.core.datasender import DataSender

import pylablib as pll
from pylablib.devices import Thorlabs

# !!!!!!!!!!!!!!!!!!!! DO NOT CHANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
"THORLABS CALIBRATION FACTORS."

"LTS300C Linear Stages"
THORLABS_STAGE_UNIT_LTS300 = "mm"
THORLABS_STAGE_CALFACTOR_POS_LTS300 = 409600.0  # step/mm
THORLABS_STAGE_CALFACTOR_VEL_LTS300 = 21987328.0  # usteps/s
THORLABS_STAGE_CALFACTOR_ACC_LTS300 = 4506.0  # usteps/s^2
THORLABS_STAGE_POS_REQ_COM_LTS300 = "MGMSG_MOT_REQ_POSCOUNTER"
THORLABS_STAGE_POS_GET_COM_LTS300 = "MGMSG_MOT_GET_POSCOUNTER"

"PRMTZ8 via KDC101 or TDC001"
THORLABS_STAGE_UNIT_PRMTZ8 = "deg"
THORLABS_STAGE_CALFACTOR_POS_PRMTZ8 = 1919.6418578623391  # step/deg
THORLABS_STAGE_CALFACTOR_VEL_PRMTZ8 = 42941.66  # deg/s
THORLABS_STAGE_CALFACTOR_ACC_PRMTZ8 = 14.66  # deg/s^2
THORLABS_STAGE_POS_REQ_COM_PRMTZ8 = "MGMSG_MOT_REQ_ENCCOUNTER"
THORLABS_STAGE_POS_GET_COM_PRMTZ8 = "MGMSG_MOT_GET_ENCCOUNTER"

stage_axes = {"x": ["x", "X"], "y": ["y", "Y"], "z": ["z", "Z"], "r": ["r", "R"]}
#################################################################

# for lin stages
max_velocity = 10  # mm/s  (recommended: <5 mm/s)
max_aclrtn = 10  # mm/s^2

# for r stage
max_velocity_r = 10  # mm/s  (recommended: <5 mm/s)
max_aclrtn_r = 10  # mm/s^2

stage_max = {"x": 299, "y": 299, "z": 299, "r": 400}

# DO NOT SET VELOCITY > 20!!! THE STAGE WILL STOP MOVING AND SYNCING WITH PC MAY BE AFFECTED
# THIS THRESHOLD WAS TESTED ON X-STAGE AFTER X,Y,Z,R STAGES WERE MOUNTED TOGETHER.
# COULD FURTHER REDUCE IF LOAD IS HEAVIER

while_loop_pause_time = 1e-3  # time in s before re-evaluate while conditions

##################################################################


class ECTstage(DataSender):
    """Stage movements in XYZR"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs, data_port=65123)
        self._lock = Lock()

    #
    def do_initializing(self, cnfg: Configuration) -> str:
        """
        Configure the Satellite and ThorLab stages
        """

        allports = pll.list_backend_resources("serial")
        self.usbports = [element for element in allports if "/dev/ttyUSB" in element]
        self.log.info(self.usbports)

        # load conf file and save into ECTstage object
        config_file = cnfg["config_file"]
        with open(config_file, "r") as f:
            self.conf = toml.load(f)

        if "save_config" in self.conf["run"]:
            if self.conf["run"]["save_config"]:
                self._save_config_file(config_file)

        # initialise stage
        if "x" in self.conf["run"]["active_axes"]:
            self.stage_x = self._init_stage("x")
        if "y" in self.conf["run"]["active_axes"]:
            self.stage_y = self._init_stage("y")
        if "z" in self.conf["run"]["active_axes"]:
            self.stage_z = self._init_stage("z")
        if "r" in self.conf["run"]["active_axes"]:
            self.stage_r = self._init_stage("r")

        # verbose
        for axis in self.conf["run"]["active_axes"]:
            self._get_stage_info(axis)
        return "Initialized"

    def do_reconfigure(self, cnfg: Configuration) -> str:
        """
        Reconfigures stages using config file,
        then re-launches the stage to start positions
        """
        # load conf file and save into ECTstage object
        config_file = cnfg["config_file"]
        with open(config_file, "r") as f:
            self.conf = toml.load(f)

        if "save_config" in self.conf["run"]:
            if self.conf["run"]["save_config"]:
                self._save_config_file(config_file)

        # close stages and delete object
        for axis in list(stage_axes.keys()):
            try:
                stage = self._stage_select(axis)
                with self._lock:
                    stage.close()
                    del stage
            except AttributeError:
                pass

        # re-initialise stages
        if "x" in self.conf["run"]["active_axes"]:
            self.stage_x = self._init_stage("x")
        if "y" in self.conf["run"]["active_axes"]:
            self.stage_y = self._init_stage("y")
        if "z" in self.conf["run"]["active_axes"]:
            self.stage_z = self._init_stage("z")
        if "r" in self.conf["run"]["active_axes"]:
            self.stage_r = self._init_stage("r")

        # verbose
        for axis in self.conf["run"]["active_axes"]:
            self._get_stage_info(axis)

        # re-launches satellite to start position
        for axis in self.conf["run"]["active_axes"]:
            if self.conf[axis]["start_position"] > stage_max[axis]:
                raise KeyError("Home position in [{}] must be smaller than {}".format(axis, stage_max[axis]))
        self._move_to_start()

        return "Reconfigured from conf file"

    def do_launching(self) -> str:
        """
        move stage to start position
        """
        for axis in self.conf["run"]["active_axes"]:
            if self.conf[axis]["start_position"] > stage_max[axis]:
                raise KeyError("Home position in [{}] must be smaller than {}".format(axis, stage_max[axis]))
        self._move_to_start()
        return "Launched"

    def do_landing(self) -> str:
        """
        Closes connections with stage and lands satellite
        """
        # close stages and delete object
        for axis in list(stage_axes.keys()):
            try:
                stage = self._stage_select(axis)
                with self._lock:
                    stage.close()
                    del stage
            except AttributeError:
                pass
        return "Connection with stages closed"

    def do_starting(self, run_identifier: str) -> str:
        """
        move to data taking position
        """
        self.start_of_run_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
        for axis in self.conf["run"]["active_axes"]:
            if self.conf[axis]["start_position"] > stage_max[axis]:
                raise KeyError("Home position in [{}] must be smaller than {}".format(axis, stage_max[axis]))
        self._move_to_start()
        return "stage moved"

    def do_run(self, payload: any) -> str:
        """The main run routine.
        Move stages to all positions while returning positions
        """
        print("Run has begun")
        self.log.info("Run has begun")

        # Send data periodically in background until end of run
        bg_event = Event()
        bg_thread = Thread(target=self._send_positions_background, args=(bg_event,))
        bg_thread.start()

        print("Started thread")
        self.log.debug("Started thread")

        # updated zigzag movement
        pos = {}
        startTime_flag = True
        for axis in list(stage_axes.keys()):
            if axis in self.conf["run"]["active_axes"]:
                param = "position_" + axis
                if param in self.conf["run"]:
                    if len(self.conf["run"][param]) == 3:
                        pos[axis] = self._list_positions(self.conf["run"][param])
                        self.log.debug(f"{axis} {pos[axis]}")
                    else:
                        raise KeyError(
                            "{} {} {}".format(
                                param,
                                "must be a 3-valued list.",
                                "To take data at a single point, remove this parameter and use `home_position` instead",
                            )
                        )
                else:
                    pos[axis] = [self.conf[axis]["start_position"]]
            else:
                pos[axis] = [np.nan]

        self.log.debug(f"pos: {pos}")
        for pos_z in pos["z"]:
            if np.isnan(pos_z):
                self.log.debug("z stage not moved")
            else:
                self.log.debug(f"z position {pos_z}")
                self._move_stage("z", pos_z)
                self._wait_until_stage_stops_moving(self.stage_z)
                if self._state_thread_evt.is_set():
                    break

            self.log.debug(self._generate_zigzagPath(pos["x"], pos["y"], pos["r"]))
            for pos_x, pos_y, pos_r in self._generate_zigzagPath(pos["x"], pos["y"], pos["r"]):
                self.log.info(f"Move to {pos_x} {pos_y} {pos_z} {pos_r}")

                if not np.isnan(pos_r):
                    self._move_stage("r", pos_r)
                    self._wait_until_stage_stops_moving(self.stage_r)
                else:
                    self.log.debug("r stage not moved")
                if self._state_thread_evt.is_set():
                    break

                if not np.isnan(pos_y):
                    self._move_stage("y", pos_y)
                    self._wait_until_stage_stops_moving(self.stage_y)
                else:
                    self.log.debug("y stage not moved")
                if self._state_thread_evt.is_set():
                    break

                if not np.isnan(pos_x):
                    self._move_stage("x", pos_x)
                    self._wait_until_stage_stops_moving(self.stage_x)
                else:
                    self.log.debug("x stage not moved")

                if self._state_thread_evt.is_set():
                    print("is this the issue?")
                    break

                if startTime_flag:
                    self.start_of_loop_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW) - self.start_of_run_time
                    startTime_flag = False
                # measurement time
                time.sleep(self.conf["run"]["stop_time_per_point_s"])

        self.log.info("exited loop")
        self.end_of_loop_time = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW) - self.start_of_run_time
        run_interrupted = self._state_thread_evt.is_set()
        if run_interrupted:
            self.log.info("Run has been interrupted")

        bg_event.set()
        bg_thread.join()
        self.log.debug("Background thread joined")

        if run_interrupted:
            return "Run has been interrupted"
        else:
            self.log.info("Run has completed")
            self._move_to_start()
            return "Finished acquisition. Stop run to proceed"

    def do_stopping(self) -> str:
        """End the run. Add run metadata for end-of-run event"""

        self.EOR = {"start_of_loop_time": self.start_of_loop_time, "end_of_loop_time": self.end_of_loop_time}
        return "Stopped. EOR sent"

    @cscp_requestable
    def go_home(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Goes back to the stage defined home position.
        Ideally should be 0 mm for linear stages and 0 deg for rotational stage
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                with self._lock:
                    stage.home()

        return "Stage moved to origin", None, {}

    @cscp_requestable
    def stage_stop(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Stops stages. Only works outside of main loop.
        FYI: For emergency stop while within run loop, use stop()
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                if self._stage_moving(stage):
                    self._stage_stop(stage)

        return "Stage Stopped", None, {}

    @cscp_requestable
    def get_status(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Returns stage status
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + ax + ": " + str(stage.get_status()) + " \n"
        self.log.info(val)
        return "Returned status: see logs/eCT terminal", None, {}

    @cscp_requestable
    def get_full_status(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Returns stage full status
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + ax + ": " + str(stage.get_full_status()) + " \n"
        self.log.info(val)
        return "Returned full status: see logs/eCT terminal", None, {}

    @cscp_requestable
    def get_full_info(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Returns stage information including status, serial port communication information
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + ax + ": " + str(stage.get_full_info()) + " \n"
        self.log.info(val)
        return "Returned full info: see logs/eCT terminal", None, {}

    @cscp_requestable
    def blink(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Blink test stages
        args: `axis`
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"] and axis is not None:
            stage = self._stage_select(axis)
            with self._lock:
                stage.blink()
            return "Blinking {} axis".format(axis), None, {}
        else:
            return "Stage not found. `axis` is a mandatory argument", None, {}

    @cscp_requestable
    def disconnect(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Disconnects the stages
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}

        for ax in self.conf["run"]["active_axes"]:
            print_axes = ""
            if ax == axis or axis is None:
                print_axes = print_axes + ax + " "
                stage = self._stage_select(ax)
                with self._lock:
                    stage.close()
                self.log.info(f"stage(s) {ax} closed! Reinitialize to reconnect")
            return "stages closed! Reinitialize to reconnect", None, {}

    def _disconnect_is_allowed(self, request: CSCPMessage) -> bool:
        return self.fsm.current_state_value in [SatelliteState.INIT]

    @cscp_requestable
    def get_vel_acc_params(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Get stage max, min velocities and acceleration
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}
        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + ax + ":" + str(stage.get_velocity_parameters(channel=self.conf[ax]["channel"], scale=True))
                if ax in stage_axes["r"]:
                    val = val + " deg; \n"
                else:
                    val = val + " mm; \n"
        return val, None, {}

    @cscp_requestable
    def get_position(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Get stage position
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis is not None:
            return "Stage not found", None, {}
        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                val = val + ax + ":" + str(self._get_position(ax))
                if ax in stage_axes["r"]:
                    val = val + " deg; \n"
                else:
                    val = val + " mm; \n"
        return val, None, {}

    @cscp_requestable
    def disable_axis(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Disable axis
        args: `axis`
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            self._enable_axis(axis, enable=False)
            return "{} stage disabled".format(axis), None, {}

        else:
            return "Stage not found! `axis` is a mandatory argument. Stage not disabled", None, {}

    @cscp_requestable
    def enable_axis(self, request: CSCPMessage) -> tuple[str, any, dict]:
        """
        Enable axis
        args: `axis`
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            self._enable_axis(axis, enable=True)
            return "{} stage enabled".format(axis), None, {}

        else:
            return "Stage not found! `axis` is a mandatory argument. Stage not enabled", None, {}

    def _get_serial_number(self, device_port):
        """
        Retrieves the ID_SERIAL of a device using udevadm.
        args:   device_port: The path to the device (e.g., "/dev/ttyUSB0").
        returns:
            The serial number as a string, or None if it's not found or an error occurs.
        """
        try:
            result = subprocess.run(
                ["udevadm", "info", "--query=property", "--property=ID_SERIAL_SHORT", "--value", device_port],
                capture_output=True,
                text=True,  # Important: Decode output as text
                check=True,  # Raise an exception for non-zero exit codes
            )
            serial = result.stdout.strip()  # Remove leading/trailing whitespace
            return serial
        except subprocess.CalledProcessError as e:
            print(f"Error getting serial number: {e}")
            print(f"Stderr: {e.stderr}")  # Print stderr for debugging
            return None
        except FileNotFoundError:
            print("udevadm command not found.")
            return None
        except Exception as e:  # Catch other potential errors
            print(f"An unexpected error occurred: {e}")
            return None

    def _init_stage(self, axis):
        "initialise the ThorLabs motor stages"
        for port in self.usbports:
            serial = self._get_serial_number(port)
            self.log.debug(f"port:{port}")
            self.log.debug(f"device serial no:{serial}")
            self.log.debug(f"serial no in config file:{self.conf[axis]['serial_no']}")
            if serial is not None and str(self.conf[axis]["serial_no"]) == str(serial):
                self.conf[axis]["port"] = port
                self.log.info(f"{axis}-axis assigned to port {port}")
                self.log.info(f"Device serial: {self.conf[axis]["serial_no"]}")
                self.usbports.remove(port)
                print(self.usbports)
        #
        with self._lock:
            if axis in stage_axes["x"] or axis in stage_axes["y"] or axis in stage_axes["z"]:
                stage = Thorlabs.KinesisMotor(
                    conn=self.conf[axis]["port"],
                    scale=(
                        THORLABS_STAGE_CALFACTOR_POS_LTS300,
                        THORLABS_STAGE_CALFACTOR_VEL_LTS300,
                        THORLABS_STAGE_CALFACTOR_ACC_LTS300,
                    ),
                )

                # if self.conf[axis]["serial_no"] != stage.get_full_info()["device_info"][0]:
                #     print_1 = f"Serial Number of {axis}-stage does not match the device."
                #     print_2 = f"Device serial number:{stage.get_full_info()['device_info'][0]}"
                #     print_3 = f"serial number in config:{self.conf[axis]['serial_no']}"
                #     raise KeyError(f"{print_1} {print_2} {print_3}")

                if self.conf[axis]["velocity"] > max_velocity:
                    raise KeyError(
                        "Velocity must be smaller than {}. Got {}".format(max_velocity, self.conf[axis]["velocity"])
                    )
                if self.conf[axis]["acceleration"] > max_aclrtn:
                    raise KeyError(
                        "Acceleration must be smaller than {}. Got {}".format(max_aclrtn, self.conf[axis]["acceleration"])
                    )
                stage.setup_velocity(
                    channel=self.conf[axis]["channel"],
                    max_velocity=self.conf[axis]["velocity"],
                    acceleration=self.conf[axis]["acceleration"],
                )

            elif axis in stage_axes["r"]:
                stage = Thorlabs.KinesisMotor(
                    conn=self.conf[axis]["port"],
                    scale=(
                        THORLABS_STAGE_CALFACTOR_POS_PRMTZ8,
                        THORLABS_STAGE_CALFACTOR_VEL_PRMTZ8,
                        THORLABS_STAGE_CALFACTOR_ACC_PRMTZ8,
                    ),
                )

                if self.conf[axis]["serial_no"] != stage.get_full_info()["device_info"][0]:
                    print_1 = f"Serial Number of {axis}-stage does not match the device."
                    print_2 = f"Device serial number:{stage.get_full_info()['device_info'][0]}"
                    print_3 = f"serial number in config:{self.conf[axis]['serial_no']}"
                    raise KeyError(f"{print_1} {print_2} {print_3}")

                if self.conf[axis]["velocity"] > max_velocity_r:
                    raise KeyError(
                        "Velocity must be smaller than {}. Got {}".format(max_velocity_r, self.conf[axis]["velocity"])
                    )
                if self.conf[axis]["acceleration"] > max_aclrtn_r:
                    raise KeyError(
                        "Acceleration must be smaller than {}. Got {}".format(max_aclrtn_r, self.conf[axis]["acceleration"])
                    )
                stage.setup_velocity(
                    channel=self.conf[axis]["channel"],
                    max_velocity=self.conf[axis]["velocity"],
                    acceleration=self.conf[axis]["acceleration"],
                )
            else:
                raise KeyError("Axis not found!")
        return stage

    def _enable_axis(self, axis, enable=True):
        stage = self._stage_select(axis)
        with self._lock:
            stage._enable_channel(enabled=enable)

    def _get_position(self, axis):
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            try:
                with self._lock:
                    return stage.get_position(channel=self.conf[axis]["channel"], scale=True)
            except NameError:
                return np.nan
        else:
            return np.nan

    def _move_stage(self, axis, position):
        """
        move stage
        """
        if position > stage_max[axis]:
            raise KeyError("Position must be smaller than {}".format(stage_max[axis]))
        stage = self._stage_select(axis)
        with self._lock:
            stage.move_to(position, channel=self.conf[axis]["channel"])

    def _wait_until_stage_stops_moving(self, stage):
        """
        wait while stage moves
        """
        while self._stage_moving(stage):
            if not self._state_thread_evt.is_set():
                time.sleep(while_loop_pause_time)
            else:
                self._stage_stop(stage)
                break

    def _get_unit(self, axis):
        if axis in ["x", "y", "z"]:
            return "mm"
        elif axis in ["r"]:
            return "deg"

    def _move_to_start(self, axis=None):
        """
        move to start positions defined in config file
        """
        for ax in self.conf["run"]["active_axes"]:
            if ax == axis or axis is None:
                # print(axis,"stage moving to start point (home)")
                unit = self._get_unit(ax)
                self.log.info(f"{ax} stage moving to start point (home): {self.conf[ax]['start_position']} {unit}")
                self._move_stage(ax, self.conf[ax]["start_position"])
            else:
                raise KeyError("Stage not found")
        self._move_lock()
        # print("stages moved")
        self.log.info("stages moved")

    def _stage_moving(self, stage):
        with self._lock:
            return stage.is_moving()

    def _stage_stop(self, stage):
        with self._lock:
            return stage.stop(immediate=True)

    def _move_lock(self):
        for axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            while self._stage_moving(stage):
                time.sleep(while_loop_pause_time)

    def _get_stage_info(self, axis):
        """
        prints many parameters
        """
        self.log.info(f"axis:{axis}")
        stage = self._stage_select(axis)
        with self._lock:
            self.log.info(stage.setup_velocity(channel=self.conf[axis]["channel"]))
            self.log.info(stage.get_full_info())
            self.log.info(stage.get_all_axes())
            self.log.info(stage.get_scale())
        self.log.info("")

    def _stage_select(self, axis):
        if axis in stage_axes["x"]:
            return self.stage_x
        elif axis in stage_axes["y"]:
            return self.stage_y
        elif axis in stage_axes["z"]:
            return self.stage_z
        elif axis in stage_axes["r"]:
            return self.stage_r
        else:
            raise KeyError("axis not found")

    def _list_positions(self, pos_range):
        """
        creates an list of positions
        """
        list = np.round(np.arange(pos_range[0], pos_range[1] + 1, pos_range[2]), 3)  # mm
        return list

    def _generate_zigzagPath(self, posX_list, posY_list, posR_list):
        """
        creates the zig-zag positions
        """
        XYarray = [[(x, y) for x in posX_list] for y in posY_list]
        XY_rev = []
        for index in range(len(XYarray)):
            if index % 2 == 1:
                XY_rev.append(XYarray[index][::-1])
            else:
                XY_rev.append(XYarray[index])
        XYarray = [item for sublist in XY_rev for item in sublist]
        XYRarray = [[(x, y, r) for (x, y) in XYarray] for r in posR_list]

        XYR_rev = []
        for index in range(len(XYRarray)):
            if index % 2 == 1:
                XYR_rev.append(XYRarray[index][::-1])
            else:
                XYR_rev.append(XYRarray[index])

        XYRarray = [item for sublist in XYR_rev for item in sublist]
        return XYRarray

    def _save_config_file(self, config_file):
        outdir = Path.cwd()
        outdir = outdir / Path("data")
        outdir.mkdir(parents=True, exist_ok=True)
        formatted_time = time.strftime("%Y-%m-%d-%H%M%S", time.localtime())

        outdir = outdir / Path("t_" + str(formatted_time) + "_" + config_file.rpartition("/")[2])
        shutil.copyfile(Path(config_file), outdir)
        self.log.info(f"Saved config file as {str(outdir)}")

    def _send_positions_background(self, event: Event):
        while not event.is_set():

            payload = np.array(
                [
                    time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW) - self.start_of_run_time,
                    self._get_position("x"),
                    self._get_position("y"),
                    self._get_position("z"),
                    self._get_position("r"),
                ],
                dtype=np.float64,
            )
            meta = {"dtype": f"{payload.dtype}"}
            self.data_queue.put((payload.tobytes(), meta))
            self.log.debug(f"Position: x={payload[1]}, y={payload[2]}, z={payload[3]}, r={payload[4]}, t={payload[0]}")
            event.wait(timeout=self.conf["run"]["readout_freq_s"])


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a
    basis for custom implementations.

    """

    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(name="FirstStage")
    args = vars(parser.parse_args(args))

    # set up logging
    setup_cli_logging(args["name"], args.pop("log_level"))

    # start server with remaining args
    s = ECTstage(**args)
    # s = eCTstage(**args)
    s.run_satellite()
