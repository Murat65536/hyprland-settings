project('hyprland-settings', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++17'])

gtkmm_dep = dependency('gtkmm-4.0')
json_glib_dep = dependency('json-glib-1.0')

configure_file(input : 'src/style.css',
               output : 'style.css',
               copy : true)

executable('hyprland-settings-gui',
           'src/main.cpp',
           'src/config_io.cpp',
           'src/config_io.hpp',
           dependencies : [gtkmm_dep, json_glib_dep],
           install : true)
