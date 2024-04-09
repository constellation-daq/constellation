# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

catch2_dep = dependency('catch2-with-main',
  version: '>=3.0.0',
  default_options: ['tests=false'],
  required: get_option('cxx_tests'),
)

if not catch2_dep.found()
  subdir_done()
endif

# Core / CHIRP

test_core_chirp_broadcast = executable('test_core_chirp_broadcast',
  sources: 'test_core_chirp_broadcast.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core CHIRP broadcast test', test_core_chirp_broadcast,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

test_core_chirp_manager = executable('test_core_chirp_manager',
  sources: 'test_core_chirp_manager.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core CHIRP manager test', test_core_chirp_manager,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

test_core_chirp_message = executable('test_core_chirp_message',
  sources: 'test_core_chirp_message.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core CHIRP message test', test_core_chirp_message,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

# Core / Logging

test_core_logging = executable('test_core_logging',
  sources: 'test_core_logging.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core logging test', test_core_logging,
  args: ['--durations', 'yes', '--verbosity', 'high'],
  is_parallel: false,
)

# Core / Message

test_core_message = executable('test_core_message',
  sources: 'test_core_message.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core message test', test_core_message,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

# Core / Configuration

test_core_config = executable('test_core_config',
  sources: 'test_core_config.cpp',
  dependencies: [core_dep, catch2_dep],
)
test('Core configuration test', test_core_config,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

# Satellite

test_satellite_fsm = executable('test_satellite_fsm',
  sources: 'test_satellite_fsm.cpp',
  dependencies: [satellite_dep, catch2_dep],
)
test('Satellite FSM test', test_satellite_fsm,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)

test_satellite_implementation = executable('test_satellite_implementation',
  sources: 'test_satellite_implementation.cpp',
  dependencies: [satellite_dep, catch2_dep],
)
test('Satellite implementation test', test_satellite_implementation,
  args: ['--durations', 'yes', '--verbosity', 'high'],
)
