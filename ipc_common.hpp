#pragma once
#include <cstdint>

namespace IPC {
    enum class RequestType : uint8_t {
        GET_ALL = 0,
        SET_OPTION_PERSIST = 1,  // Write to config file + runtime
        GET_KEYWORDS = 2,        // Get all keyword entries (exec-once, etc.)
        ADD_KEYWORD = 3,         // Add a new keyword entry
        REMOVE_KEYWORD = 4,      // Remove a keyword entry by index
        UPDATE_KEYWORD = 5       // Update a keyword entry
    };

    // Supported keyword types
    enum class KeywordType : uint8_t {
        EXEC_ONCE = 0,
        EXECR_ONCE = 1,
        EXEC = 2,
        EXECR = 3,
        EXEC_SHUTDOWN = 4
    };

    inline const char* keywordTypeToString(KeywordType type) {
        switch (type) {
            case KeywordType::EXEC_ONCE: return "exec-once";
            case KeywordType::EXECR_ONCE: return "execr-once";
            case KeywordType::EXEC: return "exec";
            case KeywordType::EXECR: return "execr";
            case KeywordType::EXEC_SHUTDOWN: return "exec-shutdown";
            default: return "unknown";
        }
    }

    inline KeywordType stringToKeywordType(const std::string& str) {
        if (str == "exec-once") return KeywordType::EXEC_ONCE;
        if (str == "execr-once") return KeywordType::EXECR_ONCE;
        if (str == "exec") return KeywordType::EXEC;
        if (str == "execr") return KeywordType::EXECR;
        if (str == "exec-shutdown") return KeywordType::EXEC_SHUTDOWN;
        return KeywordType::EXEC_ONCE; // default
    }

    const char* SOCKET_PATH = "/tmp/hyprland-settings.sock";
}