# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build listener only if feature is enabled:
if not get_option('listener_observatory')
  subdir_done()
endif

if get_option('build_gui') == 'none'
  warning('Skipping Observatory since build_gui is set to none')
  subdir_done()
endif

qrc_files = qt.compile_resources(sources: 'Observatory.qrc')
ui_files = qt.compile_ui(sources: ['Observatory.ui', 'QSenderSubscriptions.ui'])
moc_files = qt.compile_moc(headers: ['QLogFilter.hpp', 'QLogListener.hpp', 'QSenderSubscriptions.hpp', 'Observatory.hpp'])

executable('Observatory',
  sources: ['Observatory.cpp', 'QLogFilter.cpp', 'QLogListener.cpp', 'QSenderSubscriptions.cpp', qrc_files, ui_files, moc_files],
  dependencies: [core_dep, gui_dep, listener_dep, exec_dep, argparse_dep, qt_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Install metadata to find application
install_data(
  'de.desy.constellation.observatory.desktop',
  install_dir: get_option('datadir') / 'applications',
)
install_data(
  meson.project_source_root() / 'docs' / 'logo' / 'logo_observatory.svg',
  rename: 'de.desy.constellation.observatory.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
