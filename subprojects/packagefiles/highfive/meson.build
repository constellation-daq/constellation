project('HighFive', 'cpp',
  version: '3.0.0-beta1',
  license: 'BSL-1.0',
  default_options: ['cpp_std=c++17'],
)

hdf5_dep = dependency('hdf5')

highfive_inc = include_directories('include')

cmake_cpp_std = get_option('cpp_std').split('++')[1].to_int().to_string()
highfive_args = ['-DHIGHFIVE_CXX_STD='+cmake_cpp_std]

highfive_dep = declare_dependency(
  dependencies: hdf5_dep,
  include_directories: highfive_inc,
  compile_args: highfive_args,
)
