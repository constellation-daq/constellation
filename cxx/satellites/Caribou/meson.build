# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Do not build if not explicitly enabled
if not get_option('satellite_caribou').enabled()
  subdir_done()
endif

peary_dep = dependency('peary', required: false, allow_fallback: false)
if not peary_dep.found()
  cmake = import('cmake')
  peary_opts = cmake.subproject_options()

  # Set RPATH manually to ensure we can load Peary modules without installation
  peary_build_dir = meson.global_build_root() / 'subprojects' / 'peary'
  peary_opts.append_link_args('-Wl,-rpath='+peary_build_dir+',--enable-new-dtags', target: 'peary')

  # Install all peary targets
  peary_opts.set_install(true)

  # Turn off all warnings
  peary_opts.set_override_option('warning_level', '0')
  peary_opts.set_override_option('werror', 'false')

  # Peary build options
  peary_opts.add_cmake_defines({'BUILD_EXE': false})

  peary_sp = cmake.subproject('peary', options: peary_opts)
  peary_dep = peary_sp.dependency('peary')
endif

satellite_type = 'Caribou'

satellite_sources = files(
  'CaribouSatellite.cpp',
)

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
  dependencies: [core_dep, satellite_dep, peary_dep],
  gnu_symbol_visibility: 'hidden',
  install: true,
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
