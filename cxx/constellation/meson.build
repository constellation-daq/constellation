# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# argparse
argparse_dep = dependency('argparse')

# Asio
asio_raw_dep = dependency('asio')
asio_dep = declare_dependency(
  dependencies: asio_raw_dep,
  compile_args: ['-DASIO_STANDALONE=1'],
)

# Catch2
catch2_dep = dependency('catch2-with-main',
  version: '>=3.0.0',
  default_options: ['tests=false'])

# ZeroMQ
libzmq_dep = dependency('libzmq',
  default_options: [
    'drafts=true',
    'websockets=disabled',
    'curve=false',
    'tls=disabled',
    'nss=disabled',
    'openpgm=disabled',
    'norm=disabled',
    'warning_level=0',
])
cppzmq_dep = dependency('cppzmq', default_options: ['catch2=disabled'])

# magic_enum
magic_enum_dep = dependency('magic_enum', default_options: ['test=false'])

# msgpack-cxx
msgpack_cxx_dep = dependency('msgpack-cxx', default_options: ['boost=disabled', 'tests=disabled'])

# spdlog
spdlog_raw_dep = dependency('spdlog', default_options: ['tests=disabled'])
spdlog_dep = declare_dependency(
  dependencies: spdlog_raw_dep,
  compile_args: [
    '-DSPDLOG_LEVEL_NAMES={"TRACE","DEBUG","INFO","WARNING","STATUS","CRITICAL","OFF"}',
    '-DSPDLOG_SHORT_LEVEL_NAMES={"T","D","I","S","W","C","O"}',
    '-DSPDLOG_DISABLE_DEFAULT_LOGGER',
  ],
)

constellation_inc = include_directories('..')

subdir('core')
subdir('satellite')
subdir('exec')
subdir('tools')
