#include "platform/hyprland_backend.hpp"

#include <cassert>
#include <string>

int main() {
    {
        std::string escaped = hyprland::escape_keyword_value("a\"b");
        assert(escaped == "a\\\"b");
    }

    {
        std::string cmd = hyprland::build_keyword_command("general:border_size", "2\"4");
        assert(cmd == "hyprctl keyword general:border_size \"2\\\"4\"");
    }

    {
        std::string cmd = hyprland::build_device_keyword_command("my mouse", "sensitivity", "-0.5");
        assert(cmd == "hyprctl keyword device:my mouse:sensitivity \"-0.5\"");
    }

    {
        assert(hyprland::section_path_from_option_name("general:border_size") == "general");
        assert(hyprland::section_path_from_option_name("input:kb_layout") == "input");
        assert(hyprland::section_path_from_option_name("misc:sub:key") == "misc:sub");
        assert(hyprland::section_path_from_option_name("debug") == "");
    }

    return 0;
}
