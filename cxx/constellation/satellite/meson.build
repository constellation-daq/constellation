# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

satellite_src = files(
  'BaseSatellite.cpp',
  'CommandRegistry.cpp',
  'FSM.cpp',
  'Satellite.cpp',
)

satellite_lib = library('ConstellationSatellite',
  sources: satellite_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

satellite_dep = declare_dependency(
  link_with: satellite_lib,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep],
)

install_headers(
  'BaseSatellite.hpp',
  'CommandRegistry.hpp',
  'CommandRegistry.ipp',
  'exceptions.hpp',
  'FSM.hpp',
  'Satellite.hpp',
  subdir: 'constellation/satellite',
)
