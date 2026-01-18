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

// Result of scanning a file for category matches
struct CategoryScanResult {
    int maxCategoryMatch = 0;
    std::vector<int> categoryStartLines;
    std::vector<int> categoryEndLines;
};

// Parse a key like "decoration:shadow:enabled" into categories and varName
inline void parseKey(const std::string& key, std::vector<std::string>& categories, std::string& varName) {
    std::vector<std::string> parts;
    size_t pos = 0;
    size_t colonPos;
    while ((colonPos = key.find(':', pos)) != std::string::npos) {
        parts.push_back(key.substr(pos, colonPos - pos));
        pos = colonPos + 1;
    }
    parts.push_back(key.substr(pos));
    
    varName = parts.back();
    parts.pop_back();
    categories = parts;
}

// Scan a file for category matches
inline CategoryScanResult scanFileForCategories(const std::string& filePath, const std::vector<std::string>& categories) {
    CategoryScanResult result;
    if (categories.empty()) return result;
    
    std::ifstream inFile(filePath);
    if (!inFile) return result;
    
    std::string line;
    int currentCategoryMatch = 0;
    int braceDepth = 0;
    int lineNum = 0;
    
    while (std::getline(inFile, line)) {
        lineNum++;
        
        std::string trimmedLine = line;
        size_t commentPos = trimmedLine.find('#');
        if (commentPos != std::string::npos) {
            trimmedLine = trimmedLine.substr(0, commentPos);
        }
        trimmedLine = trim(trimmedLine);
        
        if (trimmedLine.empty()) continue;
        
        if (currentCategoryMatch < (int)categories.size()) {
            const std::string& expectedCat = categories[currentCategoryMatch];
            size_t catLen = expectedCat.length();
            if (trimmedLine.length() >= catLen && 
                trimmedLine.substr(0, catLen) == expectedCat) {
                std::string remainder = trim(trimmedLine.substr(catLen));
                if (remainder == "{") {
                    result.categoryStartLines.push_back(lineNum - 1);
                    result.categoryEndLines.push_back(-1);
                    currentCategoryMatch++;
                    braceDepth = 1;
                    if (currentCategoryMatch > result.maxCategoryMatch) {
                        result.maxCategoryMatch = currentCategoryMatch;
                    }
                    continue;
                }
            }
        }
        
        if (currentCategoryMatch > 0) {
            for (char c : trimmedLine) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }
            if (braceDepth == 0) {
                result.categoryEndLines[currentCategoryMatch - 1] = lineNum - 1;
                currentCategoryMatch--;
                if (currentCategoryMatch > 0) {
                    braceDepth = 1;
                }
            }
        }
    }
    
    return result;
}

// Write lines to a file
inline bool writeLinesToFile(const std::string& filePath, const std::vector<std::string>& lines) {
    std::ofstream outFile(filePath);
    if (!outFile) return false;
    
    for (size_t i = 0; i < lines.size(); ++i) {
        outFile << lines[i];
        if (i < lines.size() - 1) outFile << "\n";
    }
    outFile << "\n";
    outFile.close();
    return true;
}

