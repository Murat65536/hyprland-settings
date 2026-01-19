#include "config_io.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

// Helper to trim whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

std::string ConfigIO::getIndent(const std::string& line) {
    size_t i = 0;
    while (i < line.length() && (line[i] == ' ' || line[i] == '\t')) {
        i++;
    }
    return line.substr(0, i);
}

// Helper to split option path "general:border_size" -> ["general", "border_size"]
static std::vector<std::string> splitPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, ':')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

bool ConfigIO::updateOption(const std::string& filePath, const std::string& optionPath, const std::string& value) {
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Could not open config file for reading: " << filePath << std::endl;
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    auto parts = splitPath(optionPath);
    if (parts.empty()) return false;

    std::string key = parts.back();
    parts.pop_back(); // Remove key, remaining are sections
    // parts is now ["section", "subsection"]

    int depth = 0;
    std::vector<std::string> currentSectionStack;
    
    bool found = false;

    // First pass: try to find the existing key
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string rawLine = lines[i];
        std::string trimmed = trim(rawLine);
        
        // Remove comments
        size_t commentPos = trimmed.find('#');
        if (commentPos != std::string::npos) {
            trimmed = trimmed.substr(0, commentPos);
            trimmed = trim(trimmed);
        }

        if (trimmed.empty()) continue;

        if (trimmed.back() == '{') {
            std::string sectionName = trim(trimmed.substr(0, trimmed.length() - 1));
            currentSectionStack.push_back(sectionName);
            depth++;
        } else if (trimmed == "}") {
            if (!currentSectionStack.empty()) currentSectionStack.pop_back();
            depth--;
        } else {
            // Check if this is the key we are looking for
            // Format: key = value
            size_t eqPos = trimmed.find('=');
            if (eqPos != std::string::npos) {
                std::string currentKey = trim(trimmed.substr(0, eqPos));
                if (currentKey == key) {
                    // Check if we are in the correct section
                    if (currentSectionStack == parts) {
                        // Found it! Update line.
                        std::string indent = getIndent(rawLine);
                        lines[i] = indent + key + " = " + value;
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        // We need to insert it.
        // We need to find where the section chain ends or create it.
        
        // Reset state
        currentSectionStack.clear();
        depth = 0;

        // Try to find the deepest matching existing section
        int insertionPoint = -1;
        std::string currentIndent = "";

        // If parts is empty (root), append to end of file
        if (parts.empty()) {
            lines.push_back(key + " = " + value);
            found = true;
        } else {
            // Re-scan to find sections
            for (size_t i = 0; i < lines.size(); ++i) {
                std::string rawLine = lines[i];
                std::string trimmed = trim(rawLine);
                if (trimmed.find('#') == 0) continue; 

                if (!trimmed.empty() && trimmed.back() == '{') {
                    std::string sectionName = trim(trimmed.substr(0, trimmed.length() - 1));
                    
                    // Check if this section matches the next needed section in parts
                    if (currentSectionStack.size() < parts.size() && 
                        sectionName == parts[currentSectionStack.size()]) {
                        
                        currentSectionStack.push_back(sectionName);
                        // We are now inside the matching path
                        
                        if (currentSectionStack.size() == parts.size()) {
                            // We are inside the full target section
                            // Scan forward to find closing brace
                            // But we need to handle nested braces inside this section
                            int innerDepth = 1;
                            size_t j = i + 1;
                            for (; j < lines.size(); ++j) {
                                std::string t = trim(lines[j]);
                                if (t.empty()) continue;
                                if (t.back() == '{') innerDepth++;
                                else if (t == "}") {
                                    innerDepth--;
                                    if (innerDepth == 0) {
                                        // This is the closing brace
                                        insertionPoint = j;
                                        // Determine indent from the closing brace line or previous line
                                        currentIndent = getIndent(lines[j]) + "    "; 
                                        break;
                                    }
                                }
                            }
                            if (insertionPoint != -1) break;
                        }
                    } else {
                        // Enter a non-matching section
                        // Skip it? No, we might be inside a parent that matched
                        // But if name doesn't match parts[idx], we ignore it's content for finding path
                         // Actually, handling this correctly requires a full tree parser.
                         // But we just need to find ONE valid place.
                    }
                }
            }
            
            if (insertionPoint != -1) {
                lines.insert(lines.begin() + insertionPoint, currentIndent + key + " = " + value);
                found = true;
            } else {
                // Section doesn't exist. We need to create it.
                // For simplicity, just append to end of file?
                // Or try to put it at the end.
                
                // Construct the block
                std::string block;
                std::string indent = "";
                for (const auto& part : parts) {
                    block += indent + part + " {\n";
                    indent += "    ";
                }
                block += indent + key + " = " + value + "\n";
                for (size_t k = 0; k < parts.size(); ++k) {
                    indent = indent.substr(0, indent.length() - 4);
                    block += indent + "}\n";
                }
                
                if (!lines.empty() && !lines.back().empty()) lines.push_back("");
                lines.push_back(block);
                found = true;
            }
        }
    }

    if (found) {
        std::ofstream outFile(filePath);
        if (!outFile.is_open()) return false;
        for (const auto& l : lines) {
            outFile << l << "\n";
        }
        return true;
    }

    return false;
}
