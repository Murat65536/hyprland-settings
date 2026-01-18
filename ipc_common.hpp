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

    const char* SOCKET_PATH = "/tmp/hyprland-settings.sock";
}
