#include <gtkmm.h>

#include "config_window.hpp"

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("org.hyprland.settings");
    return app->make_window_and_run<ConfigWindow>(argc, argv);
}
