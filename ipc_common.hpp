#pragma once
#include <cstdint>

namespace IPC {
    enum class RequestType : uint8_t {
        GET_ALL = 0,
        SET_OPTION = 1
    };

    const char* SOCKET_PATH = "/tmp/hyprland-settings.sock";
}