// Read all lines from a file
inline std::vector<std::string> readLinesFromFile(const std::string& filePath) {
    std::vector<std::string> lines;
    std::ifstream inFile(filePath);
    if (!inFile) return lines;
    
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    return lines;
}

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
    
    std::vector<std::string> categories;
    std::string varName;
    parseKey(key, categories, varName);
    
    auto configFiles = collectConfigFiles(mainConfig);
    
    for (const auto& filePath : configFiles) {
        std::ifstream inFile(filePath);
        if (!inFile) continue;
        
        std::string line;
        int lineNum = 0;
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
    
    std::vector<std::string> categories;
    std::string varName;
    parseKey(key, categories, varName);
    
    SearchResult search = findConfigValue(key);
    
    if (search.found) {
        std::vector<std::string> lines = readLinesFromFile(search.filePath);
        if (lines.empty() && std::filesystem::exists(search.filePath)) {
            result.error = "Could not read file: " + search.filePath;
            return result;
        }
        
        if (search.lineNumber > 0 && search.lineNumber <= (int)lines.size()) {
            std::string& targetLine = lines[search.lineNumber - 1];
            size_t indentEnd = targetLine.find_first_not_of(" \t");
            std::string indent = (indentEnd != std::string::npos) ? targetLine.substr(0, indentEnd) : "";
            targetLine = indent + varName + " = " + value;
        }
        
        if (!writeLinesToFile(search.filePath, lines)) {
            result.error = "Could not write to file: " + search.filePath;
            return result;
        }
        
        result.success = true;
        result.wasExisting = true;
        result.filePath = search.filePath;
        return result;
    }
    
    // Not found - search all config files for parent sections
    auto configFiles = collectConfigFiles(mainConfig);
    
    std::string targetFile = mainConfig;
    CategoryScanResult bestScan;
    
    for (const auto& filePath : configFiles) {
        if (!std::filesystem::exists(filePath)) continue;
        
        CategoryScanResult scan = scanFileForCategories(filePath, categories);
        if (scan.maxCategoryMatch > bestScan.maxCategoryMatch) {
            bestScan = scan;
            targetFile = filePath;
        }
    }
    
    std::vector<std::string> lines = readLinesFromFile(targetFile);
    if (lines.empty() && std::filesystem::exists(targetFile)) {
        result.error = "Could not read config file: " + targetFile;
        return result;
    }
    
    // Add the value
    if (categories.empty()) {
        lines.push_back(varName + " = " + value);
    } else if (bestScan.maxCategoryMatch == (int)categories.size()) {
        // All categories exist
        std::string indent(categories.size() * 4, ' ');
        int insertLine = bestScan.categoryEndLines.back();
        if (insertLine < 0) insertLine = bestScan.categoryStartLines.back() + 1;
        lines.insert(lines.begin() + insertLine, indent + varName + " = " + value);
    } else if (bestScan.maxCategoryMatch > 0) {
        // Partial match - create missing nested sections
        int insertLine = bestScan.categoryEndLines[bestScan.maxCategoryMatch - 1];
        if (insertLine < 0) insertLine = bestScan.categoryStartLines[bestScan.maxCategoryMatch - 1] + 1;
        
        std::vector<std::string> newLines;
        for (size_t i = bestScan.maxCategoryMatch; i < categories.size(); i++) {
            std::string catIndent(i * 4, ' ');
            newLines.push_back(catIndent + categories[i] + " {");
        }
        std::string valueIndent(categories.size() * 4, ' ');
        newLines.push_back(valueIndent + varName + " = " + value);
        for (int i = categories.size() - 1; i >= bestScan.maxCategoryMatch; i--) {
            std::string catIndent(i * 4, ' ');
            newLines.push_back(catIndent + "}");
        }
        
        lines.insert(lines.begin() + insertLine, newLines.begin(), newLines.end());
    } else {
        // No parent categories exist
        lines.push_back("");
        for (size_t i = 0; i < categories.size(); i++) {
            std::string catIndent(i * 4, ' ');
            lines.push_back(catIndent + categories[i] + " {");
        }
        std::string indent(categories.size() * 4, ' ');
        lines.push_back(indent + varName + " = " + value);
        for (int i = categories.size() - 1; i >= 0; i--) {
            std::string catIndent(i * 4, ' ');
            lines.push_back(catIndent + "}");
        }
    }
    
    if (!writeLinesToFile(targetFile, lines)) {
        result.error = "Could not write to config file: " + targetFile;
        return result;
    }
    
    result.success = true;
    result.wasExisting = false;
    result.filePath = targetFile;
    return result;
}

// Structure to hold a keyword entry
struct KeywordEntry {
    std::string type;      // exec-once, execr-once, etc.
    std::string value;     // The command
    std::string filePath;  // Which config file it's in
    int lineNumber;        // Line number in that file
};

