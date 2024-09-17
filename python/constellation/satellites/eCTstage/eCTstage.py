#!/usr/bin/env python3
"""
Stage movement in X,Y,Z,theta for electronCT
author: Malinda de Silva (@ldesilva)
"""

import time
import random
from typing import Any

from constellation.core.satellite import Satellite, SatelliteArgumentParser
from constellation.core.configuration import Configuration
from constellation.core.fsm import SatelliteState

from constellation.core.base import setup_cli_logging
from constellation.core.base import EPILOG

from constellation.core.cscp import CSCPMessage
from constellation.core.commandmanager import cscp_requestable

import pylablib as pll
from pylablib.devices import Thorlabs


chan_axis = {0:"X",1:"Y",2:"Z"}

config_dict = {"ttyPort_XYZ":"val1","ttyPort_R":"val2",
        "home":{0:{"pos":1,"vel":1,"acc":1},
                1:{"pos":1,"vel":1,"acc":1},
                2:{"pos":1,"vel":1,"acc":1}}
        # 3:{"home":[1,1,1]},
        #channel: {home:[pos,vel,acc]}
    }

pos_dict = {0:[0,5,1],
            1:[0,5,1],
            2:[5,5,1],
        }
# pos_dic = {ch: [min,max,stepsize]}

single_pos = {0:10,1:10,2:10}

stage_op_mode = "single"
"""
operatiom modes:
    home   = move to home position
    zigzag = move in zigzag path in x and y directions (2D projection)
"""

class Stage:
    """
    stage from ThorLabs
    """
    def __init__(self,conn,scale):
        self.stage = Thorlabs.KinesisMotor(conn=conn,scale=scale)

    def _set_target_pos(self,pos):
        self.pos = pos

    def _get_target_pos(self):
        return pos

    def _list_positions(chan):
        """
        Gives a list of positions for a selected channel (axis or rotation)
        """
        list = np.round(np.arange(pos_dict[chan][0],
            pos_dict[chan][1]+1,pos_dict[chan][2]),3)  # mm
        return list

    def _generate_zigzagPath(posX_list,posY_list):
        XYarray = [[(i, j) for j in posY_list] for i in posX_list]
        for index in range(len(XYarray)):
            if index % 2 == 1:  # Check if the row index is odd (i.e., every second row)
                XYarray[index].sort(reverse=True)
        XYarray = [item for sublist in XYarray for item in sublist]
        return XYarray


