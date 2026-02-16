#ifndef HYPRLAND_BACKEND_HPP
#define HYPRLAND_BACKEND_HPP

#include "core/models.hpp"

#include <string>

namespace hyprland {
std::string escape_keyword_value(const std::string& value);
std::string build_keyword_command(const std::string& name, const std::string& value);
std::string build_device_keyword_command(const std::string& device_name, const std::string& option,
                                         const std::string& value);
std::string section_path_from_option_name(const std::string& option_name);
}

class HyprlandBackend {
public:
    bool apply_persistent_option(const std::string& name, const std::string& value) const;
    bool apply_runtime_option(const std::string& name, const std::string& value) const;
    bool add_keyword(const std::string& type, const std::string& value) const;
    bool add_device_config(const std::string& device_name, const std::string& option,
                           const std::string& value) const;

    std::vector<std::string> get_available_devices() const;
    SettingsSnapshot load_snapshot() const;
};

#endif
