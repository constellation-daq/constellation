#!/usr/bin/env python3
"""
Stage movement in X,Y,Z,theta for electronCT
author: Malinda de Silva (@ldesilva)
"""

import time,os
import random
from typing import Any
import itertools
import numpy as np

import pylablib as pll
from pylablib.devices import Thorlabs

def do_tests():
    print(pll.list_backend_resources("serial"))

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

def list_positions(chan):
    list = np.round(np.arange(pos_dict[chan][0],
        pos_dict[chan][1]+1,pos_dict[chan][2]),3)  # mm
    return list

def make_zigzagPath():
    posX_list = list_positions(chan=0)
    posY_list = list_positions(chan=1)
    XYarray = [[(i, j) for j in posY_list] for i in posX_list]
    for index in range(len(XYarray)):
        if index % 2 == 1:  # Check if the row index is odd (i.e., every second row)
            XYarray[index].sort(reverse=True)
    XYarray = [item for sublist in XYarray for item in sublist]
    return XYarray


def do_initialise(ttyPort_XYZ,ttyPort_R):
    stageXYZ = "mockstage"
    stageXYZ = Thorlabs.KinesisMotor(conn=ttyPort_XYZ,scale="stage")
    # take default scale (pos,vel,acc) from motor
    # I think this is 1mm, 1mm/sec and 1mm/sec^2 for LTS300
    print(stageXYZ.get_full_status())
    print(stageXYZ.get_all_axes())
    print(stageXYZ.get_scale())

    # stageR = Thorlabs.KinesisMotor(ttyPort_R,scale="stage")
    ## take default scale (pos,vel,acc) from motor
    ## I think this is 1deg, 1deg/sec and 1deg/sec^2 for LTS300
    # print(self.stageR.get_full_status())
    # print(self.stageR.get_all_axes())
    return stageXYZ

def do_launching(stageXYZ):
    """
    set default starting position, velocity and acceleration
    """
    stageXYZ.setup_velocity(min_velocity=0,
        max_velocity=config_dict["home"][0]["vel"],
        acceleration=config_dict["home"][0]["acc"],
        channel=0,
        scale=True)

    stageXYZ.setup_velocity(min_velocity=0,
        max_velocity=config_dict["home"][1]["vel"],
        acceleration=config_dict["home"][1]["acc"],
        channel=1,
        scale=True)

    stageXYZ.setup_velocity(min_velocity=0,
        max_velocity=config_dict["home"][2]["vel"],
        acceleration=config_dict["home"][2]["acc"],
        channel=2,
        scale=True)
    print("velocity and acceleration set")

    home_pos = [config_dict["home"][0]["pos"],
                config_dict["home"][1]["pos"],
                config_dict["home"][2]["pos"]]

    return home_pos

def do_starting(stageXYZ,pos):
    """
    move to starting position
    """
    for chan in [0,1,2]:
        print(chan,"stage moving to",pos[chan])

        stageXYZ.move_to(position=pos[chan],channel=chan,scale=True)
        while is_moving(channel=chan):
            try:
                print("current X pos:",get_position(channel=chan,scale=True),"            \r",)
            except KeyboardInterrupt:
                stageXYZ.stop(immediately=True)
                return True
        print("stage moved")



def do_run(AcquireData):
    """The main run routine.
    Here, the main part of the mission would be performed.
    """
    print("data acquiring")
    os.sleep(0.005)
    return "Finished acquisition."

def do_stop(stageXYZ):
    pass

def do_landing(stageXYZ,ErrFlag,home_pos=None):
    """
    either move back to home or emergency stop
    """
    if ErrFlag==True:
        print("An error has occured. Exiting program")
        exit()
    else:
        ErrFlag = do_starting(stageXYZ,home_pos)
        print("moved home")


def main():
    ErrFlag = False
    do_tests()
    stageXYZ = do_initialise(config_dict["ttyPort_XYZ"],config_dict["ttyPort_R"])
    home_pos = do_launching(stageXYZ)
    ErrFlag = do_starting(stageXYZ,home_pos)
    if ErrFlag == True:do_landing(stageXYZ,ErrFlag)

    XYarray = make_zigzagPath()
    for posZ in list_positions(chan=2):
        for posX,posY in XYarray:
            ErrFlag = do_starting(stageXYZ,[posX,posY,posZ])
            if ErrFlag == False:
                do_run(AcquireData)
                do_stop(stageXYZ)
            else:
                do_landing(stageXYZ,ErrFlag,home_pos)
    if ErrFlag==False: print("successful execution")

main()
