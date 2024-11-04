# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# List containing satellites as [type, sources, dependencies]
satellites_to_build = []

# Subdirs with satellites
subdir('DevNullReceiver')
subdir('RandomTransmitter')
subdir('Sputnik')

# Installation directory for satellites - private libdir since they are more like plugins
satellite_libdir = get_option('libdir') / 'ConstellationSatellites'

# Iterate over each satellite
foreach satellite : satellites_to_build
  # Extract variables from list
  satellite_type = satellite[0]
  satellite_sources = satellite[1]
  satellite_dependencies = satellite[2]
  # Automatic generation of library interface and satellite executable
  satellite_cfg_data = configuration_data()
  satellite_cfg_data.set('SATELLITE_TYPE', satellite_type)
  satellite_generator = configure_file(
    input: satellite_generator_template,
    output: 'generator' + satellite_type + '.cpp',
    configuration: satellite_cfg_data,
  )
  satellite_main = configure_file(
    input: satellite_main_template,
    output: 'main' + satellite_type + '.cpp',
    configuration: satellite_cfg_data,
  )
  # Create satellite library
  satellite_lib = shared_library(
    satellite_type,
    sources: [satellite_generator, satellite_sources],
    dependencies: [core_dep, satellite_dep] + satellite_dependencies,
    gnu_symbol_visibility: 'hidden',
    install: true,
    install_dir: satellite_libdir,
    install_rpath: constellation_rpath,
  )
  set_variable('satellite_'+satellite_type.to_lower()+'_lib', satellite_lib)
  # Create satellite executable
  executable('satellite' + satellite_type,
    sources: [satellite_main],
    dependencies: [exec_dep],
    install: true,
    install_rpath: constellation_rpath,
  )
endforeach
