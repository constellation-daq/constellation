# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_random_transmitter')
  subdir_done()
endif

satellite_type = 'RandomTransmitter'

satellite_sources = files(
  'RandomTransmitterSatellite.cpp',
)

satellite_dependencies = []

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
