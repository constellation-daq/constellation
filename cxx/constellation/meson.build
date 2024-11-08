# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Shared library prefix: UNIX: "lib", Windows: ""
# Shared library suffixes: POSIX: ".so", MacOS: ".dylib", Windows: ".dll"
dso_prefix = 'lib'
dso_suffix = '.so'
if host_machine.system() == 'darwin'
  dso_suffix = '.dylib'
elif host_machine.system() == 'windows'
  dso_prefix = ''
  dso_suffix = '.dll'
endif

# pkgconfig module to export .pc files for libraries
pkg = import('pkgconfig')

build_hpp_data = configuration_data()
build_hpp_data.set('version', meson.project_version())
build_hpp_data.set('version_code_name', version_code_name)
build_hpp_data.set('libdir', get_option('prefix') / get_option('libdir'))
build_hpp_data.set('builddir', meson.project_build_root())
build_hpp_data.set('dso_prefix', dso_prefix)
build_hpp_data.set('dso_suffix', dso_suffix)
build_hpp = configure_file(
  input: 'build.hpp',
  output: 'build.hpp',
  configuration: build_hpp_data,
)

install_headers(build_hpp, subdir: 'constellation')

subdir('core')
subdir('satellite')
subdir('controller')
subdir('exec')
