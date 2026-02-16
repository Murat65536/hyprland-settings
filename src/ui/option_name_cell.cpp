#include "ui/option_name_cell.hpp"

namespace ui {
void setup_option_name_cell(const Glib::RefPtr<Gtk::ListItem>& list_item, Gtk::Window& parent_window) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_ellipsize(Pango::EllipsizeMode::END);

    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_PRIMARY);
    gesture->signal_released().connect([&parent_window, list_item](int, double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (!item || item->m_desc.empty()) {
            return;
        }

        auto dialog = new Gtk::Dialog();
        dialog->set_transient_for(parent_window);
        dialog->set_modal(true);
        dialog->set_title("Description");

        auto content_area = dialog->get_content_area();
        content_area->set_margin(20);
        content_area->set_spacing(10);

        auto titleLabel = Gtk::make_managed<Gtk::Label>(item->m_short_name);
        titleLabel->add_css_class("dialog-title");
        titleLabel->set_halign(Gtk::Align::START);
        content_area->append(*titleLabel);

        auto descLabel = Gtk::make_managed<Gtk::Label>(item->m_desc);
        descLabel->set_wrap(true);
        descLabel->set_max_width_chars(60);
        descLabel->set_halign(Gtk::Align::START);
        content_area->append(*descLabel);

        dialog->add_button("Close", Gtk::ResponseType::CLOSE);
        dialog->signal_response().connect([dialog](int) {
            dialog->hide();
            Glib::signal_idle().connect_once([dialog]() {
                delete dialog;
            });
        });
        dialog->show();
    });
    label->add_controller(gesture);

    label->set_cursor(Gdk::Cursor::create("pointer"));
    list_item->set_child(*label);
}

void bind_option_name_cell(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_short_name);
    }
}
}  // namespace ui
