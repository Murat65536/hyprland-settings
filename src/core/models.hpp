#ifndef CORE_MODELS_HPP
#define CORE_MODELS_HPP

#include <set>
#include <string>
#include <vector>

struct ConfigOptionData {
    std::string name;
    std::string value;
    std::string description;
    std::string choice_values_csv;
    bool set_by_user = false;
    int value_type = -1;
    bool has_range = false;
    double range_min = 0.0;
    double range_max = 1.0;
    std::string section_path;
};

struct SettingsSnapshot {
    std::vector<std::string> available_devices;
    std::set<std::string> sections;
    std::vector<ConfigOptionData> options;
    bool has_root_options = false;
};

#endif
