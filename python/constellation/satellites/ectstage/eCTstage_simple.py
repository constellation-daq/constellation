#!/usr/bin/env python3.11
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

# THORLABS LTS300. DO NOT CHANGE
THORLABS_STAGE_CALFACTOR_POS_LTS300 = 409600.0      #step/mm
THORLABS_STAGE_CALFACTOR_VEL_LTS300	= 21987328.0    #usteps/s
THORLABS_STAGE_CALFACTOR_ACC_LTS300	= 4506.0        #usteps/s^2
THORLABS_STAGE_POS_REQ_COM_LTS300	= 'MGMSG_MOT_REQ_POSCOUNTER'
THORLABS_STAGE_POS_GET_COM_LTS300   = 'MGMSG_MOT_GET_POSCOUNTER'

velocity     =  2     # mm/s  (recommended: <5 mm/s)
acceleration =  1     # mm/s^2
# Note: LTS300C maximum velocity = 439720896 steps/s and acceleration=90071 steps/s^2
# DO NOT VELOCITY > 5!!! THE STAGE WILL STOP SYNCING WITH PC LEADING TO ERRORS IN POSITION


# x-axis
config = {'x':{
        "chan" : 0,
        "port" : "/dev/ttyUSB0",
        "vel"  : velocity,
        "acc"  : acceleration,
        "pos_range" : [10,30,10]},

    # y-axis
        'y':{
        "chan" : 0,
        "port" : "/dev/ttyUSB1",
        "vel"  : velocity,
        "acc"  : acceleration,
        "pos_range" : [10,30,10]}
    }

def _list_positions(axis):
    print(config[axis]["pos_range"][0],config[axis]["pos_range"][1],config[axis]["pos_range"][2])
    list = np.round(np.arange(config[axis]["pos_range"][0],
        config[axis]["pos_range"][1]+1,config[axis]["pos_range"][2]),3)  # mm
    print(list)
    return list

def _make_zigzagPath():
    posX_list = _list_positions("x")
    posY_list = _list_positions("y")
    XYarray = [[(i, j) for j in posY_list] for i in posX_list]
    for index in range(len(XYarray)):
        if index % 2 == 1:  # Check if the row index is odd (i.e., every second row)
            XYarray[index].sort(reverse=True)
    XYarray = [item for sublist in XYarray for item in sublist]
    print(XYarray)
    return XYarray


print(pll.list_backend_resources("serial"))
stageX = Thorlabs.KinesisMotor(conn=config['x']["port"],scale=(
    THORLABS_STAGE_CALFACTOR_POS_LTS300,
    THORLABS_STAGE_CALFACTOR_VEL_LTS300,
    THORLABS_STAGE_CALFACTOR_ACC_LTS300
))
stageX.setup_velocity(channel=config["x"]["chan"],max_velocity=config["x"]["vel"],
    acceleration=config["x"]["acc"])
print(stageX.setup_velocity(channel=config["x"]["chan"]))
print(stageX.get_full_status())


stageY = Thorlabs.KinesisMotor(conn=config['y']["port"],scale=(
    THORLABS_STAGE_CALFACTOR_POS_LTS300,
    THORLABS_STAGE_CALFACTOR_VEL_LTS300,
    THORLABS_STAGE_CALFACTOR_ACC_LTS300
))
stageY.setup_velocity(channel=config["y"]["chan"],max_velocity=config["y"]["vel"],
    acceleration=config["y"]["acc"])
print(stageY.setup_velocity(channel=config["y"]["chan"]))
print(stageY.get_full_status())

stageX.move_to(0,channel=config["x"]["chan"])
stageY.move_to(0,channel=config["y"]["chan"])

XYarray = _make_zigzagPath()
for posX,posY in XYarray:
    try:
        print("x-stage moving to",posX)
        stageX.move_to(posX,channel=config["x"]["chan"])
        while stageX.is_moving():
            print("current X pos:{}     \r".format(stageX.get_position(channel=config["x"]["chan"],scale=True)))
    except KeyboardInterrupt:
        stageX.stop()
        break

    try:
        print("y-stage moving to",posY)
        stageY.move_to(posY,channel=config["y"]["chan"])
        while stageY.is_moving():
            print("current Y pos:{}     \r".format(stageY.get_position(channel=config["y"]["chan"],scale=True)))
    except KeyboardInterrupt:
        stageY.stop()
        break

    print("stage moved")


# # stageX.move_to(10,channel=chan_x)
# # while True:
# for pos_x in _list_positions("x"):
#     # pos_x = float(input("position:"))
#     stageX.move_to(pos_x,channel=chan_x)
#     while stageX.is_moving(channel=chan_x):
#         try:
#             print("current X pos:{}     \r".format(stageX.get_position(channel=chan_x,scale=True)))
#         except KeyboardInterrupt:
#             stageX.stop(immediately=True)
#     print("stage moved to",stageX.get_position(channel=chan_x,scale=True))

time.sleep(1)
stageX.move_to(0,channel=config["x"]["chan"])
stageY.move_to(0,channel=config["y"]["chan"])
stageX.close()
stageY.close()
    # flag = str(input("new run? (y/n):"))
    # if (flag !="y" or flag !="Y"):
    #     pass
    # else: stageX.close()


