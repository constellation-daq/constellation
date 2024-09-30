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

from constellation.core.datasender import DataSender

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


max_velocity =  20     # mm/s  (recommended: <5 mm/s)
max_aclrtn   =  10     # mm/s^2
# DO NOT VELOCITY > 15!!! THE STAGE WILL STOP MOVING AND SYNCING WITH PC MAY BE AFFECTED


"""
TODO: operation modes:
    home   = move to home position
    zigzag = move in zigzag path in x and y directions (2D projection)
"""

class ECTstage(DataSender):
    """Stage movements in XYZR"""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs,data_port=65123)

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
        
        # close stages 
        for axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage.close()
        
        # re-initialise stages
        if "x" in self.conf["run"]["active_axes"]: self.stage_x = self._init_stage("x")
        if "y" in self.conf["run"]["active_axes"]: self.stage_y = self._init_stage("y")
        if "z" in self.conf["run"]["active_axes"]: self.stage_z = self._init_stage("z")
        if "r" in self.conf["run"]["active_axes"]: self.stage_r = self._init_stage("r")
  
        # verbose
        for axis in self.conf["run"]["active_axes"]:
            self._get_stage_info(axis)
            
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
        Move stages to all positions while returning positions
        """
        print("data acquiring")

        pos = {}
        for axis in self.conf["run"]["active_axes"]:
            try:
                pos[axis] = self._list_positions(self.conf["run"]["pos_"+axis])
            except KeyError: 
                pos[axis] = self._list_positions(np.append(np.ones(2)*self.conf[axis]["home_position"], np.array([1]), axis=0))
        
                
        if 'z' in self.conf["run"]["active_axes"]:
            for pos_z in pos['z']:
                self._move_stage("z", pos_z,True)
                while self.stage_z.is_moving():
                    if not self._state_thread_evt.is_set():
                        self._send_data()
                        #self._print_data()
                        
                    else: 
                        self.stage_z.stop()
                        return "movement stopped"
                
                #self._send_data() 
                self._print_data()   
                

                if 'r' in self.conf["run"]["active_axes"]:
                    for pos_r in pos['r']:
                        self._move_stage("r", pos_r,True)
                        while self.stage_r.is_moving():
                            if not self._state_thread_evt.is_set():
                                self._send_data()
                                #self._print_data()
                            else: 
                                self.stage_r.stop()
                                return "movement stopped"
                        
                        #self._send_data() 
                        self._print_data()   
                
                
                if ('x' in self.conf["run"]["active_axes"] and 'y' in self.conf["run"]["active_axes"]):
                    for pos_x,pos_y in self._generate_zigzagPath(pos['x'],pos['y']):
                        self.log.info(f"Move to {pos_x} {pos_y} {pos_z} {pos_r}")
                        self._move_stage("x", pos_x,True)
                        while self.stage_x.is_moving():
                            if not self._state_thread_evt.is_set():
                                self._send_data()
                                #self._print_data()
                            else: 
                                self.stage_r.stop()
                                return "movement stopped"
                        
                        #self._send_data() 
                        self._print_data()   
                        
                        self._move_stage("y", pos_y,True)
                        while self.stage_y.is_moving():
                            if not self._state_thread_evt.is_set():
                                self._send_data()
                                #self._print_data()
                            else: 
                                self.stage_r.stop()
                                return "movement stopped"
                        
                        self._send_data() 
                        #self._print_data()      
                        
                        # measurement time
                        time.sleep(self.conf["run"]["time_per_point_s"])
                        
                else:
                    print("Either x or y axes not defined")
                    exit()
        
        print("stages moving to home position")
        for axis in self.conf["run"]["active_axes"]:
            self._move_stage(axis, self.conf[axis]["home_position"],False)
        print("stages moved")
        print("Run completed")
        
        return "Finished acquisition. Stop run to proceed"


    #def do_stop(self, payload: Any) -> str:
    #    flag = input("Move home? (y/n)")
    #    if flag in ["y","Y"]:
    #        for axis in self.conf["run"]["active_axes"]:
    #            self._move_stage(axis, self.conf[axis]["home_position"],False)
    #            print("stage moved")
    #    else: print("stage not moved")
    #    return "End of run"


    @cscp_requestable
    def go_origin(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage.home()
        else:
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(axis)
                stage.home()
        return "Stage moved to origin", None, {}
        
    @cscp_requestable
    def go_home_position(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage._move_stage(axis,self.conf[axis]["home_position"],False)
        else:
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(axis)
                stage._move_stage(axis,self.conf[axis]["home_position"],False)
        return "Stage moved to home position", None, {}

    @cscp_requestable
    def stage_stop(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            if stage.is_moving(): stage.stop()
        else: 
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(axis)
                if stage.is_moving(): stage.stop()
        return "Stage Stopped", None, {}
        
    @cscp_requestable
    def get_status(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            print( axis +": "+ str(stage.get_status()))
            return str(axis +": "+ stage.get_status()), None, {}
        else:
            val = ""
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(axis)
                val = val + axis +": "+ str(stage.get_status()) +"\n"
            print(val)
            return val, None, {}
        return "stage not identified", None, {}
        

    @cscp_requestable
    def blink(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if not isinstance(axis, str):
            # missing/wrong payload
            raise TypeError("Payload must be a stage axis identification string")

        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage.blink()
        else:
            return "stage not identified", None, {}

        return "Blinking {} axis".format(axis), None, {}

    
    @cscp_requestable
    def disconnect(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        if self.fsm.current_state_value in [SatelliteState.ORBIT, SatelliteState.NEW]:
            raise RuntimeError(
                f"Command not allowed in state '{self.fsm.current_state_value.name}'"
            )
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            stage.close()
            return "stage",axis,"closed! Reinitialize to reconnect", None, {}
        else:
            for ax in self.conf["run"]["active_axes"]:
                stage = self._stage_select(ax)
                stage.close()
                print("stage",ax,"closed! Reinitialize to reconnect")
            return "stages closed! Reinitialize to reconnect", None, {}
    
    @cscp_requestable
    def get_fsm_state(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        print(self.fsm.current_state_value.name)
        return self.fsm.current_state_value.name, None, {}
    

    @cscp_requestable
    def get_position(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            return str(stage.get_position(channel=self.conf[axis]["chan"],scale=True)), None, {}
        else:
            val = ""
            for axis in self.conf["run"]["active_axes"]:
                stage = self._stage_select(axis)
                val = val + " "+axis+ ":"+str(stage.get_position(channel=self.conf[axis]["chan"],scale=True))
                if axis in stage_axes["r"]:    val = val + " deg;"
                else: val = val + " mm;"
            return val, None, {}
       
    
    def _get_position(self,axis) -> tuple[str, Any, dict]:
        if axis!=None and axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            try: return stage.get_position(channel=self.conf[axis]["chan"],scale=True)
            except NameError: return -303.303
            
    
    def _init_stage(self,axis):
        "initialise the ThorLabs motor stages"

        if (axis in stage_axes["x"] or axis in stage_axes["y"] or axis in stage_axes["z"]):
            stage = Thorlabs.KinesisMotor(conn=self.conf[axis]["port"],
                scale=(
                    THORLABS_STAGE_CALFACTOR_POS_LTS300,
                    THORLABS_STAGE_CALFACTOR_VEL_LTS300,
                    THORLABS_STAGE_CALFACTOR_ACC_LTS300
                ))
            
            if self.conf[axis]["velocity"] > max_velocity:
                raise KeyError("Velocity must be smaller than {}".format(max_velocity))
            if self.conf[axis]["acceleration"] > max_aclrtn:
                raise KeyError("Acceleration must be smaller than {}".format(max_aclrtn))
            
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

        else:
            print("axis not found.Exiting application")
            exit()

        return stage

            
    def _move_stage(self,axis,position,save):
        """
        move stage
        """
        if position == "":
            return 0
        else:
            stage = self._stage_select(axis)
        stage.move_to(position,channel=self.conf[axis]["chan"])
        
    def _send_data(self):
        meta = {}
        payload = np.array([
             time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW),
             self._get_position("x"),self._get_position("y"),
             self._get_position("z"),self._get_position("r")], dtype=np.float64)
        self.data_queue.put((payload.tobytes(), meta))
        
    def _print_data(self):
        print("time:",time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW),
             " x:",self._get_position("x")," y:",self._get_position("y"),
             " z:",self._get_position("z")," r:",self._get_position("r"))
    
    
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

    def _generate_zigzagPath(self,posX_list,posY_list):
        """
        creates the zig-zag positions
        """
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
