# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

have_c = add_languages('c', required: get_option('sat_clabdaq2'))
if not have_c
  message('Skipping clabdaq2 satellite since no C compiler was found')
  subdir_done()
endif

shared_library('read_data32bit',
  sources: 'read_data32bit.c',
  install: true,
  install_dir: py.get_install_dir() / 'constellation/satellites/clabdaq2',
)

clabdaq2_files = files(
  'rpgamma.py',
  'rpneutron.py',
  'rpsatellite.py',
)

py.install_sources(clabdaq2_files,
  subdir: 'constellation/satellites/clabdaq2')
