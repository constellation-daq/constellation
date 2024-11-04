# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

exec_src = files(
  'DSOLoader.cpp',
  'satellite.cpp',
)

exec_lib = library('ConstellationExec',
  sources: exec_src,
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep, argparse_dep, asio_dep, magic_enum_dep, dl_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

exec_dep = declare_dependency(
  link_with: exec_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, satellite_dep],
)

pkg.generate(exec_lib,
  description: 'Constellation exec library',
  extra_cflags: constellation_args,
  libraries: [core_dep, satellite_dep],
)

install_headers(
  'exceptions.hpp',
  'DSOLoader.hpp',
  'satellite.hpp',
  subdir: 'constellation/exec',
)

executable('satellite',
  sources: 'main.cpp',
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Template files for in-repo satellite implementations
satellite_generator_template = files('templates/generator.cpp.in')
satellite_main_template = files('templates/main.cpp.in')

install_data(
  satellite_generator_template,
  satellite_main_template,
  install_dir: get_option('includedir') / 'constellation/exec/templates',
)
