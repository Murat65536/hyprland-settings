#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprlang.hpp>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <any>
#include "ipc_common.hpp"

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
                 continue;
            }

            writeString(client_fd, desc.value);
            writeString(client_fd, desc.description);

            try {
                auto anyVal = configVal->getValue();
                if (anyVal.type() == typeid(Hyprlang::INT)) {
                    int64_t val = std::any_cast<Hyprlang::INT>(anyVal);
                    writeString(client_fd, std::to_string(val));
                } else if (anyVal.type() == typeid(Hyprlang::FLOAT)) {
                    double val = std::any_cast<Hyprlang::FLOAT>(anyVal);
                    writeString(client_fd, std::to_string(val));
                } else if (anyVal.type() == typeid(Hyprlang::STRING)) {
                    writeString(client_fd, std::any_cast<Hyprlang::STRING>(anyVal));
                } else if (anyVal.type() == typeid(Hyprlang::VEC2)) {
                    auto vec = std::any_cast<Hyprlang::VEC2>(anyVal);
                    writeString(client_fd, std::to_string(vec.x) + "," + std::to_string(vec.y));
                } else {
                    writeString(client_fd, "UNKNOWN");
                }
            } catch (...) {
                 writeString(client_fd, "ERROR");
            }
        }
    } 
    else if (reqType == IPC::RequestType::SET_OPTION) {
        std::string name = readString(client_fd);
        std::string value = readString(client_fd);

        if (!name.empty()) {
            // Apply the config change
            // g_pConfigManager->parseKeyword returns an error string on failure, empty on success
            std::string result = g_pConfigManager->parseKeyword(name, value);
            writeString(client_fd, result.empty() ? "OK" : result);
        } else {
            writeString(client_fd, "Invalid Name");
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
