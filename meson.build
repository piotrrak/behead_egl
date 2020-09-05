project('libbehead-egl', ['cpp'], version: '0.0.1', default_options: ['cpp_std=c++17'])

cxx = meson.get_compiler('cpp')

check_flags = ['-fvisibility=hidden']

foreach f : check_flags
   if cxx.has_argument(f)
      add_project_arguments(f, language: 'cpp')
   endif
endforeach

egl_dep = dependency('egl')

libbhd_egl_inc =  include_directories('include')

install_headers('include/bhd/behead_egl.hh', subdir: 'bhd')

subdir('src')
subdir('example')

pkg = import('pkgconfig')

pkg.generate(libbehead_egl)

run_target('q', command: behead_example)
