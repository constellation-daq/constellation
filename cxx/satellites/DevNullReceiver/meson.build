# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_dev_null_receiver')
  subdir_done()
endif

satellite_type = 'DevNullReceiver'

satellite_sources = files(
  'DevNullReceiverSatellite.cpp',
)

satellite_dependencies = []

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
