# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

if get_option('build_gui') == 'none'
  subdir_done()
endif

qt_version = get_option('build_gui')
qt = import(qt_version)
qt_dep = dependency(qt_version, modules: ['Core', 'Gui', 'Widgets'])

ui_files = qt.compile_ui(sources: ['QCommandDialog.ui', 'QResponseDialog.ui'])
moc_files = qt.compile_moc(headers: ['QController.hpp', 'QCommandDialog.hpp', 'QResponseDialog.hpp'])
qrc_files = qt.compile_resources(sources: 'Constellation.qrc')

gui_src = files(
  'QController.cpp',
  'QCommandDialog.cpp',
  'QResponseDialog.cpp',
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
  'QCommandDialog.hpp',
  'QResponseDialog.hpp',
  subdir: 'constellation/gui',
)
