# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

controller_src = files(
  'Controller.cpp',
)

controller_lib = library('ConstellationController',
  sources: controller_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

# We do not provide a C ABI, so we are fine with exporting C++ symbols via extern C
controller_compiler_args = []
if meson.get_compiler('cpp').has_argument('-Wno-return-type-c-linkage')
  controller_compiler_args += '-Wno-return-type-c-linkage'
endif

controller_dep = declare_dependency(
  link_with: controller_lib,
  compile_args: controller_compiler_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, cppzmq_dep, magic_enum_dep],
)

install_headers(
  'Controller.hpp',
  subdir: 'constellation/controller',
)