// Get all keyword entries from config files
inline std::vector<KeywordEntry> getKeywordEntries() {
    std::vector<KeywordEntry> entries;
    std::string mainConfig = getConfigPath();
    if (mainConfig.empty()) return entries;
    
    auto configFiles = collectConfigFiles(mainConfig);
    
    // Regex for keyword lines: exec-once = ..., execr-once = ..., etc.
    std::regex keywordRegex(R"(^\s*(exec-once|execr-once|exec|execr|exec-shutdown)\s*[=:]\s*(.+)\s*$)");
    
    for (const auto& filePath : configFiles) {
        std::ifstream inFile(filePath);
        if (!inFile) continue;
        
        std::string line;
        int lineNum = 0;
        
        while (std::getline(inFile, line)) {
            lineNum++;
            
            // Remove comments
            std::string checkLine = line;
            size_t commentPos = checkLine.find('#');
            if (commentPos != std::string::npos) {
                checkLine = checkLine.substr(0, commentPos);
            }
            
            std::smatch match;
            if (std::regex_match(checkLine, match, keywordRegex)) {
                KeywordEntry entry;
                entry.type = match[1].str();
                entry.value = trim(match[2].str());
                entry.filePath = filePath;
                entry.lineNumber = lineNum;
                entries.push_back(entry);
            }
        }
    }
    
    return entries;
}

// Add a new keyword entry to the main config file
inline UpdateResult addKeywordEntry(const std::string& type, const std::string& value) {
    UpdateResult result;
    std::string mainConfig = getConfigPath();
    
    if (mainConfig.empty()) {
        result.error = "Could not determine config path";
        return result;
    }
    
    std::vector<std::string> lines = readLinesFromFile(mainConfig);
    
    // Find a good place to insert - after other keywords of the same type, or at the beginning
    int insertLine = -1;
    for (int i = lines.size() - 1; i >= 0; i--) {
        std::string checkLine = lines[i];
        size_t commentPos = checkLine.find('#');
        if (commentPos != std::string::npos) {
            checkLine = checkLine.substr(0, commentPos);
        }
        checkLine = trim(checkLine);
        
        if (checkLine.find(type) == 0) {
            insertLine = i + 1;
            break;
        }
    }
    
    std::string newLine = type + " = " + value;
    
    if (insertLine >= 0) {
        lines.insert(lines.begin() + insertLine, newLine);
    } else {
        // Insert at beginning (after any initial comments)
        int firstNonComment = 0;
        for (size_t i = 0; i < lines.size(); i++) {
            std::string trimmed = trim(lines[i]);
            if (!trimmed.empty() && trimmed[0] != '#') {
                firstNonComment = i;
                break;
            }
        }
        lines.insert(lines.begin() + firstNonComment, newLine);
    }
    
    if (!writeLinesToFile(mainConfig, lines)) {
        result.error = "Could not write to config file";
        return result;
    }
    
    result.success = true;
    result.filePath = mainConfig;
    return result;
}

// Remove a keyword entry by file path and line number
inline UpdateResult removeKeywordEntry(const std::string& filePath, int lineNumber) {
    UpdateResult result;
    
    if (!std::filesystem::exists(filePath)) {
        result.error = "File does not exist: " + filePath;
        return result;
    }
    
    std::vector<std::string> lines = readLinesFromFile(filePath);
    
    if (lineNumber < 1 || lineNumber > (int)lines.size()) {
        result.error = "Invalid line number";
        return result;
    }
    
    lines.erase(lines.begin() + (lineNumber - 1));
    
    if (!writeLinesToFile(filePath, lines)) {
        result.error = "Could not write to file";
        return result;
    }
    
    result.success = true;
    result.filePath = filePath;
    return result;
}

// Update a keyword entry
inline UpdateResult updateKeywordEntry(const std::string& filePath, int lineNumber, 
                                        const std::string& type, const std::string& value) {
    UpdateResult result;
    
    if (!std::filesystem::exists(filePath)) {
        result.error = "File does not exist: " + filePath;
        return result;
    }
    
    std::vector<std::string> lines = readLinesFromFile(filePath);
    
    if (lineNumber < 1 || lineNumber > (int)lines.size()) {
        result.error = "Invalid line number";
        return result;
    }
    
    // Preserve indentation
    std::string& targetLine = lines[lineNumber - 1];
    size_t indentEnd = targetLine.find_first_not_of(" \t");
    std::string indent = (indentEnd != std::string::npos) ? targetLine.substr(0, indentEnd) : "";
    
    targetLine = indent + type + " = " + value;
    
    if (!writeLinesToFile(filePath, lines)) {
        result.error = "Could not write to file";
        return result;
    }
    
    result.success = true;
    result.wasExisting = true;
    result.filePath = filePath;
    return result;
}

} // namespace ConfigWriter
