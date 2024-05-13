# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

core_files = files(
  'constellation/core/__init__.py',
  'constellation/core/base.py',
  'constellation/core/broadcastmanager.py',
  'constellation/core/chirp.py',
  'constellation/core/commandmanager.py',
  'constellation/core/chp.py',
  'constellation/core/cmdp.py',
  'constellation/core/cscp.py',
  'constellation/core/cdtp.py',
  'constellation/core/controller.py',
  'constellation/core/configuration.py',
  'constellation/core/datareceiver.py',
  'constellation/core/datasender.py',
  'constellation/core/error.py',
  'constellation/core/fsm.py',
  'constellation/core/heartbeatchecker.py',
  'constellation/core/heartbeater.py',
  'constellation/core/monitoring.py',
  'constellation/core/protocol.py',
  'constellation/core/satellite.py',
)

satellites_files = files(
  'satellites/example_satellite.py',
  'satellites/keithley/keithley_satellite.py',
  'satellites/__init__.py',
)

py.install_sources(core_files,
  subdir: 'constellation/core')

py.install_sources(satellites_files,
  subdir: 'constellation/satellites')
