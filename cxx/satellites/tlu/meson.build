# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

library('tlu',
  sources: ['TluSatellite.cpp'],
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep],
  install_dir: satellite_libdir,
  install_rpath: constellation_rpath,
)

executable('satellite_tlu',
  sources: 'main.cpp',
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)
