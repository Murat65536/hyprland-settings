#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprlang.hpp>
#include <fstream>
#include <string>
#include <any>
#include <typeindex>
#include <dlfcn.h>

inline HANDLE PHANDLE = nullptr;

// Helper to get the directory where this plugin .so resides
std::string getPluginDirectory() {
    Dl_info info;
    // Pass the address of a function in this shared object
    if (dladdr((void*)getPluginDirectory, &info)) {
        std::string path = info.dli_fname;
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            return path.substr(0, lastSlash);
        }
    }
    return "."; // Fallback to CWD
}

void saveConfigToFile() {
    std::string path = getPluginDirectory() + "/hyprland_settings_dump.txt";
    std::ofstream out(path);
    if (!out.is_open()) return;

    const auto& descriptions = g_pConfigManager->getAllDescriptions();

    for (const auto& desc : descriptions) {
        auto* configVal = g_pConfigManager->getHyprlangConfigValuePtr(desc.value);
        if (!configVal) continue;

        out << "BEGIN_ENTRY\n";
        out << "NAME: " << desc.value << "\n";
        
        std::string description = desc.description;
        std::replace(description.begin(), description.end(), '\n', ' ');
        out << "DESC: " << description << "\n";

        std::string valStr = "";
        const auto VAL  = configVal->getValue();
        const auto TYPE = std::type_index(VAL.type());

        if (TYPE == typeid(Hyprlang::INT)) {
            valStr = std::to_string(std::any_cast<Hyprlang::INT>(VAL));
        } else if (TYPE == typeid(Hyprlang::FLOAT)) {
            valStr = std::to_string(std::any_cast<Hyprlang::FLOAT>(VAL));
        } else if (TYPE == typeid(Hyprlang::STRING)) {
            valStr = std::any_cast<Hyprlang::STRING>(VAL);
        } else if (TYPE == typeid(Hyprlang::VEC2)) {
            auto vec = std::any_cast<Hyprlang::VEC2>(VAL);
            valStr = std::to_string(vec.x) + "," + std::to_string(vec.y);
        } else if (TYPE == typeid(void*)) {
             try {
                valStr = static_cast<ICustomConfigValueData*>(std::any_cast<void*>(VAL))->toString();
             } catch(...) {
                valStr = "UNKNOWN";
             }
        } else {
            valStr = "UNKNOWN";
        }
        
        std::replace(valStr.begin(), valStr.end(), '\n', ' ');

        out << "VALUE: " << valStr << "\n";
        out << "SET_BY_USER: " << (configVal->m_bSetByUser ? "1" : "0") << "\n";
        out << "END_ENTRY\n";
    }
    out.close();
}

// Required for Hyprland plugins
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "Settings Plugin Initialized (Dynamic IO)", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);
    
    saveConfigToFile();

    return {"SettingsFileIO", "File IO Backend for Hyprland Settings App", "1.0", "Author"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    // Cleanup if needed
}
