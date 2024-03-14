# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

satellite_src = files(
  'FSM.cpp',
  'Satellite.cpp',
  'SatelliteImplementation.cpp',
)

satellite_lib = library('ConstellationSatellite',
  sources: satellite_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
)

# We do not provide a C ABI, so we are fine with exporting C++ symbols via extern C
satellite_compiler_args = []
if meson.get_compiler('cpp').has_argument('-Wno-return-type-c-linkage')
  satellite_compiler_args += '-Wno-return-type-c-linkage'
endif

satellite_dep = declare_dependency(
  link_with: satellite_lib,
  compile_args: satellite_compiler_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep],
)

install_headers(
  'exceptions.hpp',
  'FSM.hpp',
  'fsm_definitions.hpp',
  'Satellite.hpp',
  'SatelliteImplementation.hpp',
  subdir: 'constellation/satellite',
)
