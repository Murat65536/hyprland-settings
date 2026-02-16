#ifndef CORE_MODELS_HPP
#define CORE_MODELS_HPP

#include <set>
#include <string>
#include <vector>

struct ConfigOptionData {
    std::string name;
    std::string value;
    std::string description;
    bool set_by_user = false;
    bool is_boolean = false;
    std::string section_path;
};

struct SettingsSnapshot {
    std::vector<std::string> available_devices;
    std::set<std::string> sections;
    std::vector<ConfigOptionData> options;
    bool has_root_options = false;
};

#endif