# pos_dict = {0:[10,50,1],
            # 1:[0,5,1],
            # 2:[5,5,1],
        # }
# pos_dic = {ch: [min,max,stepsize]}

#
# chan_axis = {0:"X",1:"Y",2:"Z"}
#
# config_dict = {"ttyPort_XYZ":"val1","ttyPort_R":"val2",
#         "home":{0:{"pos":1,"vel":1,"acc":1},
#                 1:{"pos":1,"vel":1,"acc":1},
#                 2:{"pos":1,"vel":1,"acc":1}}
#         # 3:{"home":[1,1,1]},
#         #channel: {home:[pos,vel,acc]}
#     }
#


# def do_tests():
#     print(pll.list_backend_resources("serial"))
#
# def _list_positions(chan):
#     list = np.round(np.arange(pos_dict[chan][0],
#         pos_dict[chan][1]+1,pos_dict[chan][2]),3)  # mm
#     return list
#
# def _make_zigzagPath():
#     posX_list = _list_positions(chan=0)
#     posY_list = _list_positions(chan=1)
#     XYarray = [[(i, j) for j in posY_list] for i in posX_list]
#     for index in range(len(XYarray)):
#         if index % 2 == 1:  # Check if the row index is odd (i.e., every second row)
#             XYarray[index].sort(reverse=True)
#     XYarray = [item for sublist in XYarray for item in sublist]
#     return XYarray
#
#
# def do_initialise(ttyPort_XYZ):
#     # stageXYZ = "mockstage"
#     stageXYZ = Thorlabs.KinesisMotor(conn=ttyPort_XYZ,scale="stage")
#     # take default scale (pos,vel,acc) from motor
#     # I think this is 1mm, 1mm/sec and 1mm/sec^2 for LTS300
#     print(stageXYZ.get_full_status())
#     print(stageXYZ.get_all_axes())
#     print(stageXYZ.get_scale())
#
#     # stageR = Thorlabs.KinesisMotor(ttyPort_R,scale="stage")
#     ## take default scale (pos,vel,acc) from motor
#     ## I think this is 1deg, 1deg/sec and 1deg/sec^2 for LTS300
#     # print(self.stageR.get_full_status())
#     # print(self.stageR.get_all_axes())
#     return stageXYZ
#
# def do_launching(stageXYZ):
#     """
#     set default starting position, velocity and acceleration
#     """
#     stageXYZ.setup_velocity(min_velocity=0,
#         max_velocity=config_dict["home"][0]["vel"],
#         acceleration=config_dict["home"][0]["acc"],
#         channel=0,
#         scale=True)
#
#     stageXYZ.setup_velocity(min_velocity=0,
#         max_velocity=config_dict["home"][1]["vel"],
#         acceleration=config_dict["home"][1]["acc"],
#         channel=1,
#         scale=True)
#
#     stageXYZ.setup_velocity(min_velocity=0,
#         max_velocity=config_dict["home"][2]["vel"],
#         acceleration=config_dict["home"][2]["acc"],
#         channel=2,
#         scale=True)
#     print("velocity and acceleration set")
#
#     home_pos = [config_dict["home"][0]["pos"],
#                 config_dict["home"][1]["pos"],
#                 config_dict["home"][2]["pos"]]
#
#     return home_pos
#
# def do_starting(stageXYZ,pos):
#     """
#     move to starting position
#     """
#     for chan in [0,1,2]:
#         print(chan,"stage moving to",pos[chan])
#
#         stageXYZ.move_to(position=pos[chan],channel=chan,scale=True)
#         while is_moving(channel=chan):
#             try:
#                 print("current X pos:",get_position(channel=chan,scale=True),"            \r",)
#             except KeyboardInterrupt:
#                 stageXYZ.stop(immediately=True)
#                 return True
#         print("stage moved")
#
#
#
# def do_run(AcquireData):
#     """The main run routine.
#     Here, the main part of the mission would be performed.
#     """
#     print("data acquiring")
#     os.sleep(0.005)
#     return "Finished acquisition."
#
# def do_stop(stageXYZ):
#     pass
#
# def do_landing(stageXYZ,ErrFlag,home_pos=None):
#     """
#     either move back to home or emergency stop
#     """
#     if ErrFlag==True:
#         print("An error has occured. Exiting program")
#         exit()
#     else:
#         ErrFlag = do_starting(stageXYZ,home_pos)
#         print("moved home")
#
#
# def main():
#     ErrFlag = False
#     do_tests()
#     stageXYZ = do_initialise(config_dict["ttyPort_XYZ"])
#     home_pos = do_launching(stageXYZ)
#     ErrFlag = do_starting(stageXYZ,home_pos)
#     if ErrFlag == True:do_landing(stageXYZ,ErrFlag)
#
#     # XYarray = make_zigzagPath()
#     # for posZ in list_positions(chan=2):
#     #     for posX,posY in XYarray:
#     #         ErrFlag = do_starting(stageXYZ,[posX,posY,posZ])
#     #         if ErrFlag == False:
#     #             do_run(AcquireData)
#     #             do_stop(stageXYZ)
#     #         else:
#     #             do_landing(stageXYZ,ErrFlag,home_pos)
#     # if ErrFlag==False: print("successful execution")
#     #
# main()
