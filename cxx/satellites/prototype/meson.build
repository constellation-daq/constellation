# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

library('prototype',
  sources: 'prototype.cpp',
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep],
  install_dir: satellite_libdir,
)

executable('satellite_prototypey',
  sources: 'main.cpp',
  dependencies: [exec_dep],
)
