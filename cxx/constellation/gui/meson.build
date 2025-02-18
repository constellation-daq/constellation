# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if get_option('build_gui') == 'none'
  subdir_done()
endif

qt_version = get_option('build_gui')
qt = import(qt_version)
qt_dep = dependency(qt_version, modules: ['Core', 'Gui', 'Widgets'])

ui_files = qt.compile_ui(sources: ['QConnectionDialog.ui', 'QCommandDialog.ui', 'QResponseDialog.ui', 'QLogMessageDialog.ui'])
moc_files = qt.compile_moc(headers: ['QController.hpp', 'QConnectionDialog.hpp', 'QCommandDialog.hpp', 'QResponseDialog.hpp', 'QLogMessageDialog.hpp'])
qrc_files = qt.compile_resources(sources: 'Constellation.qrc')

gui_src = files(
  'QController.cpp',
  'QConnectionDialog.cpp',
  'QCommandDialog.cpp',
  'QResponseDialog.cpp',
  'QLogMessage.cpp',
  'QLogMessageDialog.cpp'
)

gui_lib = library('ConstellationGUI',
  sources: [gui_src, ui_files, moc_files, qrc_files],
  include_directories: constellation_inc,
  dependencies: [core_dep, controller_dep, qt_dep],
  gnu_symbol_visibility: 'hidden',
  cpp_args: constellation_args + ['-DCNSTLN_BUILDLIB=1'],
  install: true,
  install_rpath: constellation_rpath,
)

gui_dep = declare_dependency(
  link_with: gui_lib,
  compile_args: constellation_args,
  include_directories: constellation_inc,
  dependencies: [core_dep, controller_dep, qt_dep],
)

pkg.generate(gui_lib,
  description: 'Constellation GUI library',
  extra_cflags: constellation_args,
  libraries: [core_dep, controller_dep, qt_dep],
)

install_headers(
  'qt_utils.hpp',
  'QController.hpp',
  'QConnectionDialog.hpp',
  'QCommandDialog.hpp',
  'QResponseDialog.hpp',
  'QLogMessageDialog.hpp',
  'QLogMessage.hpp',
  subdir: 'constellation/gui',
)
