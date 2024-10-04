# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('cxx_tools')
  subdir_done()
endif

# Core / CHIRP

executable('chirp_manager',
  sources: 'chirp_manager.cpp',
  dependencies: [core_dep, magic_enum_dep],
)

# Core/ Heartbeat

executable('chp_sender',
  sources: 'chp_sender.cpp',
  dependencies: [core_dep, magic_enum_dep],
)

executable('chp_receiver',
  sources: 'chp_receiver.cpp',
  dependencies: [core_dep],
)

# Satellite

executable('dummy_controller',
  sources: 'dummy_controller.cpp',
  dependencies: [core_dep, controller_dep, msgpack_cxx_dep],
)
