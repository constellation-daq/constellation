# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_files = files(
  'core/__init__.py',
  'core/base.py',
  'core/broadcastmanager.py',
  'core/chirp.py',
  'core/commandmanager.py',
  'core/chp.py',
  'core/cmdp.py',
  'core/cscp.py',
  'core/cdtp.py',
  'core/controller.py',
  'core/confighandler.py',
  'core/datareceiver.py',
  'core/datasender.py',
  'core/error.py',
  'core/fsm.py',
  'core/heartbeatchecker.py',
  'core/heartbeater.py',
  'core/monitoring.py',
  'core/protocol.py',
  'core/satellite.py',
  'core/confighandler.py',
)

satellites_files = files(
  'satellites/example_satellite.py',
  'satellites/__init__.py',
)

py.install_sources(core_files,
  subdir: 'constellation/core')

py.install_sources(satellites_files,
  subdir: 'constellation/satellites')
