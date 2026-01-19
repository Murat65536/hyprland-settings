#ifndef CONFIG_IO_HPP
#define CONFIG_IO_HPP

#include <string>
#include <vector>

class ConfigIO {
public:
    static bool updateOption(const std::string& filePath, const std::string& optionPath, const std::string& value);

private:
    static std::string getIndent(const std::string& line);
};

#endif // CONFIG_IO_HPP
