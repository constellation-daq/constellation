# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# argparse
argparse_dep = dependency('argparse')

# Asio
asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1'],
)

# ZeroMQ
libzmq_dep = dependency('libzmq',
  default_options: [
    'drafts=true',
    'websockets=disabled',
    'curve=false',
    'openpgm=disabled',
    'norm=disabled',
    'warning_level=0',
])
cppzmq_dep = dependency('cppzmq', default_options: ['examples=false', 'tests=disabled'])

# magic_enum
magic_enum_dep = dependency('magic_enum', default_options: ['test=false'])

# msgpack-cxx
msgpack_cxx_dep = dependency('msgpack-cxx', default_options: ['boost=disabled', 'tests=disabled'])

# spdlog
spdlog_dep = dependency('spdlog', default_options: ['tests=disabled'])


constellation_inc = include_directories('.')

# RPATH for non-default install locations
constellation_rpath = get_option('prefix') / get_option('libdir')

subdir('constellation')
subdir('tools')
subdir('satellites')
subdir('tests')
