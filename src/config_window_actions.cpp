#include "config_window.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace {
std::string trim_copy(std::string value) {
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(),
                             [](unsigned char ch) { return !std::isspace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [](unsigned char ch) { return !std::isspace(ch); }).base(),
                value.end());
    return value;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::optional<double> parse_number_full(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    char* end = nullptr;
    const double parsed = std::strtod(trimmed.c_str(), &end);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return parsed;
}

bool equivalent_option_values(const std::string& lhs, const std::string& rhs) {
    if (lhs == rhs) {
        return true;
    }

    const std::string l = lower_copy(trim_copy(lhs));
    const std::string r = lower_copy(trim_copy(rhs));
    if (l == r) {
        return true;
    }

    const bool l_true = (l == "true" || l == "1");
    const bool l_false = (l == "false" || l == "0");
    const bool r_true = (r == "true" || r == "1");
    const bool r_false = (r == "false" || r == "0");
    if ((l_true || l_false) && (r_true || r_false)) {
        return (l_true && r_true) || (l_false && r_false);
    }

    const auto l_num = parse_number_full(l);
    const auto r_num = parse_number_full(r);
    if (l_num.has_value() && r_num.has_value()) {
        return std::fabs(*l_num - *r_num) < 1e-9;
    }

    return false;
}
}

void ConfigWindow::send_update(const std::string& name, const std::string& value) {
    auto known = m_OptionValues.find(name);
    if (known != m_OptionValues.end() && equivalent_option_values(known->second, value)) {
        return;
    }

    bool ok = m_SettingsController.apply_persistent_option(name, value);
    if (ok) {
        m_OptionValues[name] = value;
        set_status_message("Applied " + name + " = " + value, false);
    } else {
        set_status_message("Failed to apply " + name, true);
    }
}

void ConfigWindow::send_runtime_update(const std::string& name, const std::string& value) {
    auto known = m_OptionValues.find(name);
    if (known != m_OptionValues.end() && equivalent_option_values(known->second, value)) {
        return;
    }

    bool ok = m_SettingsController.apply_runtime_option(name, value);
    if (ok) {
        m_OptionValues[name] = value;
    } else {
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
