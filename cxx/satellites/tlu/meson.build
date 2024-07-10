# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Build satellite if feature is not disabled:
if get_option('satellite_aidatlu').disabled()
  subdir_done()
endif

cactus_root = get_option('cactus_root')
cpp = meson.get_compiler('cpp')

uhal_uhal_dep = cpp.find_library(
   'cactus_uhal_uhal',
   dirs: cactus_root / 'lib',
   has_headers: ['uhal/uhal.hpp'],
   header_include_directories: include_directories(cactus_root / 'include'),
   required: false,
)
uhal_grammars_dep = cpp.find_library(
   'cactus_uhal_grammars',
   dirs: cactus_root / 'lib',
   required: false,
)
uhal_log_dep = cpp.find_library(
   'cactus_uhal_log',
   dirs: cactus_root / 'lib',
   required: false,
)
if not uhal_uhal_dep.found()
  error('Need uhal')
endif
if not uhal_grammars_dep.found()
  error('Need uhal')
endif
if not uhal_log_dep.found()
  error('Need uhal')
endif


# Type this satellite identifies as:
satellite_type = 'AidaTLU'

# Source files to be compiled for this satellite:
satellite_sources = files(
  'TluSatellite.cpp',
  'aida_src/AidaTluDisplay.cc',
  'aida_src/AidaTluController.cc',
  'aida_src/AidaTluPowerModule.cc',
  'aida_src/AidaTluI2c.cc',
  'aida_src/AidaTluHardware.cc'
)

# Automatic generation of library interface and satellite executable:
satellite_cfg_data = configuration_data()
satellite_cfg_data.set('SATELLITE_TYPE', satellite_type)
satellite_generator = configure_file(
  input: satellite_generator_template,
  output: 'generator.cpp',
  configuration: satellite_cfg_data,
)
satellite_main = configure_file(
  input: satellite_main_template,
  output: 'main.cpp',
  configuration: satellite_cfg_data,
)

# Explicitly store sputnik lib variable for core/exec test:
tlu_lib = shared_library(satellite_type,
  include_directories: ['aida_include',cactus_root / 'include'],
  sources: [satellite_generator, satellite_sources],
  dependencies: [core_dep, satellite_dep, uhal_log_dep, uhal_uhal_dep, uhal_grammars_dep],
  gnu_symbol_visibility: 'hidden',
  install_dir: satellite_libdir,
  install_rpath: constellation_rpath,
  cpp_args: '-Wno-error=deprecated-declarations',
)

executable('satellite' + satellite_type,
  sources: [satellite_main],
  dependencies: [exec_dep],
  install: true,
  install_rpath: constellation_rpath,
)

# Add satellite to build summary:
satellites_to_build += satellite_type
