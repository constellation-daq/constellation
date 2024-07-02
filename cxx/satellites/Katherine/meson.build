# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build satellite if feature is not disabled:
if get_option('satellite_katherine').disabled()
  subdir_done()
endif

# LibKatherine is needed as dependency
katherine_dep = dependency('katherine', required: true, allow_fallback: true, default_options: ['werror=false'])

# Type this satellite identifies as:
satellite_type = 'Katherine'

# Source files to be compiled for this satellite:
satellite_sources = files(
  'KatherineSatellite.cpp',
)

# Automatic generation of library interface and satellite executable:
satellite_cfg_data = configuration_data()
satellite_cfg_data.set('SATELLITE_TYPE', satellite_type)
satellite_generator = configure_file(
  input: satellite_generator_template,
  output: 'generator.cpp',
  configuration: satellite_cfg_data,
)
satellite_main = configure_file(
  input: satellite_main_template,
  output: 'main.cpp',
  configuration: satellite_cfg_data,
)

shared_library(satellite_type,
  sources: [satellite_generator, satellite_sources],
  dependencies: [core_dep, satellite_dep],
  gnu_symbol_visibility: 'hidden',
  install_dir: satellite_libdir,
  install_rpath: constellation_rpath,
)

executable('satellite' + satellite_type,
  sources: [satellite_main],
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Add satellite to build summary:
satellites_to_build += satellite_type
