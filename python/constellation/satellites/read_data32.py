import ctypes

import numpy as np


def read_data32(start=0, stop=16384, channel=1):
    # Define the struct in Python
    class Array(ctypes.Structure):
        _fields_ = [("data", ctypes.POINTER(ctypes.c_uint32))]

    # Load the shared library
    lib = ctypes.CDLL("/root/read_data32.so")

    # Define the argument and return types of the function
    lib.readData.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.readData.restype = Array

    # Define register offset depending onc channel
    if channel == 1:
        offset = 1074855936
    elif channel == 2:
        offset = 1074921472
    elif channel == 3:
        offset = 1075904512
    elif channel == 4:
        offset = 1075970048

    # Call the C function
    result = lib.readData(0, 16384, offset)
    lib.freeData(result.data)

    # Convert the result to a NumPy array
    data_array = np.ctypeslib.as_array(result.data, shape=(16384,))[start:stop]

    return data_array


if __name__ == "__main__":
    data = read_data32(start=0, stop=16384, channel=1)
    print(data)
