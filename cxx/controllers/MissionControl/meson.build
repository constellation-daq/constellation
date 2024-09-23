# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build listener only if feature is enabled:
if not get_option('controller_missioncontrol')
  subdir_done()
endif

qt = import(get_option('qt_version'))
qt_dep = dependency(get_option('qt_version'), modules: ['Core', 'Gui', 'Widgets'])

inc = include_directories('.')

qrc_files = qt.compile_resources(sources: 'MissionControl.qrc')
ui_files = qt.compile_ui(sources: 'MissionControl.ui')
moc_files = qt.compile_moc(headers: ['QController.hpp', 'MissionControl.hpp'])

executable('MissionControl', 'MissionControl.cpp', 'QController.cpp', ui_files, moc_files, qrc_files,
           include_directories: inc,
           dependencies: [exec_dep, argparse_dep, qt_dep, controller_dep],
           install: true,
           install_rpath: constellation_rpath,
           cpp_args: ['-Wno-deprecated-enum-enum-conversion', '-Wno-unused-variable'],
           )
