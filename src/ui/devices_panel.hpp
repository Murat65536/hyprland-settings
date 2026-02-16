#ifndef UI_DEVICES_PANEL_HPP
#define UI_DEVICES_PANEL_HPP

#include "ui/item_models.hpp"

#include <gtkmm.h>

#include <functional>
#include <string>
#include <vector>

namespace ui {
class DevicesPanel {
public:
    DevicesPanel(
        const std::vector<std::string>& available_devices,
        const Glib::RefPtr<Gio::ListStore<DeviceConfigItem>>& device_store,
        const std::function<void(const std::string&, const std::string&, const std::string&)>& on_add_device_config,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_name,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_option,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_value,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_name,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_option,
        const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_value);

    Gtk::Box* widget() const;

private:
    Gtk::Box* m_root = nullptr;
};
}  // namespace ui

#endif
