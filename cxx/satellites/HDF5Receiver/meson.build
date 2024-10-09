# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_hdf5_receiver')
  subdir_done()
endif

highfive_dep = dependency('HighFive')

satellite_type = 'HDF5Receiver'

satellite_sources = files(
  'HDF5ReceiverSatellite.cpp',
)

satellite_dependencies = [highfive_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
