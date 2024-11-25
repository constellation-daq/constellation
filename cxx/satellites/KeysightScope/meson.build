# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

#if not get_option('satellite_keysight_scope')
#  subdir_done()
#endif

satellite_type = 'KeysightScope'

satellite_sources = files(
  'KeysightScopeSatellite.cpp',
)

# Asio used for IP communication
satellite_dependencies = [asio_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
