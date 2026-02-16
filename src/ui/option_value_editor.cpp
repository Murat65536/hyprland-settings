#include "ui/option_value_editor.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>

namespace {
std::optional<long long> parse_int_truncate(const std::string& value) {
    try {
        double parsed = std::stod(value);
        return static_cast<long long>(std::trunc(parsed));
    } catch (...) {
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

    auto rangeBox = Gtk::make_managed<Gtk::Fixed>();
    rangeBox->set_size_request(180, 32);
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
    rangeBox->put(*entry, 0, 0);

    auto slider = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL);
    slider->set_hexpand(false);
    slider->set_size_request(90, -1);
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
    rangeBox->put(*slider, 80, 0);

    container->append(*rangeBox);
    list_item->set_child(*container);

    boolButton->signal_clicked().connect([boolButton, list_item, send_update]() {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_valueType == 0) {
            const bool nextValue = (item->m_value != "true");
            item->m_value = nextValue ? "true" : "false";
            boolButton->set_label(item->m_value);
            send_update(item->m_name, item->m_value);
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
            send_update(item->m_name, newValue);
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
        if (item->m_valueType == 1) {
            auto truncated = parse_int_truncate(newVal);
            if (!truncated.has_value()) {
                label->set_text(item->m_value);
                return;
            }
            newVal = std::to_string(*truncated);
            label->set_text(newVal);
        }

        if (newVal != item->m_value) {
            item->m_value = newVal;
            send_update(item->m_name, newVal);
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
        if (item) {
            send_update(item->m_name, item->m_value);
        }
    });
    slider->add_controller(dragGesture);

    auto clickGesture = Gtk::GestureClick::create();
    clickGesture->signal_released().connect([list_item, send_update](int, double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item) {
            send_update(item->m_name, item->m_value);
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
                        send_update(item->m_name, valStr);
                    }
                    entry->set_text(valStr);
                }
            } catch (...) {
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
    auto choiceDropDown = dynamic_cast<Gtk::DropDown*>(boolButton->get_next_sibling());
    auto label = dynamic_cast<Gtk::EditableLabel*>(choiceDropDown->get_next_sibling());
    auto rangeBox = dynamic_cast<Gtk::Fixed*>(label->get_next_sibling());
    if (!boolButton || !choiceDropDown || !label || !rangeBox) return;

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
        auto slider = dynamic_cast<Gtk::Scale*>(first);
        auto entry = dynamic_cast<Gtk::Entry*>(second);
        if (!slider || !entry) {
            slider = dynamic_cast<Gtk::Scale*>(second);
            entry = dynamic_cast<Gtk::Entry*>(first);
        }
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
            entry->set_text(format_range_value(val, item->m_isFloat));
        } catch (...) {
            slider->set_value(item->m_rangeMin);
            entry->set_text(format_range_value(item->m_rangeMin, item->m_isFloat));
        }
        binding_programmatically = false;
    } else {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(false);
        label->set_visible(true);
        rangeBox->set_visible(false);
        label->set_text(item->m_value);
    }
}
}  // namespace ui
