#ifndef UI_ITEM_MODELS_HPP
#define UI_ITEM_MODELS_HPP

#include <gtkmm.h>

#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace ui {
class ConfigItem : public Glib::Object {
public:
    std::string m_name;
    std::string m_short_name;
    std::string m_value;
    std::string m_desc;
    bool m_setByUser = false;
    int m_valueType = -1;
    bool m_hasChoices = false;
    std::vector<std::pair<std::string, std::string>> m_choices;

    bool m_hasRange = false;
    double m_rangeMin = 0.0;
    double m_rangeMax = 1.0;
    bool m_isFloat = false;

    static Glib::RefPtr<ConfigItem> create(const std::string& name, const std::string& value,
                                           const std::string& desc, bool setByUser,
                                           int valueType) {
        return Glib::make_refptr_for_instance<ConfigItem>(
            new ConfigItem(name, value, desc, setByUser, valueType));
    }

protected:
    ConfigItem(const std::string& name, const std::string& value, const std::string& desc,
               bool setByUser, int valueType)
        : m_name(name),
          m_value(value),
          m_desc(desc),
          m_setByUser(setByUser),
          m_valueType(valueType) {
        size_t pos = name.rfind(':');
        if (pos != std::string::npos) {
            m_short_name = name.substr(pos + 1);
        } else {
            m_short_name = name;
        }

        if (m_valueType == 0) {
            m_value = (m_value == "true") ? "true" : "false";
        }

        auto trim = [](std::string s) {
            s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
            return s;
        };

        std::regex choiceRegex(R"((-?\d+)\s*-\s*([^,;\n]+))");
        std::vector<std::pair<std::string, std::string>> parsedChoices;
        std::vector<std::smatch> choiceMatches;
        for (std::sregex_iterator it(m_desc.begin(), m_desc.end(), choiceRegex), end; it != end; ++it) {
            const auto& match = *it;
            std::string choiceValue = match[1].str();
            std::string choiceLabel = trim(match[2].str());

            if (!std::regex_search(choiceLabel, std::regex(R"([A-Za-z])"))) {
                continue;
            }

            parsedChoices.emplace_back(choiceValue, choiceLabel);
            choiceMatches.push_back(match);
        }

        if (parsedChoices.size() >= 2 && !choiceMatches.empty()) {
            const auto& first = choiceMatches.front();
            const auto& last = choiceMatches.back();
            size_t firstPos = static_cast<size_t>(first.position());
            size_t tailPos = static_cast<size_t>(last.position() + last.length());

            std::string tail = m_desc.substr(tailPos);
            if (std::regex_match(tail, std::regex(R"(\s*[.)]?\s*)"))) {
                m_hasChoices = true;
                m_choices = parsedChoices;

                m_desc = trim(m_desc.substr(0, firstPos));
                m_desc = std::regex_replace(m_desc, std::regex(R"([:;,.\-]\s*$)"), "");
            }
        }

        std::regex rangeRegex(R"(\[\s*(-?\d+\.?\d*)\s*(?:-|\.\.|to|,)\s*(-?\d+\.?\d*)\s*\])");
        std::smatch match;
        if (std::regex_search(m_desc, match, rangeRegex)) {
            try {
                m_rangeMin = std::stod(match[1].str());
                m_rangeMax = std::stod(match[2].str());
                m_hasRange = true;
                if (m_valueType == 2) {
                    m_isFloat = true;
                } else if (m_valueType == 1) {
                    m_isFloat = false;
                } else {
                    m_isFloat = (match[1].str().find('.') != std::string::npos) ||
                                (match[2].str().find('.') != std::string::npos);
                }
                m_desc = std::regex_replace(m_desc, rangeRegex, "");
                m_desc = std::regex_replace(m_desc, std::regex(R"(^\s+|\s+$)"), "");
            } catch (...) {
                m_hasRange = false;
            }
        }
    }
};

class KeywordItem : public Glib::Object {
public:
    std::string m_type;
    std::string m_value;

    static Glib::RefPtr<KeywordItem> create(const std::string& type, const std::string& value) {
        return Glib::make_refptr_for_instance<KeywordItem>(new KeywordItem(type, value));
    }

protected:
    KeywordItem(const std::string& type, const std::string& value)
        : m_type(type), m_value(value) {}
};

class DeviceConfigItem : public Glib::Object {
public:
    std::string m_deviceName;
    std::string m_option;
    std::string m_value;

    static Glib::RefPtr<DeviceConfigItem> create(const std::string& deviceName,
                                                 const std::string& option,
                                                 const std::string& value) {
        return Glib::make_refptr_for_instance<DeviceConfigItem>(
            new DeviceConfigItem(deviceName, option, value));
    }

protected:
    DeviceConfigItem(const std::string& deviceName, const std::string& option,
                     const std::string& value)
        : m_deviceName(deviceName), m_option(option), m_value(value) {}
};
}  // namespace ui

#endif
