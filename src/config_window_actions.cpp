#include "config_window.hpp"

void ConfigWindow::send_update(const std::string& name, const std::string& value) {
    bool ok = m_SettingsController.apply_persistent_option(name, value);
    if (ok) {
        set_status_message("Applied " + name + " = " + value, false);
    } else {
        set_status_message("Failed to apply " + name, true);
    }
}

void ConfigWindow::send_runtime_update(const std::string& name, const std::string& value) {
    bool ok = m_SettingsController.apply_runtime_option(name, value);
    if (!ok) {
        set_status_message("Failed runtime update for " + name, true);
    }
}

void ConfigWindow::send_keyword_add(const std::string& type, const std::string& value) {
    bool ok = m_SettingsController.add_keyword(type, value);
    if (ok) {
        set_status_message("Added keyword " + type, false);
    } else {
        set_status_message("Failed to add keyword " + type, true);
    }
}

void ConfigWindow::send_device_config_add(const std::string& deviceName, const std::string& option,
                                          const std::string& value) {
    bool ok = m_SettingsController.add_device_config(deviceName, option, value);
    if (ok) {
        set_status_message("Applied device config " + deviceName + ":" + option, false);
    } else {
        set_status_message("Failed device config " + deviceName + ":" + option, true);
    }
}
