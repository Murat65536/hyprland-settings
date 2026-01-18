#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <regex>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <optional>
#include <glob.h>

// Use Hyprland's built-in path resolution when available (in plugin context)
#ifdef HYPRLAND_PLUGIN
#include <hyprland/src/helpers/MiscFunctions.hpp>
#endif

namespace ConfigWriter {

// Expand environment variables in a path (e.g., $HOME -> /home/user)
inline std::string expandEnvVars(const std::string& path) {
    std::string result = path;
    size_t pos = 0;
    
    while ((pos = result.find('$', pos)) != std::string::npos) {
        // Find end of variable name
        size_t end = pos + 1;
        while (end < result.length() && (std::isalnum(result[end]) || result[end] == '_')) {
            end++;
        }
        
        if (end > pos + 1) {
            std::string varName = result.substr(pos + 1, end - pos - 1);
            const char* varValue = std::getenv(varName.c_str());
            if (varValue) {
                result.replace(pos, end - pos, varValue);
                pos += std::strlen(varValue);
            } else {
                pos = end;
            }
        } else {
            pos++;
        }
    }
    
    // Handle ~ at start
    if (!result.empty() && result[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            result = std::string(home) + result.substr(1);
        }
    }
    
    return result;
}

// Resolve path relative to a base path (like Hyprland's absolutePath)
inline std::string resolvePath(const std::string& path, const std::string& basePath) {
#ifdef HYPRLAND_PLUGIN
    // Use Hyprland's built-in function when available
    return absolutePath(path, basePath);
#else
    std::string expanded = expandEnvVars(path);
    
    // If already absolute, return as-is
    if (!expanded.empty() && expanded[0] == '/') {
        return expanded;
    }
    
    // Make relative to base path's directory
    std::string baseDir = std::filesystem::path(basePath).parent_path().string();
    if (baseDir.empty()) {
        baseDir = ".";
    }
    
    return baseDir + "/" + expanded;
#endif
}

// Get the main hyprland config path
inline std::string getConfigPath() {
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    std::string configDir;
    
    if (xdgConfig && xdgConfig[0] != '\0') {
        configDir = xdgConfig;
    } else {
        const char* home = std::getenv("HOME");
        if (!home) return "";
        configDir = std::string(home) + "/.config";
    }
    
    return configDir + "/hypr/hyprland.conf";
}

// Parse a config key like "general:gaps_in" into category and variable
inline std::pair<std::string, std::string> parseConfigKey(const std::string& key) {
    size_t pos = key.find(':');
    if (pos == std::string::npos) {
        return {"", key};
    }
    return {key.substr(0, pos), key.substr(pos + 1)};
}

// Trim whitespace from both ends
inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t");
    return str.substr(start, end - start + 1);
}

// Result of searching for a config value
struct SearchResult {
    bool found = false;
    std::string filePath;
    int lineNumber = -1;
    std::string originalLine;
};

// Result of attempting to update config
struct UpdateResult {
    bool success = false;
    bool wasExisting = false;
    std::string filePath;
    std::string error;
};

// Expand glob patterns using system glob() - same as Hyprland uses
inline std::vector<std::string> expandGlob(const std::string& pattern) {
    std::vector<std::string> results;
    
    glob_t glob_buf;
    int r = glob(pattern.c_str(), GLOB_TILDE | GLOB_NOCHECK, nullptr, &glob_buf);
    
    if (r == 0) {
        for (size_t i = 0; i < glob_buf.gl_pathc; i++) {
            results.push_back(glob_buf.gl_pathv[i]);
        }
    }
    
    globfree(&glob_buf);
    return results;
}

