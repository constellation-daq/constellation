#!/usr/bin/env python3
"""
Stage movement in X,Y,Z,theta(R) for electronCT
author: Malinda de Silva (@desilvam)
"""

import time
import random
from typing import Any
import itertools
import numpy as np
import toml

from constellation.core.satellite import Satellite, SatelliteArgumentParser
from constellation.core.configuration import Configuration
from constellation.core.fsm import SatelliteState

from constellation.core.base import setup_cli_logging
from constellation.core.base import EPILOG

from constellation.core.cscp import CSCPMessage
from constellation.core.commandmanager import cscp_requestable

import pylablib as pll
from pylablib.devices import Thorlabs

# !!!!!!!!!!!!!!!!!!!! DO NOT CHANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
'THORLABS CALIBRATION FACTORS.'

"LTS300C Linear Stages"
THORLABS_STAGE_UNIT_LTS300			= "mm"
THORLABS_STAGE_CALFACTOR_POS_LTS300 = 409600.0      #step/mm
THORLABS_STAGE_CALFACTOR_VEL_LTS300	= 21987328.0    #usteps/s
THORLABS_STAGE_CALFACTOR_ACC_LTS300	= 4506.0        #usteps/s^2
THORLABS_STAGE_POS_REQ_COM_LTS300	= 'MGMSG_MOT_REQ_POSCOUNTER'
THORLABS_STAGE_POS_GET_COM_LTS300   = 'MGMSG_MOT_GET_POSCOUNTER'

"PRMTZ8 via KDC101 or TDC001"
THORLABS_STAGE_UNIT_PRMTZ8			= "deg"
THORLABS_STAGE_CALFACTOR_POS_PRMTZ8	= 1919.6418578623391    # step/deg
THORLABS_STAGE_CALFACTOR_VEL_PRMTZ8	= 42941.66              # deg/s
THORLABS_STAGE_CALFACTOR_ACC_PRMTZ8	= 14.66                 # deg/s^2
THORLABS_STAGE_POS_REQ_COM_PRMTZ8	= 'MGMSG_MOT_REQ_ENCCOUNTER'
THORLABS_STAGE_POS_GET_COM_PRMTZ8	= 'MGMSG_MOT_GET_ENCCOUNTER'


stage_axes = {"x":["x","X"],"y":["y","Y"],"z":["z","Z"],"r":["r","R"]}
stage_limits = {'x':[0,250],'y':[0,250],'z':[0,250],'r':[-360,360]}
#################################################################


velocity     =  2     # mm/s  (recommended: <5 mm/s)
acceleration =  1     # mm/s^2
# DO NOT VELOCITY > 5!!! THE STAGE WILL STOP SYNCING WITH PC LEADING TO ERRORS IN POSITION


"""
TODO: operation modes:
    home   = move to home position
    zigzag = move in zigzag path in x and y directions (2D projection)
"""

