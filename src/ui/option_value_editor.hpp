#ifndef UI_OPTION_VALUE_EDITOR_HPP
#define UI_OPTION_VALUE_EDITOR_HPP

#include "ui/item_models.hpp"

#include <gtkmm.h>

#include <functional>
#include <string>

namespace ui {
void setup_option_value_editor(
    const Glib::RefPtr<Gtk::ListItem>& list_item,
    Gtk::ScrolledWindow& content_scroll,
    bool& binding_programmatically,
    const std::function<void(const std::string&, const std::string&)>& send_update,
    const std::function<void(const std::string&, const std::string&)>& send_runtime_update);

void bind_option_value_editor(const Glib::RefPtr<Gtk::ListItem>& list_item,
                              bool& binding_programmatically);
}

#endif