// Recursively collect all config files (main + sourced)
// hyprVars tracks Hyprland variable definitions (e.g., $configs = /path)
inline std::vector<std::string> collectConfigFilesWithVars(
    const std::string& configPath,
    std::map<std::string, std::string>& hyprVars
) {
    std::vector<std::string> files;
    
    if (!std::filesystem::exists(configPath)) {
        return files;
    }
    
    files.push_back(configPath);
    
    std::ifstream inFile(configPath);
    if (!inFile) return files;
    
    std::string line;
    std::regex sourceRegex(R"(^\s*source\s*[=:]\s*(.+)\s*$)");
    std::regex varDefRegex(R"(^\s*\$(\w+)\s*=\s*(.+)\s*$)");
    
    while (std::getline(inFile, line)) {
        // Remove comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        std::smatch match;
        
        // Check for Hyprland variable definition ($varname = value)
        if (std::regex_match(line, match, varDefRegex)) {
            std::string varName = match[1].str();
            std::string varValue = trim(match[2].str());
            // Expand any variables in the value itself
            for (const auto& [name, val] : hyprVars) {
                size_t pos;
                while ((pos = varValue.find("$" + name)) != std::string::npos) {
                    varValue.replace(pos, name.length() + 1, val);
                }
            }
            varValue = expandEnvVars(varValue);
            hyprVars[varName] = varValue;
            continue;
        }
        
        // Check for source directive
        if (std::regex_match(line, match, sourceRegex)) {
            std::string sourcePath = trim(match[1].str());
            
            // Expand Hyprland variables first
            for (const auto& [name, val] : hyprVars) {
                size_t pos;
                while ((pos = sourcePath.find("$" + name)) != std::string::npos) {
                    sourcePath.replace(pos, name.length() + 1, val);
                }
            }
            
            // Resolve path relative to current config (handles $HOME, ~, etc.)
            std::string resolved = resolvePath(sourcePath, configPath);
            
            // Use glob to expand wildcards
            auto expanded = expandGlob(resolved);
            
            for (const auto& path : expanded) {
                if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
                    auto subFiles = collectConfigFilesWithVars(path, hyprVars);
                    files.insert(files.end(), subFiles.begin(), subFiles.end());
                }
            }
        }
    }
    
    return files;
}

inline std::vector<std::string> collectConfigFiles(const std::string& configPath) {
    std::map<std::string, std::string> hyprVars;
    return collectConfigFilesWithVars(configPath, hyprVars);
}

// Search for a config value across all config files
// Returns the LAST occurrence (since that's what takes effect)
inline SearchResult findConfigValue(const std::string& key) {
    SearchResult result;
    std::string mainConfig = getConfigPath();
    
    if (mainConfig.empty()) {
        return result;
    }
    
    auto [category, varName] = parseConfigKey(key);
    auto configFiles = collectConfigFiles(mainConfig);
    
    for (const auto& filePath : configFiles) {
        std::ifstream inFile(filePath);
        if (!inFile) continue;
        
        std::string line;
        int lineNum = 0;
        bool inTargetCategory = category.empty();
        int braceDepth = 0;
        
        while (std::getline(inFile, line)) {
            lineNum++;
            
            std::string trimmedLine = line;
            size_t commentPos = trimmedLine.find('#');
            if (commentPos != std::string::npos) {
                trimmedLine = trimmedLine.substr(0, commentPos);
            }
            trimmedLine = trim(trimmedLine);
            
            if (trimmedLine.empty()) continue;
            
            // Check for category start
            if (!category.empty()) {
                if (!inTargetCategory) {
                    if (trimmedLine == category + " {" || trimmedLine == category + "{") {
                        inTargetCategory = true;
                        braceDepth = 1;
                        continue;
                    }
                } else {
                    for (char c : trimmedLine) {
                        if (c == '{') braceDepth++;
                        else if (c == '}') braceDepth--;
                    }
                    if (braceDepth == 0) {
                        inTargetCategory = false;
                        continue;
                    }
                }
            }
            
            // Check for variable match
            if (inTargetCategory || category.empty()) {
                std::string varPattern = varName + " =";
                std::string varPattern2 = varName + "=";
                
                if (trimmedLine.starts_with(varPattern) || trimmedLine.starts_with(varPattern2)) {
                    result.found = true;
                    result.filePath = filePath;
                    result.lineNumber = lineNum;
                    result.originalLine = line;
                }
            }
        }
    }
    
    return result;
}

