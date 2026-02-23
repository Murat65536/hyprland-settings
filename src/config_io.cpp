#include "config_io.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
// Helper to split option path "general:border_size" -> ["general", "border_size"]
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, ':')) {
        if (!item.empty()) {
            parts.push_back(item);
        }
    }
    return parts;
}
}  // namespace

bool ConfigIO::updateOption(const std::string& filePath, const std::string& optionPath, const std::string& value) {
    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Could not open config file for reading: " << filePath << '\n';
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line)) {
        lines.push_back(line);
    }
    inFile.close();

    auto parts = split_path(optionPath);
    if (parts.empty()) {
        return false;
    }

    std::string key = parts.back();
    parts.pop_back(); // Remove key, remaining are sections
    // parts is now ["section", "subsection"]

    // Simply append the option to the end of the file
    std::string indent = "";
    for (const auto& part : parts) {
        lines.push_back(indent + part + " {");
        indent += "    ";
    }
    lines.push_back(indent + key + " = " + value);
    for (size_t i = 0; i < parts.size(); ++i) {
        indent = indent.substr(0, indent.length() - 4);
        lines.push_back(indent + "}");
    }

    // Write the updated content back to the file
    std::ofstream outFile(filePath);
    if (!outFile.is_open()) {
        return false;
    }
    for (const auto& l : lines) {
        outFile << l << '\n';
    }
    return true;
}
