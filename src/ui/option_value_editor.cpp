#include "ui/option_value_editor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {
std::string trim_copy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

std::string collapse_whitespace(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    bool previous_was_space = false;
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!previous_was_space) {
                out.push_back(' ');
                previous_was_space = true;
            }
        } else {
            out.push_back(c);
            previous_was_space = false;
        }
    }
    return trim_copy(out);
}

bool is_digits_only(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

bool is_hex_digits_only(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
}

std::string to_lower_ascii(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::optional<double> parse_double_strict(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    try {
        size_t consumed = 0;
        double parsed = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return std::nullopt;
        }
        return parsed;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<long long> parse_int_truncate(const std::string& value) {
    try {
        double parsed = std::stod(value);
        return static_cast<long long>(std::trunc(parsed));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::string format_range_value(double val, bool is_float) {
    if (!is_float) {
        return std::to_string(static_cast<long long>(std::trunc(val)));
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << val;
    std::string valStr = ss.str();
    if (!valStr.empty()) {
        valStr.erase(valStr.find_last_not_of('0') + 1, std::string::npos);
        if (!valStr.empty() && valStr.back() == '.') {
            valStr.pop_back();
        }
    }
    return valStr;
}

std::optional<std::string> normalize_color_value(const std::string& input) {
    std::string value = trim_copy(input);
    if (value.empty()) {
        return std::string();
    }

    if (is_digits_only(value)) {
        return value;
    }

    if (value.size() > 2 && (value[0] == '0') && (value[1] == 'x' || value[1] == 'X')) {
        const std::string hex_part = value.substr(2);
        if (!is_hex_digits_only(hex_part)) {
            return std::nullopt;
        }
        return std::string("0x") + to_lower_ascii(hex_part);
    }

    if (!value.empty() && value[0] == '#') {
        const std::string hex_part = value.substr(1);
        if ((hex_part.size() != 6 && hex_part.size() != 8) || !is_hex_digits_only(hex_part)) {
            return std::nullopt;
        }
        return std::string("0x") + to_lower_ascii(hex_part);
    }

    if ((value.size() == 6 || value.size() == 8) && is_hex_digits_only(value)) {
        return std::string("0x") + to_lower_ascii(value);
    }

    return std::nullopt;
}

std::optional<std::string> normalize_gradient_value(const std::string& input) {
    std::string value = collapse_whitespace(input);
    if (value.empty()) {
        return std::nullopt;
    }
    return value;
}

bool has_fractional_component(double value) {
    return std::fabs(value - std::round(value)) > 0.000001;
}

std::optional<std::pair<double, double>> parse_vector_value(const std::string& input) {
    std::string value = input;
    std::replace(value.begin(), value.end(), ',', ' ');

    std::stringstream ss(value);
    double x = 0.0;
    double y = 0.0;
    if (!(ss >> x >> y)) {
        return std::nullopt;
    }

    std::string tail;
    if (ss >> tail) {
        return std::nullopt;
    }

    return std::make_pair(x, y);
}

std::string format_scalar(double value, bool as_float) {
    return format_range_value(value, as_float || has_fractional_component(value));
}

std::string format_vector_value(double x, double y, bool as_float) {
    return format_scalar(x, as_float) + ", " + format_scalar(y, as_float);
}
}

namespace ui {
void setup_option_value_editor(
    const Glib::RefPtr<Gtk::ListItem>& list_item,
    Gtk::ScrolledWindow& content_scroll,
    bool& binding_programmatically,
    const std::function<void(const std::string&, const std::string&)>& send_update,
    const std::function<void(const std::string&, const std::string&)>& send_runtime_update) {
    auto container = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    container->set_spacing(10);

    auto boolButton = Gtk::make_managed<Gtk::Button>();
    boolButton->set_halign(Gtk::Align::START);
    boolButton->set_focus_on_click(false);
    container->append(*boolButton);

    auto choiceDropDown = Gtk::make_managed<Gtk::DropDown>();
    choiceDropDown->set_halign(Gtk::Align::START);
    container->append(*choiceDropDown);

    auto label = Gtk::make_managed<Gtk::EditableLabel>();
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    container->append(*label);

    auto rangeBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    rangeBox->set_spacing(10);
    rangeBox->set_hexpand(false);
    rangeBox->set_halign(Gtk::Align::START);
    rangeBox->set_valign(Gtk::Align::CENTER);
    rangeBox->add_css_class("range-editor");

    auto entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_width_chars(6);
    entry->set_size_request(70, -1);
    entry->set_halign(Gtk::Align::START);
    entry->set_valign(Gtk::Align::CENTER);
    entry->add_css_class("range-entry");
    rangeBox->append(*entry);

    auto slider = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL);
    slider->set_hexpand(true);
    slider->set_size_request(120, -1);
    slider->set_halign(Gtk::Align::START);
    slider->set_valign(Gtk::Align::CENTER);
    slider->set_draw_value(false);
    slider->add_css_class("range-slider");

    auto sliderScrollBlocker = Gtk::EventControllerScroll::create();
    sliderScrollBlocker->set_flags(
        Gtk::EventControllerScroll::Flags::VERTICAL |
        Gtk::EventControllerScroll::Flags::HORIZONTAL |
        Gtk::EventControllerScroll::Flags::DISCRETE);
    sliderScrollBlocker->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    sliderScrollBlocker->signal_scroll().connect(
        [&content_scroll](double dx, double dy) {
            auto vadj = content_scroll.get_vadjustment();
            if (vadj && dy != 0.0) {
                const double step = vadj->get_step_increment() > 0.0 ? vadj->get_step_increment() : 40.0;
                const double lower = vadj->get_lower();
                const double upper = vadj->get_upper() - vadj->get_page_size();
                const double target = std::clamp(vadj->get_value() + (dy * step), lower, upper);
                vadj->set_value(target);
            }

            auto hadj = content_scroll.get_hadjustment();
            if (hadj && dx != 0.0) {
                const double step = hadj->get_step_increment() > 0.0 ? hadj->get_step_increment() : 40.0;
                const double lower = hadj->get_lower();
                const double upper = hadj->get_upper() - hadj->get_page_size();
                const double target = std::clamp(hadj->get_value() + (dx * step), lower, upper);
                hadj->set_value(target);
            }

            return true;
        },
        false);
    slider->add_controller(sliderScrollBlocker);
    rangeBox->append(*slider);

    container->append(*rangeBox);
    list_item->set_child(*container);

    boolButton->signal_clicked().connect([boolButton, list_item, send_update]() {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_valueType == 0) {
            const bool nextValue = (item->m_value != "true");
            item->m_value = nextValue ? "true" : "false";
            boolButton->set_label(item->m_value);
            if (item->m_value != item->m_lastAppliedValue) {
                send_update(item->m_name, item->m_value);
                item->m_lastAppliedValue = item->m_value;
            }
        }
    });

    choiceDropDown->property_selected().signal_changed().connect([choiceDropDown, list_item, &binding_programmatically, send_update]() {
        if (binding_programmatically) return;

        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (!item || !item->m_hasChoices) return;

        auto selected = choiceDropDown->get_selected();
        if (selected == GTK_INVALID_LIST_POSITION || selected >= item->m_choices.size()) return;

        const std::string newValue = item->m_choices[selected].first;
        if (newValue != item->m_value) {
            item->m_value = newValue;
            if (item->m_value != item->m_lastAppliedValue) {
                send_update(item->m_name, item->m_value);
                item->m_lastAppliedValue = item->m_value;
            }
        }
    });

    label->property_editing().signal_changed().connect([label, list_item, send_update]() {
        if (label->get_editing()) {
            return;
        }

        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (!item || item->m_hasRange || item->m_valueType == 0 || item->m_hasChoices) {
            return;
        }

        std::string newVal = label->get_text();
        switch (item->m_valueType) {
        case 1: {
            auto truncated = parse_int_truncate(newVal);
            if (!truncated.has_value()) {
                label->set_text(item->m_value);
                return;
            }
            newVal = std::to_string(*truncated);
            label->set_text(newVal);
            break;
        }
        case 2: {
            auto parsed = parse_double_strict(newVal);
            if (!parsed.has_value()) {
                label->set_text(item->m_value);
                return;
            }
            newVal = format_scalar(*parsed, true);
            label->set_text(newVal);
            break;
        }
        case 3: {
            newVal = collapse_whitespace(newVal);
            label->set_text(newVal);
            break;
        }
        case 4: {
            if (newVal == "[[EMPTY]]") {
                newVal.clear();
                label->set_text(newVal);
            }
            break;
        }
        case 5: {
            auto normalized = normalize_color_value(newVal);
            if (!normalized.has_value()) {
                label->set_text(item->m_value);
                return;
            }
            newVal = *normalized;
            label->set_text(newVal);
            break;
        }
        case 7: {
            auto normalized = normalize_gradient_value(newVal);
            if (!normalized.has_value()) {
                label->set_text(item->m_value);
                return;
            }
            newVal = *normalized;
            label->set_text(newVal);
            break;
        }
        case 8: {
            auto vector = parse_vector_value(newVal);
            if (!vector.has_value()) {
                label->set_text(item->m_value);
                return;
            }

            double x = vector->first;
            double y = vector->second;
            if (item->m_hasVectorRange) {
                x = std::clamp(x, item->m_vectorMinX, item->m_vectorMaxX);
                y = std::clamp(y, item->m_vectorMinY, item->m_vectorMaxY);
            }

            const bool as_float = has_fractional_component(x) || has_fractional_component(y) ||
                                  has_fractional_component(item->m_vectorMinX) ||
                                  has_fractional_component(item->m_vectorMinY) ||
                                  has_fractional_component(item->m_vectorMaxX) ||
                                  has_fractional_component(item->m_vectorMaxY);
            newVal = format_vector_value(x, y, as_float);
            label->set_text(newVal);
            break;
        }
        default:
            break;
        }

        if (newVal != item->m_value) {
            item->m_value = newVal;
            if (item->m_value != item->m_lastAppliedValue) {
                send_update(item->m_name, item->m_value);
                item->m_lastAppliedValue = item->m_value;
            }
        }
    });

    slider->signal_value_changed().connect([slider, entry, list_item, &binding_programmatically, send_runtime_update]() {
        if (binding_programmatically) return;

        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange) {
            const std::string valStr = format_range_value(slider->get_value(), item->m_isFloat);
            if (valStr != item->m_value) {
                entry->set_text(valStr);
                item->m_value = valStr;
                send_runtime_update(item->m_name, valStr);
            }
        }
    });

    auto dragGesture = Gtk::GestureDrag::create();
    dragGesture->signal_drag_end().connect([list_item, send_update](double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange && item->m_value != item->m_lastAppliedValue) {
            send_update(item->m_name, item->m_value);
            item->m_lastAppliedValue = item->m_value;
        }
    });
    slider->add_controller(dragGesture);

    auto clickGesture = Gtk::GestureClick::create();
    clickGesture->signal_released().connect([list_item, send_update](int, double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange && item->m_value != item->m_lastAppliedValue) {
            send_update(item->m_name, item->m_value);
            item->m_lastAppliedValue = item->m_value;
        }
    });
    slider->add_controller(clickGesture);

    entry->signal_activate().connect([slider, entry, list_item, send_update]() {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange) {
            try {
                double val = std::stod(entry->get_text());
                if (val < item->m_rangeMin) val = item->m_rangeMin;
                if (val > item->m_rangeMax) val = item->m_rangeMax;

                slider->set_value(val);
                if (slider->get_value() == val) {
                    const std::string valStr = format_range_value(val, item->m_isFloat);
                    if (valStr != item->m_value) {
                        item->m_value = valStr;
                        if (item->m_value != item->m_lastAppliedValue) {
                            send_update(item->m_name, item->m_value);
                            item->m_lastAppliedValue = item->m_value;
                        }
                    }
                    entry->set_text(valStr);
                }
            } catch (const std::exception&) {
                entry->set_text(item->m_value);
            }
        }
    });

}

