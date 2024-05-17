# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

cactus_root = get_option('cactus_root')
cpp = meson.get_compiler('cpp')

uhal_dep = cpp.find_library(
   'cactus_uhal_grammars',
   dirs: cactus_root / 'lib',
   has_headers: ['uhal/uhal.hpp'],
   header_include_directories: include_directories(cactus_root / 'include'),
   required:false,
)
if not uhal_dep.found()
  error('Need uhal')
endif

aida_src = files(
  'aida_src/AidaTluDisplay.cc',
  'aida_src/AidaTluController.cc',
  'aida_src/AidaTluPowerModule.cc',
  'aida_src/AidaTluI2c.cc',
  'aida_src/AidaTluHardware.cc'
)

library('tlu',
  sources: ['TluSatellite.cpp',aida_src],
  include_directories: ['aida_include',cactus_root / 'include'],
  dependencies: [core_dep, satellite_dep, uhal_dep],
  install_dir: satellite_libdir,
  install_rpath: constellation_rpath,
  cpp_args: '-Wno-error=deprecated-declarations',
)

executable('satellite_tlu',
  sources: 'main.cpp',
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)
