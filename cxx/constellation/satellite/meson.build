# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

satellite_src = files(
  'BaseSatellite.cpp',
  'CommandRegistry.cpp',
  'FSM.cpp',
  'ReceiverSatellite.cpp',
  'Satellite.cpp',
  'TransmitterSatellite.cpp',
)

satellite_lib = library(
  'ConstellationSatellite',
  sources: satellite_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

satellite_dep = declare_dependency(
  link_with: satellite_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep],
)

pkg.generate(satellite_lib,
  description: 'Constellation satellite library',
  extra_cflags: constellation_args,
  libraries: [core_dep, cppzmq_dep, magic_enum_dep],
)

install_headers(
  'BaseSatellite.hpp',
  'CommandRegistry.hpp',
  'CommandRegistry.ipp',
  'exceptions.hpp',
  'FSM.hpp',
  'ReceiverSatellite.hpp',
  'Satellite.hpp',
  'TransmitterSatellite.hpp',
  subdir: 'constellation/satellite',
)
