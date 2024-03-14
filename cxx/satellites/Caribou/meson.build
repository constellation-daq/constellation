# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

peary_dep = dependency('peary', required: false, allow_fallback: false)
if not peary_dep.found()
  cmake = import('cmake')
  peary_opts = cmake.subproject_options()
  peary_opts.add_cmake_defines({'BUILD_EXE': false})
  # add build options here

  peary_sp = cmake.subproject('peary', options: peary_opts)
  peary_dep = peary_sp.dependency('peary')
endif

library('Caribou',
  sources: ['CaribouSatellite.cpp'],
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep, peary_dep],
  install_dir: satellite_libdir,
)
