#SPDX - FileCopyrightText : 2024 DESY and the Constellation authors
#SPDX - License - Identifier : CC0-1.0

#Build satellite if feature is not disabled:
if not get_option('satellite_aidatlu')
  subdir_done()
endif

cactus_root = get_option('cactus_root')

cpp = meson.get_compiler('cpp')

uhal_uhal_dep = cpp.find_library('cactus_uhal_uhal',
                                 dirs : cactus_root / 'lib',
                                 has_headers : ['uhal/uhal.hpp'],
                                 header_include_directories : include_directories(cactus_root / 'include'),
                                 required : false, )
uhal_grammars_dep = cpp.find_library('cactus_uhal_grammars',
                                     dirs : cactus_root / 'lib',
                                     required : false, )

uhal_log_dep = cpp.find_library('cactus_uhal_log',
                                  dirs : cactus_root / 'lib',
                                  required : false, )

if not uhal_uhal_dep.found()
  error('Need uhal')
endif

if not uhal_grammars_dep.found()
  error('Need uhal')
endif

if not uhal_log_dep.found()
  error('Need uhal')
endif

# Type this satellite identifies as
satellite_type = 'AidaTLU'

# Source files to be compiled for this satellite
satellite_sources = files(
  'AidaTLUSatellite.cpp',
  'aida_src/AidaTluDisplay.cc',
  'aida_src/AidaTluController.cc',
  'aida_src/AidaTluPowerModule.cc',
  'aida_src/AidaTluI2c.cc',
  'aida_src/AidaTluHardware.cc',
)

# Additional dependencies for this satellite
satellite_dependencies = [uhal_log_dep, uhal_uhal_dep, uhal_grammars_dep]

# Add [type, sources, dependencies] to build list
satellites_to_build += [[satellite_type, satellite_sources, satellite_dependencies]]
