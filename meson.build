# SPDX-FileCopyrightText: 2023 DESY and the Constellation authors
# SPDX-License-Identifier: CC0-1.0

project('Constellation',
  version: '0.0.0',
  license: 'EUPL-1.2',
  default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++20'],
)

# C++ implementation
impl_cxx = add_languages('cpp', required: get_option('impl_cxx'))
if impl_cxx
  subdir('cxx')
elif get_option('impl_cxx').auto()
  warning('No C++ compiler found, set one with CXX=/path/to/c++-compiler or disable the C++ implementation with impl_cxx=disabled')
endif

# Python implementation
py = import('python').find_installation(required: get_option('impl_py'))
impl_py = py.found()
if impl_py
  subdir('python/constellation')
endif
