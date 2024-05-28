if not get_option('sat_clabdaq2').enabled()
  subdir_done()
endif

add_languages('c')

shared_library('read_data32bit',
  sources: 'read_data32bit.c',
  install: true,
  install_dir: py.get_path('platlibdir'),
)
