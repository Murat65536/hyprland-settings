#ifndef UI_ITEM_MODELS_HPP
#define UI_ITEM_MODELS_HPP

#include <gtkmm.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace ui {
class ConfigItem : public Glib::Object {
public:
    std::string m_name;
    std::string m_short_name;
    std::string m_value;
    std::string m_lastAppliedValue;
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
                                           int valueType, const std::string& choiceValuesCsv = "",
                                           bool hasRange = false,
                                           double rangeMin = 0.0, double rangeMax = 1.0) {
        return Glib::make_refptr_for_instance<ConfigItem>(
            new ConfigItem(name, value, desc, setByUser, valueType, choiceValuesCsv,
                           hasRange, rangeMin, rangeMax));
    }

protected:
    ConfigItem(const std::string& name, const std::string& value, const std::string& desc,
               bool setByUser, int valueType, const std::string& choiceValuesCsv,
               bool hasRange, double rangeMin, double rangeMax)
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

        auto trim = [](const std::string& s) -> std::string {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])) != 0) {
                ++start;
            }

            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
                --end;
            }

            return s.substr(start, end - start);
        };

        if (m_valueType == 6) {
            std::vector<std::pair<std::string, std::string>> parsedChoices;
            size_t start = 0;
            size_t index = 0;
            while (start <= choiceValuesCsv.size()) {
                const size_t end = choiceValuesCsv.find(',', start);
                const std::string token = (end == std::string::npos)
                    ? choiceValuesCsv.substr(start)
                    : choiceValuesCsv.substr(start, end - start);
                const std::string label = trim(token);
                if (!label.empty()) {
                    parsedChoices.emplace_back(std::to_string(index), label);
                }
                ++index;
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }

            if (!parsedChoices.empty()) {
                m_hasChoices = true;
                m_choices = std::move(parsedChoices);
            }
        }

        if (m_valueType == 6 && m_hasChoices) {
            const auto matchesCurrentValue = [this](const std::pair<std::string, std::string>& choice) {
                return choice.first == m_value;
            };
            if (std::find_if(m_choices.begin(), m_choices.end(), matchesCurrentValue) == m_choices.end()) {
                m_value = "0";
                if (!m_choices.empty()) {
                    m_value = m_choices.front().first;
                }
            }
        }

        m_hasRange = hasRange;
        if (m_hasRange) {
            m_rangeMin = rangeMin;
            m_rangeMax = rangeMax;
            if (m_valueType == 2) {
                m_isFloat = true;
            } else if (m_valueType == 1) {
                m_isFloat = false;
            } else {
                const double minFrac = std::fabs(m_rangeMin - std::round(m_rangeMin));
                const double maxFrac = std::fabs(m_rangeMax - std::round(m_rangeMax));
                m_isFloat = minFrac > 0.0 || maxFrac > 0.0;
            }
        }

        m_lastAppliedValue = m_value;
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
