#ifndef SETTINGS_CONTROLLER_HPP
#define SETTINGS_CONTROLLER_HPP

#include "core/models.hpp"
#include "platform/hyprland_backend.hpp"

#include <string>

class SettingsController {
public:
    explicit SettingsController(HyprlandBackend backend = HyprlandBackend());

    SettingsSnapshot load_snapshot() const;
    bool apply_persistent_option(const std::string& name, const std::string& value) const;
    bool apply_runtime_option(const std::string& name, const std::string& value) const;
    bool add_keyword(const std::string& type, const std::string& value) const;
    bool add_device_config(const std::string& device_name, const std::string& option,
                           const std::string& value) const;

private:
    HyprlandBackend m_backend;
};

#endif
