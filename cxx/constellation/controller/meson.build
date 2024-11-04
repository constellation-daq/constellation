# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

controller_src = files(
  'Controller.cpp',
  'ControllerConfiguration.cpp',
)

controller_lib = library('ConstellationController',
  sources: controller_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, tomlplusplus_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

controller_dep = declare_dependency(
  link_with: controller_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep],
)

pkg.generate(controller_lib,
  description: 'Constellation controller library',
  extra_cflags: constellation_args,
  libraries: [core_dep, cppzmq_dep],
)

install_headers(
  'Controller.hpp',
  'ControllerConfiguration.hpp',
  'exceptions.hpp',
  subdir: 'constellation/controller',
)
