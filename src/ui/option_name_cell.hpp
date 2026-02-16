#ifndef UI_OPTION_NAME_CELL_HPP
#define UI_OPTION_NAME_CELL_HPP

#include "ui/item_models.hpp"

#include <gtkmm.h>

namespace ui {
void setup_option_name_cell(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Window& parent_window);
void bind_option_name_cell(const Glib::RefPtr<Gtk::ListItem>& list_item);
}

#endif
