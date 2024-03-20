# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

peary_dep = dependency('Peary', required: false, allow_fallback: false, method: 'cmake', modules: ['Caribou::peary'])
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

library('Caribou',
  sources: ['CaribouSatellite.cpp'],
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep, peary_dep],
  install_dir: satellite_libdir,
  install_rpath: constellation_rpath,
)

executable('satellite_caribou',
  sources: 'main.cpp',
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)
