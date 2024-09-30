# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('satellite_katherine')
  subdir_done()
endif

# LibKatherine is needed as dependency
katherine_dep = dependency('katherine', required: true, allow_fallback: meson.version() >= '1.3.0', default_options: ['werror=false', 'warning_level=0'])
katherinexx_dep = dependency('katherinexx', required: true, allow_fallback: meson.version() >= '1.3.0', default_options: ['werror=false', 'warning_level=0'])

satellite_type = 'Katherine'

satellite_sources = files(
  'KatherineSatellite.cpp',
)

satellite_dependencies = [katherine_dep, katherinexx_dep]

satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
