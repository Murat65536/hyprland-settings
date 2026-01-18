#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/ConfigDataValues.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprlang.hpp>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <any>
#include <typeindex>
#include <set>
#include "ipc_common.hpp"
#include "config_writer.hpp"

inline HANDLE PHANDLE = nullptr;
std::thread g_ipcThread;
bool g_runIPC = true;

// Helper to write data to socket
void writeData(int fd, const void* data, size_t size) {
    write(fd, data, size);
}

void writeString(int fd, const std::string& str) {
    uint32_t len = str.length();
    writeData(fd, &len, sizeof(len));
    if (len > 0)
        writeData(fd, str.c_str(), len);
}

// Helper to read data from socket
bool readData(int fd, void* buffer, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t r = read(fd, (char*)buffer + total, size - total);
        if (r <= 0) return false;
        total += r;
    }
    return true;
}

std::string readString(int fd) {
    uint32_t len = 0;
    if (!readData(fd, &len, sizeof(len))) return "";
    if (len == 0) return "";
    std::vector<char> buf(len + 1);
    if (!readData(fd, buf.data(), len)) return "";
    buf[len] = '\0';
    return std::string(buf.data());
}

void handleClient(int client_fd) {
    IPC::RequestType reqType;
    if (!readData(client_fd, &reqType, sizeof(reqType))) {
        close(client_fd);
        return;
    }

    if (reqType == IPC::RequestType::GET_ALL) {
        const auto& descriptions = g_pConfigManager->getAllDescriptions();
        
        // Send count
        uint32_t count = descriptions.size();
        writeData(client_fd, &count, sizeof(count));

        for (const auto& desc : descriptions) {
            auto* configVal = g_pConfigManager->getHyprlangConfigValuePtr(desc.value);
            if (!configVal) {
                 writeString(client_fd, desc.value);
                 writeString(client_fd, desc.description);
                 writeString(client_fd, "");
                 uint8_t setByUser = 0;
                 writeData(client_fd, &setByUser, sizeof(setByUser));
                 continue;
            }

            writeString(client_fd, desc.value);
            writeString(client_fd, desc.description);

            // Implementation based on Hyprland's dispatchGetOption in src/debug/HyprCtl.cpp
            const auto VAL  = configVal->getValue();
            const auto TYPE = std::type_index(VAL.type());

            if (TYPE == typeid(Hyprlang::INT)) {
                writeString(client_fd, std::to_string(std::any_cast<Hyprlang::INT>(VAL)));
            } else if (TYPE == typeid(Hyprlang::FLOAT)) {
                writeString(client_fd, std::to_string(std::any_cast<Hyprlang::FLOAT>(VAL)));
            } else if (TYPE == typeid(Hyprlang::STRING)) {
                writeString(client_fd, std::any_cast<Hyprlang::STRING>(VAL));
            } else if (TYPE == typeid(Hyprlang::VEC2)) {
                auto vec = std::any_cast<Hyprlang::VEC2>(VAL);
                writeString(client_fd, std::to_string(vec.x) + "," + std::to_string(vec.y));
            } else if (TYPE == typeid(void*)) {
                // Custom config types (gradients, css gaps, etc.) implement ICustomConfigValueData
                writeString(client_fd, static_cast<ICustomConfigValueData*>(std::any_cast<void*>(VAL))->toString());
            } else {
                writeString(client_fd, "UNKNOWN");
            }
            
            // Send m_bSetByUser flag
            uint8_t setByUser = configVal->m_bSetByUser ? 1 : 0;
            writeData(client_fd, &setByUser, sizeof(setByUser));
        }
    }
    else if (reqType == IPC::RequestType::SET_OPTION_PERSIST) {
        std::string name = readString(client_fd);
        std::string value = readString(client_fd);

        if (!name.empty()) {
            // First, check if it was set by user (helps with decision-making on GUI side)
            auto* configVal = g_pConfigManager->getHyprlangConfigValuePtr(name);
            bool setByUser = configVal ? configVal->m_bSetByUser : false;
            
            // Write to config file
            auto writeResult = ConfigWriter::updateConfigValue(name, value, setByUser);
            
            if (writeResult.success) {
                // Apply the config change at runtime too
                std::string parseResult = g_pConfigManager->parseKeyword(name, value);
                if (parseResult.empty()) {
                    writeString(client_fd, writeResult.wasExisting ? "OK:UPDATED" : "OK:ADDED");
                } else {
                    writeString(client_fd, "PARSE_ERROR:" + parseResult);
                }
            } else {
                writeString(client_fd, "WRITE_ERROR:" + writeResult.error);
            }
        } else {
            writeString(client_fd, "Invalid Name");
        }
    }
    else if (reqType == IPC::RequestType::GET_KEYWORDS) {
        auto entries = ConfigWriter::getKeywordEntries();
        
        uint32_t count = entries.size();
        writeData(client_fd, &count, sizeof(count));
        
        for (const auto& entry : entries) {
            writeString(client_fd, entry.type);
            writeString(client_fd, entry.value);
            writeString(client_fd, entry.filePath);
            int32_t lineNum = entry.lineNumber;
            writeData(client_fd, &lineNum, sizeof(lineNum));
        }
    }
    else if (reqType == IPC::RequestType::ADD_KEYWORD) {
        std::string type = readString(client_fd);
        std::string value = readString(client_fd);
        
        auto result = ConfigWriter::addKeywordEntry(type, value);
        
        if (result.success) {
            // Apply at runtime for exec keywords
            g_pConfigManager->parseKeyword(type, value);
            writeString(client_fd, "OK:ADDED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }
    else if (reqType == IPC::RequestType::REMOVE_KEYWORD) {
        std::string filePath = readString(client_fd);
        int32_t lineNumber;
        readData(client_fd, &lineNumber, sizeof(lineNumber));
        
        auto result = ConfigWriter::removeKeywordEntry(filePath, lineNumber);
        
        if (result.success) {
            writeString(client_fd, "OK:REMOVED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }
    else if (reqType == IPC::RequestType::UPDATE_KEYWORD) {
        std::string filePath = readString(client_fd);
        int32_t lineNumber;
        readData(client_fd, &lineNumber, sizeof(lineNumber));
        std::string type = readString(client_fd);
        std::string value = readString(client_fd);
        
        auto result = ConfigWriter::updateKeywordEntry(filePath, lineNumber, type, value);
        
        if (result.success) {
            writeString(client_fd, "OK:UPDATED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }
    else if (reqType == IPC::RequestType::GET_DEVICES) {
        // Get device list directly from InputManager
        std::set<std::string> deviceNames;
        
        // Collect all device names from InputManager
        for (const auto& kb : g_pInputManager->m_keyboards) {
            if (kb && !kb->m_hlName.empty())
                deviceNames.insert(kb->m_hlName);
        }
        for (const auto& ptr : g_pInputManager->m_pointers) {
            if (ptr && !ptr->m_hlName.empty())
                deviceNames.insert(ptr->m_hlName);
        }
        for (const auto& touch : g_pInputManager->m_touches) {
            if (touch && !touch->m_hlName.empty())
                deviceNames.insert(touch->m_hlName);
        }
        for (const auto& tablet : g_pInputManager->m_tablets) {
            if (tablet && !tablet->m_hlName.empty())
                deviceNames.insert(tablet->m_hlName);
        }
        
        // Send count then each device name
        uint32_t count = deviceNames.size();
        writeData(client_fd, &count, sizeof(count));
        for (const auto& name : deviceNames) {
            writeString(client_fd, name);
        }
    }
    else if (reqType == IPC::RequestType::GET_DEVICE_CONFIGS) {
        auto entries = ConfigWriter::getDeviceConfigEntries();
        
        uint32_t count = entries.size();
        writeData(client_fd, &count, sizeof(count));
        
        for (const auto& entry : entries) {
            writeString(client_fd, entry.deviceName);
            writeString(client_fd, entry.option);
            writeString(client_fd, entry.value);
            writeString(client_fd, entry.filePath);
            int32_t startLine = entry.startLine;
            int32_t optionLine = entry.optionLine;
            writeData(client_fd, &startLine, sizeof(startLine));
            writeData(client_fd, &optionLine, sizeof(optionLine));
        }
    }
    else if (reqType == IPC::RequestType::ADD_DEVICE_CONFIG) {
        std::string deviceName = readString(client_fd);
        std::string option = readString(client_fd);
        std::string value = readString(client_fd);
        
        auto result = ConfigWriter::addDeviceConfig(deviceName, option, value);
        
        if (result.success) {
            // Apply at runtime
            g_pConfigManager->parseKeyword("device:" + deviceName + ":" + option, value);
            writeString(client_fd, "OK:ADDED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }
    else if (reqType == IPC::RequestType::UPDATE_DEVICE_CONFIG) {
        std::string filePath = readString(client_fd);
        int32_t lineNumber;
        readData(client_fd, &lineNumber, sizeof(lineNumber));
        std::string deviceName = readString(client_fd);
        std::string option = readString(client_fd);
        std::string value = readString(client_fd);
        
        auto result = ConfigWriter::updateDeviceConfigOption(filePath, lineNumber, option, value);
        
        if (result.success) {
            // Apply at runtime
            g_pConfigManager->parseKeyword("device:" + deviceName + ":" + option, value);
            writeString(client_fd, "OK:UPDATED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }
    else if (reqType == IPC::RequestType::REMOVE_DEVICE_CONFIG) {
        std::string filePath = readString(client_fd);
        int32_t lineNumber;
        readData(client_fd, &lineNumber, sizeof(lineNumber));
        std::string deviceName = readString(client_fd);
        
        auto result = ConfigWriter::removeDeviceConfigOption(filePath, lineNumber, deviceName);
        
        if (result.success) {
            writeString(client_fd, "OK:REMOVED");
        } else {
            writeString(client_fd, "ERROR:" + result.error);
        }
    }

    close(client_fd);
}

void ipcLoop() {
    int server_fd;
    struct sockaddr_un address;

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == 0) {
        HyprlandAPI::addNotification(PHANDLE, "IPC: Socket failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return;
    }

    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, IPC::SOCKET_PATH, sizeof(address.sun_path) - 1);
    unlink(IPC::SOCKET_PATH); 

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        HyprlandAPI::addNotification(PHANDLE, "IPC: Bind failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return;
    }

    if (listen(server_fd, 3) < 0) {
        HyprlandAPI::addNotification(PHANDLE, "IPC: Listen failed", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        return;
    }

    // Set a timeout on accept so we can exit the thread gracefully
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while (g_runIPC) {
        int client_fd;
        int addrlen = sizeof(address);
        client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd >= 0) {
            handleClient(client_fd);
        }
    }
    close(server_fd);
    unlink(IPC::SOCKET_PATH);
}

// This function is called when the plugin is loaded
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    g_ipcThread = std::thread(ipcLoop);
    // g_ipcThread.detach(); // We join now

    HyprlandAPI::addNotification(PHANDLE, "Example Plugin Initialized with IPC", CHyprColor{0.2, 1.0, 0.2, 1.0}, 5000);

    return {"ExamplePlugin", "A simple hello world plugin", "Me", "0.1"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    g_runIPC = false;
    if (g_ipcThread.joinable())
        g_ipcThread.join();
}
