#!/usr/bin/env python3
"""
Stage movement in X,Y,Z,theta(R) for electronCT
author: Malinda de Silva (@desilvam)

TODO: 
re-implement the loops
set position max

"""

import time
import random
from typing import Any
import itertools
import numpy as np
import toml
from threading import Lock, Thread, Event

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
#################################################################


max_velocity =  20     # mm/s  (recommended: <5 mm/s)
max_aclrtn   =  10     # mm/s^2
stage_max = {'x':250,'y':250,'z':250,'r':360}

# DO NOT SET VELOCITY > 20!!! THE STAGE WILL STOP MOVING AND SYNCING WITH PC MAY BE AFFECTED
# THIS THRESHOLD WAS TESTED ON X-STAGE AFTER X,Y,Z,R STAGES WERE MOUNTED TOGETHER. 
# COULD FURTHER REDUCE IF LOAD IS HEAVIER

##################################################################


class ECTstage(DataSender):
    """Stage movements in XYZR"""
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs,data_port=65123)
        self._lock = Lock()

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
        
        # close stages and delete object
        for axis in ["x","y","z","r"]:
            stage = self._stage_select(axis)
            if stage is not None:
                with self._lock:
                    stage.close()
                del stage
        
    
        # re-initialise stages
        if "x" in self.conf["run"]["active_axes"]: self.stage_x = self._init_stage("x")
        if "y" in self.conf["run"]["active_axes"]: self.stage_y = self._init_stage("y")
        if "z" in self.conf["run"]["active_axes"]: self.stage_z = self._init_stage("z")
        if "r" in self.conf["run"]["active_axes"]: self.stage_r = self._init_stage("r")
  
        # verbose
        for axis in self.conf["run"]["active_axes"]:
            self._get_stage_info(axis)
            
        return "Reconfigured from conf file"


        

    def do_launching(self, payload: Any) -> str:
        """
        move stage to start position (home)
        """

        for axis in self.conf["run"]["active_axes"]:
            if self.conf[axis]["start_position"]>stage_max[axis]:
                raise KeyError("Home position in [{}] must be smaller than {}".format(axis,stage_max[axis]))
            #para = "pos_"+axis
            #if para in self.conf[axis]: print("parameter",axis,"exists")
        print("test")
        self._move_to_start()

        return "Launched"


    def do_landing(self, payload: Any) -> str:
        """
        move back to home
        """
        return "Landed"



    def do_starting(self, payload: Any) -> str:
        """
        move to data taking position
        """
        self._move_to_start()

        return "stage moved"

    def send_positions_background(self, event: Event):
        while not event.is_set():
            payload = np.array([
                time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW),
                self._get_position("x"), self._get_position("y"),
                self._get_position("z"), self._get_position("r")], dtype=np.float64)
            meta = {"dtype": f"{payload.dtype}"}
            self.data_queue.put((payload.tobytes(), meta))
            self.log.debug(f"Position: x={payload[1]}, y={payload[2]}, z={payload[3]}, r={payload[4]}, t={payload[0]}")
            event.wait(timeout=0.5)

    def do_run(self, payload: Any) -> str:
        """The main run routine.
        Move stages to all positions while returning positions
        """
        print("Run has begun")
        self.log.info("Run has begun")
        
        # Send data periodically in background until end of run
        bg_event = Event()
        bg_thread = Thread(target=self.send_positions_background, args=(bg_event,))
        bg_thread.start()

        pos = {}
        for axis in self.conf["run"]["active_axes"]:
            try:
                pos[axis] = self._list_positions(self.conf["run"]["pos_"+axis])
            except KeyError: 
                pos[axis] = self._list_positions(np.append(np.ones(2)*self.conf[axis]["start_position"], np.array([1]), axis=0))
        
                
        if 'z' in self.conf["run"]["active_axes"]:
            for pos_z in pos['z']:
                self._move_stage("z", pos_z)
                self._wait_until_stage_stops_moving(self.stage_z)
                if self._state_thread_evt.is_set():
                    break

                if 'r' in self.conf["run"]["active_axes"]:
                    for pos_r in pos['r']:
                        self._move_stage("r", pos_r)            
                        self._wait_until_stage_stops_moving(self.stage_r)
                        if self._state_thread_evt.is_set():
                            break
                
                
                        if ('x' in self.conf["run"]["active_axes"] and 'y' in self.conf["run"]["active_axes"]):
                            for pos_x,pos_y in self._generate_zigzagPath(pos['x'],pos['y']):
                                self.log.info(f"Move to {pos_x} {pos_y} {pos_z} {pos_r}")
                                self._move_stage("y", pos_y)
                                self._wait_until_stage_stops_moving(self.stage_y)
                                if self._state_thread_evt.is_set():
                                    break
                                
                                self._move_stage("x", pos_x)
                                self._wait_until_stage_stops_moving(self.stage_x)
                                if self._state_thread_evt.is_set():
                                    break

                                # measurement time
                                time.sleep(self.conf["run"]["stop_time_per_point_s"])
                                
                        else:
                            raise Exception("Either x or y axes not defined")
        
        print("exited loop")
        run_interrupted = self._state_thread_evt.is_set()
        if run_interrupted:
            self.log.info("Run has been interrupted")
        
        #while not self._state_thread_evt.is_set():
        #    time.sleep(self.conf["run"]["readout_freq_s"])

        bg_event.set()
        bg_thread.join()
        self.log.debug("Background thread joined")

        if run_interrupted:
            return "Run has been interrupted"
        else:
            self.log.info("Run has completed")
            self._move_to_start()
            return "Finished acquisition. Stop run to proceed"
            
    
    @cscp_requestable
    def go_home(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Goes back to the stage defined home position. 
        Ideally should be 0 mm for linear stages and 0 deg for rotational stage
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                with self._lock: stage.home()
            
        return "Stage moved to origin", None, {}
    
    
       
    @cscp_requestable
    def go_start_position(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        move to start position
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        self._move_to_start(axis=axis)
        
        return "Stage moved to home position", None, {}

    @cscp_requestable
    def stage_stop(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Stops stages. Only works outside of main loop.
        FYI: For emergency stop while within run loop, use stop()
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                if self._stage_moving(stage):
                    self._stage_stop(stage)
        
        return "Stage Stopped", None, {}

        
    @cscp_requestable
    def get_status(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Returns stage status
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        
        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + axis +": "+ str(stage.get_status())+" "
        print(val)
        self.log.info(val)
        return "Returned status: see logs/eCT terminal", None, {}
              
        

    @cscp_requestable
    def blink(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Blink test stages
        args: `axis`
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"] and axis!=None:
            stage = self._stage_select(axis)
            with self._lock:
                stage.blink()
            return "Blinking {} axis".format(axis), None, {}
        else:
            return "Stage not found", None, {}
            
        #if not isinstance(axis, str):
        #    # missing/wrong payload
        #    raise TypeError("Payload must be a stage axis identification string")

        
    @cscp_requestable
    def disconnect(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Disconnects the stages
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                with self._lock:
                   stage.close()
                self.log.info("stage",ax,"closed! Reinitialize to reconnect")
            return "stage closed! Reinitialize to reconnect", None, {}

    def _disconnect_is_allowed(self, request: CSCPMessage) -> bool:
        return self.fsm.current_state_value in [SatelliteState.INIT]
                
    @cscp_requestable
    def get_vel_acc_params(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Get stage max, min velocities and acceleration
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + axis+ ":"+str(stage.get_velocity_parameters(channel=self.conf[axis]["chan"],scale=True))
                if axis in stage_axes["r"]:    val = val + " deg;"
                else: val = val + " mm;"   
        return val, None, {}
        
    @cscp_requestable
    def get_position(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Get stage position
        args: `axis` (optional). else: applies to all stages
        """
        axis = request.payload
        if axis not in self.conf["run"]["active_axes"] and axis!=None:
            return "Stage not found", None, {}
        val = ""
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                stage = self._stage_select(ax)
                with self._lock:
                    val = val + axis+ ":"+str(self._get_position(axis))
                if axis in stage_axes["r"]:    val = val + " deg;"
                else: val = val + " mm;"   
        return val, None, {}
        
                
    @cscp_requestable
    def disable_axis(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Disable axis
        args: `axis` 
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            self._enable_axis(axis,enable=False)
            return "{} stage disabled".format(axis), None, {}
                
        else: return "Stage not found! Not disabled", None, {}
        
    @cscp_requestable
    def enable_axis(self,request: CSCPMessage) -> tuple[str, Any, dict]:
        """
        Disable axis
        args: `axis` 
        """
        axis = request.payload
        if axis in self.conf["run"]["active_axes"]:
            self._enable_axis(axis,enable=True)
            return "{} stage enabled".format(axis), None, {}
                
        else: return "Stage not found! Not enabled", None, {}
            
    
    def _init_stage(self,axis):
        "initialise the ThorLabs motor stages"

        with self._lock:
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
                raise KeyError("Axis not found!")

        return stage

    def _enable_axis(self,axis,enable=True):
        stage = self._stage_select(axis)
        with self._lock:
            stage._enable_channel(enabled=enable)
    
    def _get_position(self,axis):
        if axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            try:
                with self._lock:
                    return stage.get_position(channel=self.conf[axis]["chan"],scale=True)
            except NameError:
                return np.nan
        else: raise NameError("Stage not found")

            
    def _move_stage(self,axis,position):
        """
        move stage
        """
        if position > stage_max[axis]:
            raise KeyError("Position must be smaller than {}".format(stage_max[axis]))
        stage = self._stage_select(axis)
        with self._lock:
            stage.move_to(position,channel=self.conf[axis]["chan"])
        
        
    def _wait_until_stage_stops_moving(self, stage):
        """
        wait while stage moves
        """
        while self._stage_moving(stage):
            if not self._state_thread_evt.is_set():
                time.sleep(self.conf["run"]["readout_freq_s"])
            else:
                self._stage_stop(stage)
                break
    
    def _move_to_start(self,axis=None):
        """
        move to start positions defined in config file
        """
        for ax in self.conf["run"]["active_axes"]:
            if ax==axis or axis==None:
                print(axis,"stage moving to start point (home)")
                self.log.info(f"{ax} stage moving to start point (home)")
                stage = self._stage_select(ax)
                self._move_stage(ax, self.conf[ax]["start_position"])
            else: raise KeyError("Stage not found")
        self._move_lock()
        print("stages moved")
        self.log.info(f"stages moved")

    def _stage_moving(self, stage):
        with self._lock:
            return stage.is_moving()

    def _stage_stop(self, stage):
        with self._lock:
            return stage.stop()

    def _move_lock(self):
        for axis in self.conf["run"]["active_axes"]:
            stage = self._stage_select(axis)
            while self._stage_moving(stage):
                time.sleep(self.conf["run"]["readout_freq_s"])
    
    def _get_stage_info(self,axis):
        """
        prints many parameters
        """
        print(axis,"axis:")
        stage = self._stage_select(axis)
        with self._lock:
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
        else: raise KeyError("axis not found")
            

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
