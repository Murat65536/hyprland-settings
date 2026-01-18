#pragma once
#include <cstdint>

namespace IPC {
    enum class RequestType : uint8_t {
        GET_ALL = 0,
        SET_OPTION_PERSIST = 1  // Write to config file + runtime
    };

    const char* SOCKET_PATH = "/tmp/hyprland-settings.sock";
}