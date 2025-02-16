# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_mattermost_logger')
  subdir_done()
endif

cpr_dep = dependency('cpr')

satellite_type = 'MattermostLogger'

satellite_sources = files(
  'MattermostLoggerSatellite.cpp',
)

satellite_dependencies = [listener_dep, cpr_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
