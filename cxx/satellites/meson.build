# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

# Installation directory for satellites - private libdir since they are more like plugins
satellite_libdir = get_option('libdir') / 'ConstellationSatellites'

# List of satellites for build summary
satellites_to_build = []

# Subdirs with satellites
subdir('Sputnik')

# Print build summary for satellites
summary('Building', satellites_to_build, section: 'Satellites')