void bind_option_value_editor(const Glib::RefPtr<Gtk::ListItem>& list_item,
                              bool& binding_programmatically) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto container = dynamic_cast<Gtk::Box*>(list_item->get_child());
    if (!item || !container) return;

    auto boolButton = dynamic_cast<Gtk::Button*>(container->get_first_child());
    if (!boolButton) return;
    auto choiceDropDown = dynamic_cast<Gtk::DropDown*>(boolButton->get_next_sibling());
    if (!choiceDropDown) return;
    auto label = dynamic_cast<Gtk::EditableLabel*>(choiceDropDown->get_next_sibling());
    if (!label) return;
    auto rangeBox = dynamic_cast<Gtk::Box*>(label->get_next_sibling());
    if (!rangeBox) return;

    if (item->m_valueType == 0) {
        boolButton->set_visible(true);
        choiceDropDown->set_visible(false);
        label->set_visible(false);
        rangeBox->set_visible(false);

        item->m_value = (item->m_value == "true") ? "true" : "false";
        boolButton->set_label(item->m_value);
    } else if (item->m_hasChoices) {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(true);
        label->set_visible(false);
        rangeBox->set_visible(false);

        auto model = Gtk::StringList::create({});
        guint selected = 0;
        for (guint i = 0; i < item->m_choices.size(); ++i) {
            model->append(item->m_choices[i].second);
            if (item->m_choices[i].first == item->m_value) {
                selected = i;
            }
        }
        binding_programmatically = true;
        choiceDropDown->set_model(model);
        choiceDropDown->set_selected(selected);
        binding_programmatically = false;
    } else if (item->m_hasRange) {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(false);
        label->set_visible(false);
        rangeBox->set_visible(true);

        auto first = rangeBox->get_first_child();
        auto second = first ? first->get_next_sibling() : nullptr;
        auto entry = dynamic_cast<Gtk::Entry*>(first);
        auto slider = dynamic_cast<Gtk::Scale*>(second);
        if (!slider || !entry) return;

        slider->set_range(item->m_rangeMin, item->m_rangeMax);
        if (item->m_isFloat) {
            slider->set_increments(0.01, 0.1);
            slider->set_digits(2);
        } else {
            slider->set_increments(1.0, 10.0);
            slider->set_digits(0);
        }

        binding_programmatically = true;
        try {
            double val = std::stod(item->m_value);
            slider->set_value(val);
            const std::string formatted = format_range_value(val, item->m_isFloat);
            entry->set_text(formatted);
            item->m_value = formatted;
            item->m_lastAppliedValue = formatted;
        } catch (const std::exception&) {
            slider->set_value(item->m_rangeMin);
            const std::string formatted = format_range_value(item->m_rangeMin, item->m_isFloat);
            entry->set_text(formatted);
            item->m_value = formatted;
            item->m_lastAppliedValue = formatted;
        }
        binding_programmatically = false;
    } else {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(false);
        label->set_visible(true);
        rangeBox->set_visible(false);

        std::string displayValue = item->m_value;
        if (item->m_valueType == 3 || item->m_valueType == 4) {
            if (displayValue == "[[EMPTY]]") {
                displayValue.clear();
                item->m_value.clear();
            }
        } else if (item->m_valueType == 5) {
            auto normalized = normalize_color_value(displayValue);
            if (normalized.has_value()) {
                displayValue = *normalized;
                item->m_value = displayValue;
            }
        } else if (item->m_valueType == 7) {
            auto normalized = normalize_gradient_value(displayValue);
            if (normalized.has_value()) {
                displayValue = *normalized;
                item->m_value = displayValue;
            }
        } else if (item->m_valueType == 8) {
            auto vector = parse_vector_value(displayValue);
            if (vector.has_value()) {
                double x = vector->first;
                double y = vector->second;
                if (item->m_hasVectorRange) {
                    x = std::clamp(x, item->m_vectorMinX, item->m_vectorMaxX);
                    y = std::clamp(y, item->m_vectorMinY, item->m_vectorMaxY);
                }

                const bool as_float = has_fractional_component(x) || has_fractional_component(y) ||
                                      has_fractional_component(item->m_vectorMinX) ||
                                      has_fractional_component(item->m_vectorMinY) ||
                                      has_fractional_component(item->m_vectorMaxX) ||
                                      has_fractional_component(item->m_vectorMaxY);
                displayValue = format_vector_value(x, y, as_float);
                item->m_value = displayValue;
            }
        }

        label->set_text(displayValue);
    }
}
}  // namespace ui
