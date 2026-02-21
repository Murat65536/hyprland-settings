#include "platform/hyprland_backend.hpp"

#include "config_io.hpp"

#include <cstdlib>
#include <iostream>
#include <json-glib/json-glib.h>
#include <optional>
#include <regex>

namespace {
bool run_capture(const std::string& cmd, std::string& output) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return false;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

    return pclose(pipe) == 0;
}

bool run_system(const std::string& cmd) {
    int ret = std::system(cmd.c_str());
    return ret == 0;
}

std::string json_node_to_string(JsonObject* data_obj, const char* member) {
    if (!json_object_has_member(data_obj, member)) {
        return "";
    }

    JsonNode* node = json_object_get_member(data_obj, member);
    if (!node || !JSON_NODE_HOLDS_VALUE(node)) {
        return "";
    }

    GType type = json_node_get_value_type(node);
    if (type == G_TYPE_STRING) {
        return json_object_get_string_member(data_obj, member);
    }
    if (type == G_TYPE_INT64) {
        return std::to_string(json_object_get_int_member(data_obj, member));
    }
    if (type == G_TYPE_DOUBLE) {
        return std::to_string(json_object_get_double_member(data_obj, member));
    }
    if (type == G_TYPE_BOOLEAN) {
        return json_object_get_boolean_member(data_obj, member) ? "true" : "false";
    }
    return "";
}

std::optional<double> json_node_to_double(JsonObject* data_obj, const char* member) {
    if (!json_object_has_member(data_obj, member)) {
        return std::nullopt;
    }

    JsonNode* node = json_object_get_member(data_obj, member);
    if (!node || !JSON_NODE_HOLDS_VALUE(node)) {
        return std::nullopt;
    }

    GType type = json_node_get_value_type(node);
    if (type == G_TYPE_DOUBLE) {
        return json_object_get_double_member(data_obj, member);
    }
    if (type == G_TYPE_INT64) {
        return static_cast<double>(json_object_get_int_member(data_obj, member));
    }
    if (type == G_TYPE_STRING) {
        try {
            return std::stod(json_object_get_string_member(data_obj, member));
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::string json_string_member_if_string(JsonObject* data_obj, const char* member) {
    if (!json_object_has_member(data_obj, member)) {
        return "";
    }

    JsonNode* node = json_object_get_member(data_obj, member);
    if (!node || !JSON_NODE_HOLDS_VALUE(node)) {
        return "";
    }

    if (json_node_get_value_type(node) != G_TYPE_STRING) {
        return "";
    }

    return json_object_get_string_member(data_obj, member);
}
}

std::string hyprland::escape_keyword_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char c : value) {
        if (c == '"') {
            escaped += "\\\"";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

std::string hyprland::build_keyword_command(const std::string& name, const std::string& value) {
    return "hyprctl keyword " + name + " \"" + escape_keyword_value(value) + "\"";
}

std::string hyprland::build_device_keyword_command(const std::string& device_name,
                                                   const std::string& option,
                                                   const std::string& value) {
    return build_keyword_command("device:" + device_name + ":" + option, value);
}

std::string hyprland::section_path_from_option_name(const std::string& option_name) {
    size_t pos = option_name.rfind(':');
    if (pos == std::string::npos) {
        return "";
    }
    return option_name.substr(0, pos);
}

bool HyprlandBackend::apply_persistent_option(const std::string& name, const std::string& value) const {
    const char* home = std::getenv("HOME");
    std::string config_path = home ? std::string(home) + "/.config/hypr/hyprland.conf" : "hyprland.conf";

    if (!ConfigIO::updateOption(config_path, name, value)) {
        std::cerr << "Failed to update config file for: " << name << std::endl;
        return false;
    }

    return run_system(hyprland::build_keyword_command(name, value));
}

bool HyprlandBackend::apply_runtime_option(const std::string& name, const std::string& value) const {
    return run_system(hyprland::build_keyword_command(name, value));
}

bool HyprlandBackend::add_keyword(const std::string& type, const std::string& value) const {
    return run_system(hyprland::build_keyword_command(type, value));
}

bool HyprlandBackend::add_device_config(const std::string& device_name, const std::string& option,
                                        const std::string& value) const {
    return run_system(hyprland::build_device_keyword_command(device_name, option, value));
}

std::vector<std::string> HyprlandBackend::get_available_devices() const {
    std::vector<std::string> devices;
    std::string output;
    if (!run_capture("hyprctl -j devices", output)) {
        return devices;
    }

    std::regex name_regex("\\\"name\\\":\\s*\\\"([^\\\"]+)\\\"");
    std::smatch match;
    std::string::const_iterator search_start(output.cbegin());
    while (std::regex_search(search_start, output.cend(), match, name_regex)) {
        devices.push_back(match[1]);
        search_start = match.suffix().first;
    }

    return devices;
}

SettingsSnapshot HyprlandBackend::load_snapshot() const {
    SettingsSnapshot snapshot;
    snapshot.available_devices = get_available_devices();

    std::string json_output;
    if (!run_capture("hyprctl descriptions -j", json_output)) {
        return snapshot;
    }

    GError* error = nullptr;
    JsonParser* parser = json_parser_new();
    bool parsed = json_parser_load_from_data(parser, json_output.c_str(), -1, &error);
    if (!parsed) {
        if (error) {
            g_error_free(error);
        }
        g_object_unref(parser);
        return snapshot;
    }

    JsonNode* root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_ARRAY(root)) {
        g_object_unref(parser);
        return snapshot;
    }

    JsonArray* array = json_node_get_array(root);
    guint length = json_array_get_length(array);

    for (guint i = 0; i < length; ++i) {
        JsonObject* obj = json_array_get_object_element(array, i);
        const char* name = json_object_get_string_member(obj, "value");
        const char* desc = json_object_get_string_member(obj, "description");
        if (!name || !desc) {
            continue;
        }

        ConfigOptionData option;
        option.name = name;
        option.description = desc;

        if (json_object_has_member(obj, "type")) {
            option.value_type = static_cast<int>(json_object_get_int_member(obj, "type"));
        }

        if (json_object_has_member(obj, "data")) {
            JsonObject* data_obj = json_object_get_object_member(obj, "data");
            if (option.value_type == 6) {
                option.choice_values_csv = json_string_member_if_string(data_obj, "value");
            }

            option.value = json_node_to_string(data_obj, "current");
            if (option.value.empty()) {
                option.value = json_node_to_string(data_obj, "value");
            }
            if (json_object_has_member(data_obj, "explicit")) {
                option.set_by_user = json_object_get_boolean_member(data_obj, "explicit");
            }

            auto min_value = json_node_to_double(data_obj, "min");
            auto max_value = json_node_to_double(data_obj, "max");
            if (min_value.has_value() && max_value.has_value()) {
                option.has_range = true;
                option.range_min = min_value.value();
                option.range_max = max_value.value();
            }
        }

        if (option.value_type == 0) {
            if (option.value == "1" || option.value == "true") {
                option.value = "true";
            } else if (option.value == "0" || option.value == "false") {
                option.value = "false";
            }
        }

        option.section_path = hyprland::section_path_from_option_name(option.name);
        if (!option.section_path.empty()) {
            snapshot.sections.insert(option.section_path);
        } else {
            snapshot.has_root_options = true;
        }

        snapshot.options.push_back(std::move(option));
    }

    g_object_unref(parser);
    return snapshot;
}
