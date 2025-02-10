# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

listener_src = files(
  'CMDPListener.cpp',
  'LogListener.cpp',
  'Notifications.cpp',
)

listener_lib = library('ConstellationListener',
  sources: listener_src,
  include_directories: constellation_inc,
  dependencies: [core_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

listener_dep = declare_dependency(
  link_with: listener_lib,
  include_directories: constellation_inc,
  dependencies: [core_dep],
)

install_headers(
  'CMDPListener.hpp',
  'LogListener.hpp',
  'Notifications.hpp',
  subdir: 'constellation/listener',
)
