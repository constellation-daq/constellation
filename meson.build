# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

project('Constellation',
  version: '0.1',
  license: 'EUPL-1.2',
  default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++20'],
)

# Version code name are taken from the IAU constellations sorted by solid angle
# https://en.wikipedia.org/wiki/IAU_designated_constellations_by_area
version_code_name = 'Crux'

# C++ implementation
impl_cxx = add_languages('cpp', native: false, required: get_option('impl_cxx'))
if impl_cxx
  message('Enabled C++ implementation')
  subdir('cxx')
endif

# Python implementation
py = import('python').find_installation(required: get_option('impl_py'))
impl_py = py.found()
if impl_py
  message('Enabled Python implementation')
  subdir('python/constellation')
endif
