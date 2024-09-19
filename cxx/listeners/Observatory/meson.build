# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build listener only if feature is enabled:
if not get_option('listener_observatory').enabled()
  subdir_done()
endif

qt = import(get_option('qt_version'))
qt_dep = dependency(get_option('qt_version'), modules: ['Core', 'Gui', 'Widgets'])

inc = include_directories('.')

qrc_files = qt.compile_resources(sources: 'Observatory.qrc')
ui_files = qt.compile_ui(sources: ['Observatory.ui', 'LogDialog.ui'])
moc_files = qt.compile_moc(headers: ['QLogListener.hpp', 'Observatory.hpp'])

executable('Observatory', 'Observatory.cpp', 'QLogListener.cpp', ui_files, moc_files, qrc_files,
           include_directories: inc,
           dependencies: [exec_dep, argparse_dep, qt_dep, core_dep, magic_enum_dep],
           install: true,
           install_rpath: constellation_rpath,
           cpp_args: ['-Wno-deprecated-enum-enum-conversion', '-Wno-unused-variable'],
           )
