#include "features/settings_controller.hpp"

#include <utility>

SettingsController::SettingsController(HyprlandBackend backend)
    : m_backend(std::move(backend)) {}

SettingsSnapshot SettingsController::load_snapshot() const {
    return m_backend.load_snapshot();
}

bool SettingsController::apply_persistent_option(const std::string& name, const std::string& value) const {
    return m_backend.apply_persistent_option(name, value);
}

bool SettingsController::apply_runtime_option(const std::string& name, const std::string& value) const {
    return m_backend.apply_runtime_option(name, value);
}

bool SettingsController::add_keyword(const std::string& type, const std::string& value) const {
    return m_backend.add_keyword(type, value);
}

bool SettingsController::add_device_config(const std::string& device_name, const std::string& option,
                                           const std::string& value) const {
    return m_backend.add_device_config(device_name, option, value);
}
