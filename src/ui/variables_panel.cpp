#include "ui/variables_panel.hpp"

namespace ui {
VariablesPanel::VariablesPanel(
    const std::string& section_path,
    const Glib::RefPtr<Gio::ListStore<ConfigItem>>& section_store,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_column_read,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_column_edit,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_name,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_value) {
    m_root = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    m_root->set_spacing(10);

    auto label = Gtk::make_managed<Gtk::Label>(section_path);
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    m_root->append(*label);

    auto selectionModel = Gtk::SingleSelection::create(section_store);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(setup_column_read);
    factory_name->signal_bind().connect(bind_name);
    auto col_name = Gtk::ColumnViewColumn::create("Option", factory_name);
    col_name->set_fixed_width(280);
    columnView->append_column(col_name);

    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(setup_column_edit);
    factory_value->signal_bind().connect(bind_value);
    auto col_val = Gtk::ColumnViewColumn::create("Value", factory_value);
    col_val->set_expand(true);
    columnView->append_column(col_val);

    m_root->append(*columnView);
}

Gtk::Box* VariablesPanel::widget() const {
    return m_root;
}
}  // namespace ui
