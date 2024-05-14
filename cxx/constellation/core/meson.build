# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_src = files(
  'chirp/BroadcastRecv.cpp',
  'chirp/BroadcastSend.cpp',
  'chirp/Manager.cpp',
  'config/Configuration.cpp',
  'config/Dictionary.cpp',
  'config/Value.cpp',
  'config/exceptions.cpp',
  'heartbeat/HeartbeatSend.cpp',
  'heartbeat/HeartbeatManager.cpp',
  'logging/CMDPSink.cpp',
  'logging/Logger.cpp',
  'logging/SinkManager.cpp',
  'message/CDTP1Message.cpp',
  'message/CHIRPMessage.cpp',
  'message/CHP1Message.cpp',
  'message/CMDP1Message.cpp',
  'message/CSCP1Message.cpp',
  'message/BaseHeader.cpp',
)

core_lib = library('ConstellationCore',
  sources: core_src,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep, threads_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
)

install_headers(
  'chirp/BroadcastRecv.hpp',
  'chirp/BroadcastSend.hpp',
  'chirp/CHIRP_definitions.hpp',
  'chirp/Manager.hpp',
  subdir: 'constellation/core/chirp',
)

install_headers(
  'config/Configuration.hpp',
  'config/Configuration.tpp',
  'config/Dictionary.hpp',
  'config/Value.hpp',
  'config/Value.tpp',
  'config/exceptions.hpp',
  subdir: 'constellation/core/config',
)

install_headers(
  'external/md5.h',
  subdir: 'constellation/core/external',
)

install_headers(
  'heartbeat/HeartbeatRecv.hpp',
  'heartbeat/HeartbeatSend.hpp',
  'heartbeat/HeartbeatManager.hpp',
  subdir: 'constellation/core/heartbeat',
)

install_headers(
  'logging/CMDPSink.hpp',
  'logging/Level.hpp',
  'logging/log.hpp',
  'logging/Logger.hpp',
  'logging/ProxySink.hpp',
  'logging/SinkManager.hpp',
  subdir: 'constellation/core/logging',
)

install_headers(
  'message/BaseHeader.hpp',
  'message/CDTP1Message.hpp',
  'message/CHIRPMessage.hpp',
  'message/CHP1Message.hpp',
  'message/CMDP1Message.hpp',
  'message/CSCP1Message.hpp',
  'message/exceptions.hpp',
  'message/payload_buffer.hpp',
  'message/Protocol.hpp',
  'message/satellite_definitions.hpp',
  subdir: 'constellation/core/message',
)

install_headers(
  'subscriber/Subscriber.hpp',
  'subscriber/Subscriber.tpp',
  subdir: 'constellation/core/subscriber',
)

install_headers(
  'utils/casts.hpp',
  'utils/exceptions.hpp',
  'utils/ports.hpp',
  'utils/std_future.hpp',
  'utils/string.hpp',
  'utils/type.hpp',
  subdir: 'constellation/core/utils',
)

core_dep = declare_dependency(
  link_with: core_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [asio_dep, cppzmq_dep, magic_enum_dep, msgpack_cxx_dep, spdlog_dep, threads_dep],
)
