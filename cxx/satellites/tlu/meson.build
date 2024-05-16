# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

aida_src = files(
  'aida_src/AidaTluDisplay.cc',
  'aida_src/AidaTluController.cc',
  'aida_src/AidaTluPowerModule.cc',
  'aida_src/AidaTluI2c.cc',
  'aida_src/AidaTluHardware.cc'
)

library('tlu',
  sources: ['TluSatellite.cpp',aida_src],
  include_directories: [constellation_inc],
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
