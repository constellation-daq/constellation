# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

qt5 = import('qt6')
qt5_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets'])

inc = include_directories('.')

qrc_files = qt5.compile_resources(sources: 'MissionControl.qrc')
ui_files = qt5.compile_ui(sources: 'MissionControl.ui')
moc_files = qt5.compile_moc(headers: ['QController.hpp', 'MissionControl.hpp'])

executable('euRun', 'MissionControl.cpp', 'QController.cpp', ui_files, moc_files, qrc_files,
           include_directories: inc,
           dependencies: [exec_dep, argparse_dep, qt5_dep, controller_dep],
           install: true,
           install_rpath: constellation_rpath,
           cpp_args: ['-Wno-deprecated-enum-enum-conversion', '-Wno-unused-variable'],
           )
