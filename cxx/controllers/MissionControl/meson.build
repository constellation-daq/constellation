# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if not get_option('controller_missioncontrol')
  subdir_done()
endif

if get_option('build_gui') == 'none'
  warning('Skipping MissionControl since build_gui is set to none')
  subdir_done()
endif

ui_files = qt.compile_ui(sources: 'MissionControl.ui')
moc_files = qt.compile_moc(headers: ['MissionControl.hpp', 'QScanParameter.hpp'])

executable('MissionControl',
  sources: ['MissionControl.cpp', 'QScanParameter.cpp', ui_files, moc_files],
  dependencies: [core_dep, controller_dep, gui_dep, exec_dep, argparse_dep, qt_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Install metadata to find application
install_data(
  'de.desy.constellation.missioncontrol.desktop',
  install_dir: get_option('datadir') / 'applications',
)
install_data(
  meson.project_source_root() / 'docs' / 'logo' / 'logo.svg',
  rename: 'de.desy.constellation.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
