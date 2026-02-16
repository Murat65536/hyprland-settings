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
           'src/config_window.cpp',
           'src/config_window_views.cpp',
           'src/config_window_actions.cpp',
           'src/config_window_loader.cpp',
           'src/features/settings_controller.cpp',
           'src/features/navigation_feature.cpp',
           'src/platform/hyprland_backend.cpp',
           'src/ui/variables_panel.cpp',
           'src/ui/keywords_panel.cpp',
           'src/ui/devices_panel.cpp',
           'src/ui/option_value_editor.cpp',
           'src/ui/option_name_cell.cpp',
           'src/config_io.cpp',
           'src/config_io.hpp',
           'src/config_window.hpp',
           'src/ui/item_models.hpp',
           'src/core/models.hpp',
           'src/features/navigation_feature.hpp',
           'src/features/settings_controller.hpp',
           'src/platform/hyprland_backend.hpp',
           'src/ui/variables_panel.hpp',
           'src/ui/keywords_panel.hpp',
           'src/ui/devices_panel.hpp',
           'src/ui/option_value_editor.hpp',
           'src/ui/option_name_cell.hpp',
           include_directories : include_directories('src'),
           dependencies : [gtkmm_dep, json_glib_dep],
           install : true)

backend_tests = executable('hyprland-backend-tests',
                           'tests/hyprland_backend_test.cpp',
                           'src/platform/hyprland_backend.cpp',
                           'src/config_io.cpp',
                           'src/platform/hyprland_backend.hpp',
                           'src/core/models.hpp',
                           'src/config_io.hpp',
                           include_directories : include_directories('src'),
                           dependencies : [json_glib_dep])

test('hyprland-backend-tests', backend_tests)