class ECTstage(Satellite):
    """Stage movements in XYZR"""

    #
    def do_initializing(self, cnfg: Configuration) -> str:
        """
        Configure the Satellite and ThorLab stages
        """

        # load conf file and save into ECTstage object
        config_file = cnfg["config_file"]
        with open(config_file, 'r') as f:
            self.conf = toml.load(f)

        # initialise stage
        if "x" in self.conf["run"]["active_axes"]: self.stage_x = self._init_stage("x")
        if "y" in self.conf["run"]["active_axes"]: self.stage_y = self._init_stage("y")
        if "z" in self.conf["run"]["active_axes"]: self.stage_z = self._init_stage("z")
        if "r" in self.conf["run"]["active_axes"]: self.stage_r = self._init_stage("r")

        # verbose
        for axis in self.conf["run"]["active_axes"]:
            self._get_stage_info(axis)

        return "Initialized"


    def do_reconfigure(self, cnfg: Configuration) -> str:
        """
        set new position
        """

        # load conf file and save into ECTstage object
        config_file = cnfg["config_file"]
        with open(config_file, 'r') as f:
            self.conf = toml.load(f)

        return "Reconfigured from conf file"


    # def do_launching(self, conf: Configuration) -> str:
    def do_launching(self, payload: Any) -> str:
        """
        move stage to start position (home)
        """
        for axis in self.conf["run"]["active_axes"]:
            print(axis,"stage moving to start point (home)")
            self._move_stage(axis, self.conf[axis]["home_position"],False)

        return "Launched"


    def do_landing(self, payload: Any) -> str:
        """
        move back to home
        """
        for axis in self.conf["run"]["active_axes"]:
            print(axis,"stage moving to start point (home)")
            self._move_stage(axis, self.conf[axis]["home_position"],False)
        return "Landed"



    def do_starting(self, payload: Any) -> str:
        """
        move to data taking position
        """
        for axis in self.conf["run"]["active_axes"]:
            print(axis,"stage moving to start point (home)")
            self._move_stage(axis, self.conf[axis]["home_position"],False)

        return "stage moved"


    def do_run(self, payload: Any) -> str:
        """The main run routine.
        Here, the main part of the mission would be performed.
        """
        print("data acquiring")

        pos = {}
        for axis in self.conf["run"]["active_axes"]:
            if type(self.conf["run"]["pos_"+axis]) == int :
                pos[axis] = np.ones(3)*self.conf["run"]["pos_"+axis]

        if "x" not in self.conf["run"]["active_axes"] : pos["x"] = [""]
        if "y" not in self.conf["run"]["active_axes"] : pos["y"] = [""]
        if "z" not in self.conf["run"]["active_axes"] : pos["z"] = [""]
        if "r" not in self.conf["run"]["active_axes"] : pos["r"] = [""]

        print(pos)
        for pos_z in pos["z"]:
            self._move_stage("z", pos_z,True)

            for pos_r in pos["r"]:
                self._move_stage("r", pos_r,True)

                for pos_x,pos_y in self._generate_zigzagPath(pos["x"],pos["y"]):
                   self._move_stage("x", pos_x,True)
                   self._move_stage("y", pos_y,True)
                   self.log.info(f"Move to {pos_x} {pos_y} {pos_r} {pos_z}")
                   time.sleep(3)

        return "Finished acquisition."


    def do_stop(self, payload: Any) -> str:
        flag = input("Move home? (y/n)")
        if flag in ["y","Y"]:
            for axis in self.conf["run"]["active_axes"]:
                self._move_stage(axis, self.conf[axis]["home_position"],False)
                print("stage moved")
        else: print("stage not moved")
        return "End of run"


    @cscp_requestable
    def stage_home(self,axis=None,request: CSCPMessage) -> tuple[str, Any, dict]:
        if axis!=None and axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage.home()
        else:
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(ax)
                stage.home()
        return "Stage moved home", None, {}



    @cscp_requestable
    def stage_stop(self,axis=None,request: CSCPMessage) -> tuple[str, Any, dict]:
        if axis!=None and axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            if stage.is_moving(): stage.stop()
        else:
            for ax in self.conf["run"]["active_axes"]:
                stage = self._stage_select(ax)
                if stage.is_moving(): stage.stop()

        return "Stage Stopped", None, {}

    @cscp_requestable
    def stage_disconnect(self,axis=None,request: CSCPMessage) -> tuple[str, Any, dict]:
        if axis!=None and axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
        else:
            for ax in self.conf["run"]["active_axes"]:
                stage = self._stage_select(ax)
        return "Stage disconnected", None, {}



    def _init_stage(self,axis):
        "initialise the ThorLabs motor stages"

        if (axis in stage_axes["x"] or axis in stage_axes["y"] or axis in stage_axes["z"]):
            stage = Thorlabs.KinesisMotor(conn=self.conf[axis]["port"],
                scale=(
                    THORLABS_STAGE_CALFACTOR_POS_LTS300,
                    THORLABS_STAGE_CALFACTOR_VEL_LTS300,
                    THORLABS_STAGE_CALFACTOR_ACC_LTS300
                ))

            stage.setup_velocity(channel=self.conf[axis]["chan"],
                max_velocity=self.conf[axis]["velocity"],
                acceleration=self.conf[axis]["acceleration"])

        elif (axis in stage_axes["r"]):
            stage = Thorlabs.KinesisMotor(conn=self.conf[axis]["port"],
                scale=(
                    THORLABS_STAGE_CALFACTOR_POS_PRMTZ8,
                    THORLABS_STAGE_CALFACTOR_VEL_PRMTZ8,
                    THORLABS_STAGE_CALFACTOR_ACC_PRMTZ8
                ))

            stage.setup_velocity(channel=self.conf[axis]["chan"],
                max_velocity=self.conf[axis]["velocity"],
                acceleration=self.conf[axis]["acceleration"])

        else:
            print("axis not found.Exiting application")
            exit()

        return stage


    def _move_stage(self,axis,position,save):
        """
        move stage to home position
        """
        if position == "":
            return 0
        else:
            stage = self._stage_select(axis)
        try:
            stage.move_to(position,channel=self.conf[axis]["chan"])
            while stage.is_moving():
                print("current {} pos:{} time:{}    \r".format(axis,
                    stage.get_position(channel=self.conf[axis]["chan"],
                    scale=True),
                    time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)))
        except KeyboardInterrupt:
            stage.stop()

    def _get_stage_info(self,axis):
        """
        prints many parameters
        """
        print(axis,"axis:")
        stage = self._stage_select(axis)
        print(stage.setup_velocity(channel=self.conf[axis]["chan"]))
        print(stage.get_full_info())
        print(stage.get_all_axes())
        print(stage.get_scale())
        print("")

    def _stage_select(self,axis):
        if   (axis in stage_axes["x"]): return self.stage_x
        elif (axis in stage_axes["y"]): return self.stage_y
        elif (axis in stage_axes["z"]): return self.stage_z
        elif (axis in stage_axes["r"]): return self.stage_r
        else:
            print("axis not found")
            exit()

    def _list_positions(self,pos_range):
        """
        creates an list of positions
        """
        list = np.round(np.arange(pos_range[0],pos_range[1]+1,pos_range[2]),3)  # mm
        print(list)
        return list

    def _generate_zigzagPath(self,pos_x,pos_y):
        """
        creates the zig-zag positions
        """
        posX_list = self._list_positions(pos_x)
        posY_list = self._list_positions(pos_y)
        XYarray = [[(i, j) for j in posY_list] for i in posX_list]
        for index in range(len(XYarray)):
            if index % 2 == 1:  # Check if the row index is odd (i.e., every second row)
                XYarray[index].sort(reverse=True)
        XYarray = [item for sublist in XYarray for item in sublist]
        print(XYarray)
        return XYarray


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a
    basis for custom implementations.

    """
    print(pll.list_backend_resources("serial"))


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
