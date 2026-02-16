#ifndef UI_VARIABLES_PANEL_HPP
#define UI_VARIABLES_PANEL_HPP

#include "ui/item_models.hpp"

#include <gtkmm.h>

#include <string>

namespace ui {
class VariablesPanel {
public:
    VariablesPanel(const std::string& section_path,
                   const Glib::RefPtr<Gio::ListStore<ConfigItem>>& section_store,
                   const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_column_read,
                   const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_column_edit,
                   const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_name,
                   const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_value);

    Gtk::Box* widget() const;

private:
    Gtk::Box* m_root = nullptr;
};
}  // namespace ui

#endif