// Update a config value in its source file, or add to main config if not found
inline UpdateResult updateConfigValue(const std::string& key, const std::string& value, bool setByUser) {
    UpdateResult result;
    std::string mainConfig = getConfigPath();
    
    if (mainConfig.empty()) {
        result.error = "Could not determine config path";
        return result;
    }
    
    auto [category, varName] = parseConfigKey(key);
    
    // First, search for existing definition
    SearchResult search = findConfigValue(key);
    
    if (search.found) {
        // Update existing line in the file where it was found
        std::ifstream inFile(search.filePath);
        if (!inFile) {
            result.error = "Could not read file: " + search.filePath;
            return result;
        }
        
        std::vector<std::string> lines;
        std::string line;
        int lineNum = 0;
        
        while (std::getline(inFile, line)) {
            lineNum++;
            if (lineNum == search.lineNumber) {
                size_t indentEnd = line.find_first_not_of(" \t");
                std::string indent = (indentEnd != std::string::npos) ? line.substr(0, indentEnd) : "";
                lines.push_back(indent + varName + " = " + value);
            } else {
                lines.push_back(line);
            }
        }
        inFile.close();
        
        std::ofstream outFile(search.filePath);
        if (!outFile) {
            result.error = "Could not write to file: " + search.filePath;
            return result;
        }
        
        for (size_t i = 0; i < lines.size(); ++i) {
            outFile << lines[i];
            if (i < lines.size() - 1) outFile << "\n";
        }
        outFile << "\n";
        outFile.close();
        
        result.success = true;
        result.wasExisting = true;
        result.filePath = search.filePath;
        return result;
    }
    
    // Not found anywhere - add to main config file
    if (!std::filesystem::exists(mainConfig)) {
        result.error = "Main config file does not exist: " + mainConfig;
        return result;
    }
    
    std::ifstream inFile(mainConfig);
    if (!inFile) {
        result.error = "Could not read main config file";
        return result;
    }
    
    std::vector<std::string> lines;
    std::string line;
    bool inTargetCategory = category.empty();
    int categoryStartLine = -1;
    int categoryEndLine = -1;
    int braceDepth = 0;
    int lineNum = 0;
    
    while (std::getline(inFile, line)) {
        lines.push_back(line);
        lineNum++;
        
        std::string trimmedLine = line;
        size_t commentPos = trimmedLine.find('#');
        if (commentPos != std::string::npos) {
            trimmedLine = trimmedLine.substr(0, commentPos);
        }
        trimmedLine = trim(trimmedLine);
        
        if (trimmedLine.empty()) continue;
        
        if (!category.empty() && !inTargetCategory) {
            if (trimmedLine == category + " {" || trimmedLine == category + "{") {
                inTargetCategory = true;
                categoryStartLine = lineNum - 1;
                braceDepth = 1;
                continue;
            }
        }
        
        if (inTargetCategory && !category.empty()) {
            for (char c : trimmedLine) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }
            if (braceDepth == 0) {
                categoryEndLine = lineNum - 1;
                inTargetCategory = false;
            }
        }
    }
    inFile.close();
    
    // Add the value
    if (category.empty()) {
        lines.push_back(varName + " = " + value);
    } else if (categoryStartLine >= 0) {
        int insertLine = (categoryEndLine >= 0) ? categoryEndLine : categoryStartLine + 1;
        lines.insert(lines.begin() + insertLine, "    " + varName + " = " + value);
    } else {
        lines.push_back("");
        lines.push_back(category + " {");
        lines.push_back("    " + varName + " = " + value);
        lines.push_back("}");
    }
    
    std::ofstream outFile(mainConfig);
    if (!outFile) {
        result.error = "Could not write to main config file";
        return result;
    }
    
    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i < lines.size() - 1) outFile << "\n";
    }
    outFile << "\n";
    outFile.close();
    
    result.success = true;
    result.wasExisting = false;
    result.filePath = mainConfig;
    return result;
}

} // namespace ConfigWriter
