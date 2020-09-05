srcs = ['behead_egl.cc', 'minidrm.cc', 'ufd.cc']

libbehead_egl = both_libraries(
   'behead-egl', srcs,
    include_directories: libbhd_egl_inc,
    dependencies: egl_dep,
    install: true)

libbehead_egl_dep = declare_dependency(
   include_directories: libbhd_egl_inc,
   link_with: libbehead_egl,
   dependencies: [egl_dep])

libbehead_egl_static_dep = declare_dependency(
   include_directories: libbhd_egl_inc,
   link_with: libbehead_egl.get_static_lib(),
   dependencies: [egl_dep])