class eCTstage(Satellite):
    """Stage movements in XYZR"""

    def do_initializing(self, config: Configuration) -> str:
        """
        Configure the Satellite and any associated hardware

        requires: velocity, acceleration, home positions

        """
        self.stageXYZ = "mockstage"
        '''
        stageXYZ = Stage(conn=ttyPort_XYZ,scale="stage")
        # take default scale (pos,vel,acc) from motor
        # I think this is 1mm, 1mm/sec and 1mm/sec^2 for LTS300
        print(stageXYZ.stage.get_full_status())
        print(stageXYZ.stage.get_all_axes())
        print(stageXYZ.stage.get_scale())

        # Set velocity and acceleration for X,Y,Z axes
        stageXYZ.stage.setup_velocity(min_velocity=0,
            max_velocity=config_dict["home"][0]["vel"],
            acceleration=config_dict["home"][0]["acc"],
            channel=0,
            scale=True)

        stageXYZ.stage.setup_velocity(min_velocity=0,
            max_velocity=config_dict["home"][1]["vel"],
            acceleration=config_dict["home"][1]["acc"],
            channel=1,
            scale=True)

        stageXYZ.stage.setup_velocity(min_velocity=0,
            max_velocity=config_dict["home"][2]["vel"],
            acceleration=config_dict["home"][2]["acc"],
            channel=2,
            scale=True)
        print("velocity and acceleration set")
        '''
        return "Initialized"
        # return stageXYZ


    # def do_launching(self, config: Configuration) -> str:
    def do_launching(self, payload: Any) -> str:
        """
        move stage to home position
        """

        # saves home position for X,Y,Z
        stageXYZ._set_target_pos([config_dict["home"][0]["pos"],
                    config_dict["home"][1]["pos"],
                    config_dict["home"][2]["pos"]])

        pos = stageXYZ._get_target_pos()
        self._move_stage(pos)

        return "Lauch successful"
        # return home_pos

    def do_zigzag(self, config: Configuration) -> str:
        """
        move in zigzag
        """
        posX_list = Stage._list_positions(chan=0)
        posY_list = Stage._list_positions(chan=1)
        posZ_list = Stage._list_positions(chan=2)
        XYarray = Stage._generate_zigzagPath(posX_list,posY_list)

        for posZ in posZ_list:
            for posX,posY in XYarray:
                self.do_reconfigure(posXYZ=[posX,posY,posZ])
                msg = self.do_starting()
                if msg == "Interrupted!": return "zigzag loop ended"

                self.do_run()
                self.do_stop()

        return "zigzag loop ended"

    def do_starting(self, payload: Any):
        """
        move to starting position
        """
        pos = stageXYZ._get_target_pos()
        return self._move_stage(pos)

    def do_run(self, payload: Any) -> str:
        """The main run routine.
        Here, the main part of the mission would be performed.
        """
        print("data acquiring")
        time.sleep(0.005)
        print("Finished acquisition.")
        return self.do_stop()

    def do_stop(self, payload: Any) -> str:
        return("Finished acquisition")


    def do_landing(self, payload: Any) -> str:
        """
        either move back to home or emergency stop
        """
        flag = input("1. Exit (default)")
        if flag == 1: self.do_disconnect()
        else: self.do_disconnect()

        # saves home position for X,Y,Z
        stageXYZ._set_target_pos([config_dict["home"][0]["pos"],
                    config_dict["home"][1]["pos"],
                    config_dict["home"][2]["pos"]])

        pos = stageXYZ._get_target_pos()
        self._move_stage(pos)

    def do_interrupt(self, payload: Any):
        flag = input("Select option \n 1. safely land (default) \n 2. move home and land \n 3. disconnect immediately")
        if flag == 2:
            # saves home position
            stageXYZ._set_target_pos = [config_dict["home"][0]["pos"],
                        config_dict["home"][1]["pos"],
                        config_dict["home"][2]["pos"]]
            pos = stageXYZ._get_target_pos()
            self._move_stage(pos)
            self.do_landing()
        elif flag==3:
            self.do_disconnect()

    def do_disconnect(self):
        # stageXYZ.stage.close()
        print("stage disconnected")
        print("exiting application")
        exit()

    def do_reconfigure(self,posXYZ):
        """
        set new position
        """
        stageXYZ._set_target_pos(posXYZ[0],posXYZ[1],posXYZ[2])
        # print("Set new position")
        return "Set new position"


    def _move_stage(self,pos):
        for chan in [0,1,2]:
            print(chan_axis[chan],"stage moving to",pos[chan])

            # stageXYZ.stage.move_to(position=pos[chan],channel=chan,scale=True)
            while stageXYZ.stage.is_moving(channel=chan):
                try:
                    # print("current",chan_axis[chan],"pos:",stageXYZ.stage.get_position(channel=chan,scale=True),"            \r",)
                    time.sleep(1)
                except KeyboardInterrupt:
                    # stageXYZ.stage.stop(immediately=True)
                    print("Interrupted! Landing...")
                    return self.do_interrupt()
            # print("stage pos:",stageXYZ.stage.get_position(channel=chan,scale=True))
        return "moved stage"

    # @cscp_requestable
    # def get_attitude(self, request: CSCPMessage) -> (str, int, None):
    #     """Determine and return the space craft's attitude.
    #
    #     This is an example for a command that can be triggered from a Controller
    #     via CSCP. The return value of the function consists of a message, a
    #     payload value and an (optional) dictionary with meta information.
    #
    #     """
    #     # we cannot perform this command when not ready:
    #     if self.fsm.current_state_value in [
    #         SatelliteState.NEW,
    #         SatelliteState.ERROR,
    #         SatelliteState.DEAD,
    #         SatelliteState.initializing,
    #         SatelliteState.reconfiguring,
    #     ]:
    #         return "Canopus Star Tracker not ready", None, None
    #     return "Canopus Star Tracker locked and ready", self.device.get_attitude(), None


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
    s = eCTstage(**args)
    s.run_satellite()
