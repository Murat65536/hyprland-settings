#pragma once
#include <cstdint>

namespace IPC {
    enum class OptionType : uint8_t {
        INT = 0,
        FLOAT = 1,
        STRING = 2,
        VEC2 = 3,
        UNKNOWN = 255
    };

    enum class RequestType : uint8_t {
        GET_ALL = 0,
        SET_OPTION = 1
    };

    const char* SOCKET_PATH = "/tmp/hyprland-settings.sock";
}