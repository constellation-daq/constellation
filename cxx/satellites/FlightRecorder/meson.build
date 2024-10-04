# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build satellite if option is set
if not get_option('satellite_flightrecorder')
  subdir_done()
endif

# Type this satellite identifies as
satellite_type = 'FlightRecorder'

# Source files to be compiled for this satellite
satellite_sources = files(
  'FlightRecorderSatellite.cpp',
)

# Additional dependencies for this satellite
satellite_dependencies = []

# Add [type, sources, dependencies] to build list
satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
