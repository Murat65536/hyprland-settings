#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprlang.hpp>
#include <fstream>
#include <iostream>
#include <any>

inline HANDLE PHANDLE = nullptr;

void printAllOptions() {
    std::ofstream out("/tmp/hyprland_options.txt");
    if (!out.is_open()) {
        HyprlandAPI::addNotification(PHANDLE, "Failed to open output file!", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return;
    }

    out << "Hyprland Configuration Options:\n==============================\n";

    // getAllDescriptions() returns a const reference to a vector of SConfigOptionDescription
    // We assume this method is exported and available.
    const auto& descriptions = g_pConfigManager->getAllDescriptions();

    for (const auto& desc : descriptions) {
        out << "Option: " << desc.value << "\n";
        out << "  Description: " << desc.description << "\n";

        // Try to get the current value from Hyprlang config
        auto* configVal = g_pConfigManager->getHyprlangConfigValuePtr(desc.value);
        if (configVal) {
            try {
                // Hyprlang::CConfigValue::getValue() returns std::any
                auto anyVal = configVal->getValue();
                
                if (anyVal.type() == typeid(Hyprlang::INT)) {
                    out << "  Value: " << std::any_cast<Hyprlang::INT>(anyVal) << " (INT)\n";
                } else if (anyVal.type() == typeid(Hyprlang::FLOAT)) {
                    out << "  Value: " << std::any_cast<Hyprlang::FLOAT>(anyVal) << " (FLOAT)\n";
                } else if (anyVal.type() == typeid(Hyprlang::STRING)) {
                    out << "  Value: " << std::any_cast<Hyprlang::STRING>(anyVal) << " (STRING)\n";
                } else if (anyVal.type() == typeid(Hyprlang::VEC2)) {
                    auto vec = std::any_cast<Hyprlang::VEC2>(anyVal);
                    out << "  Value: [" << vec.x << ", " << vec.y << "] (VEC2)\n";
                } else {
                    out << "  Value: [Complex/Unknown Type]\n";
                }
            } catch (const std::exception& e) {
                out << "  Value: [Error retrieving value: " << e.what() << "]\n";
            }
        } else {
            out << "  Value: [Not found in ConfigManager]\n";
        }
        out << "\n";
    }

    out.close();
    HyprlandAPI::addNotification(PHANDLE, "Config options dumped to /tmp/hyprland_options.txt", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
}

// This function is called when the plugin is loaded
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    printAllOptions();

    return {"ExamplePlugin", "A simple hello world plugin", "Me", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // Cleanup code here
}
