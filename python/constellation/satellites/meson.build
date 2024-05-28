# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

subdir('caenhv')
subdir('clabdaq2')

satellites_files = files(
  '__init__.py',
  'example_satellite.py',
)

py.install_sources(satellites_files,
  subdir: 'constellation/satellites')
