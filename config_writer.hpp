#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <regex>
#include <map>
#include <filesystem>
#include <cstdlib>
#include <glob.h>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/config/ConfigManager.hpp>

namespace ConfigWriter {

// Expand environment variables in a path (e.g., $HOME -> /home/user)
inline std::string expandEnvVars(const std::string& path) {
    std::string result = path;
    size_t pos = 0;
    
    while ((pos = result.find('$', pos)) != std::string::npos) {
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
    
    if (!result.empty() && result[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            result = std::string(home) + result.substr(1);
        }
    }
    
    return result;
}

inline std::string getConfigPath() {
    return g_pConfigManager->getMainConfigPath();
}

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

inline std::vector<std::string> expandGlob(const std::string& pattern) {
    std::vector<std::string> results;
    glob_t glob_buf;
    
    if (glob(pattern.c_str(), GLOB_TILDE | GLOB_NOCHECK, nullptr, &glob_buf) == 0) {
        for (size_t i = 0; i < glob_buf.gl_pathc; i++) {
            results.push_back(glob_buf.gl_pathv[i]);
        }
    }
    
    globfree(&glob_buf);
    return results;
}

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
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        std::smatch match;
        
        if (std::regex_match(line, match, varDefRegex)) {
            std::string varName = match[1].str();
            std::string varValue = trim(match[2].str());
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
        
        if (std::regex_match(line, match, sourceRegex)) {
            std::string sourcePath = trim(match[1].str());
            
            for (const auto& [name, val] : hyprVars) {
                size_t pos;
                while ((pos = sourcePath.find("$" + name)) != std::string::npos) {
                    sourcePath.replace(pos, name.length() + 1, val);
                }
            }
            
            std::string resolved = absolutePath(sourcePath, configPath);
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

inline SearchResult findConfigValue(const std::string& key) {
    SearchResult result;
    std::string mainConfig = getConfigPath();
    if (mainConfig.empty()) return result;
    
    // Parse nested categories (e.g., "decoration:shadow:enabled")
    std::vector<std::string> parts;
    size_t pos = 0;
    size_t colonPos;
    while ((colonPos = key.find(':', pos)) != std::string::npos) {
        parts.push_back(key.substr(pos, colonPos - pos));
        pos = colonPos + 1;
    }
    parts.push_back(key.substr(pos));
    
    std::string varName = parts.back();
    parts.pop_back();
    std::vector<std::string> categories = parts;
    
    auto configFiles = collectConfigFiles(mainConfig);
    
    for (const auto& filePath : configFiles) {
        std::ifstream inFile(filePath);
        if (!inFile) continue;
        
        std::string line;
        int lineNum = 0;
        std::vector<int> categoryDepths(categories.size(), 0);
        int currentCategoryMatch = 0;
        bool inTargetCategory = categories.empty();
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
            
            if (!categories.empty()) {
                if (!inTargetCategory) {
                    // Check if we're entering the next expected category
                    if (currentCategoryMatch < (int)categories.size()) {
                        const std::string& expectedCat = categories[currentCategoryMatch];
                        size_t catLen = expectedCat.length();
                        if (trimmedLine.length() >= catLen && 
                            trimmedLine.substr(0, catLen) == expectedCat) {
                            std::string remainder = trim(trimmedLine.substr(catLen));
                            if (remainder == "{") {
                                currentCategoryMatch++;
                                braceDepth = 1;
                                if (currentCategoryMatch == (int)categories.size()) {
                                    inTargetCategory = true;
                                }
                                continue;
                            }
                        }
                    }
                } else {
                    for (char c : trimmedLine) {
                        if (c == '{') braceDepth++;
                        else if (c == '}') braceDepth--;
                    }
                    if (braceDepth == 0) {
                        inTargetCategory = false;
                        currentCategoryMatch = 0;
                        continue;
                    }
                }
            }
            
            if (inTargetCategory || categories.empty()) {
                size_t varLen = varName.length();
                if (trimmedLine.length() >= varLen && 
                    trimmedLine.substr(0, varLen) == varName) {
                    std::string remainder = trim(trimmedLine.substr(varLen));
                    if (remainder.length() > 0 && remainder[0] == '=') {
                        result.found = true;
                        result.filePath = filePath;
                        result.lineNumber = lineNum;
                        result.originalLine = line;
                    }
                }
            }
        }
    }
    
    return result;
}

inline UpdateResult updateConfigValue(const std::string& key, const std::string& value, bool setByUser) {
    UpdateResult result;
    std::string mainConfig = getConfigPath();
    
    if (mainConfig.empty()) {
        result.error = "Could not determine config path";
        return result;
    }
    
    // Parse nested categories (e.g., "decoration:shadow:enabled")
    std::vector<std::string> parts;
    size_t pos = 0;
    size_t colonPos;
    while ((colonPos = key.find(':', pos)) != std::string::npos) {
        parts.push_back(key.substr(pos, colonPos - pos));
        pos = colonPos + 1;
    }
    parts.push_back(key.substr(pos));
    
    std::string varName = parts.back();
    parts.pop_back();
    std::vector<std::string> categories = parts;
    
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
    int currentCategoryMatch = 0;
    bool inTargetCategory = categories.empty();
    std::vector<int> categoryStartLines;
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
        
        if (!categories.empty() && !inTargetCategory) {
            if (currentCategoryMatch < (int)categories.size()) {
                const std::string& expectedCat = categories[currentCategoryMatch];
                size_t catLen = expectedCat.length();
                if (trimmedLine.length() >= catLen && 
                    trimmedLine.substr(0, catLen) == expectedCat) {
                    std::string remainder = trim(trimmedLine.substr(catLen));
                    if (remainder == "{") {
                        categoryStartLines.push_back(lineNum - 1);
                        currentCategoryMatch++;
                        braceDepth = 1;
                        if (currentCategoryMatch == (int)categories.size()) {
                            inTargetCategory = true;
                        }
                        continue;
                    }
                }
            }
        }
        
        if (inTargetCategory && !categories.empty()) {
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
    std::string indent(categories.size() * 4, ' ');
    if (categories.empty()) {
        lines.push_back(varName + " = " + value);
    } else if (!categoryStartLines.empty() && categoryStartLines.size() == categories.size()) {
        // All categories exist, insert at the end of the innermost category
        int insertLine = (categoryEndLine >= 0) ? categoryEndLine : categoryStartLines.back() + 1;
        lines.insert(lines.begin() + insertLine, indent + varName + " = " + value);
    } else {
        // Need to create missing categories
        lines.push_back("");
        for (size_t i = 0; i < categories.size(); i++) {
            std::string catIndent(i * 4, ' ');
            lines.push_back(catIndent + categories[i] + " {");
        }
        lines.push_back(indent + varName + " = " + value);
        for (int i = categories.size() - 1; i >= 0; i--) {
            std::string catIndent(i * 4, ' ');
            lines.push_back(catIndent + "}");
        }
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
