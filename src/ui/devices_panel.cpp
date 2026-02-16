#include "ui/devices_panel.hpp"

namespace ui {
DevicesPanel::DevicesPanel(
    const std::vector<std::string>& available_devices,
    const std::vector<std::string>& available_options,
    const Glib::RefPtr<Gio::ListStore<DeviceConfigItem>>& device_store,
    const std::function<void(const std::string&, const std::string&, const std::string&)>& on_add_device_config,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_name,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_option,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_device_value,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_name,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_option,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_device_value) {
    m_root = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    m_root->set_spacing(10);

    auto label = Gtk::make_managed<Gtk::Label>("Per-device Input Configs");
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    m_root->append(*label);

    auto addFrame = Gtk::make_managed<Gtk::Frame>("Set Per-Device Config");
    auto addBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    addBox->set_spacing(5);
    addBox->set_margin(10);

    auto deviceCombo = Gtk::make_managed<Gtk::DropDown>();
    auto deviceModel = Gtk::StringList::create({});
    deviceCombo->set_model(deviceModel);

    for (const auto& device : available_devices) {
        deviceModel->append(device);
    }
    if (!available_devices.empty()) {
        deviceCombo->set_selected(0);
    }

    auto optionCombo = Gtk::make_managed<Gtk::DropDown>();
    auto optionModel = Gtk::StringList::create({});
    for (const auto& option : available_options) {
        optionModel->append(option);
    }
    optionCombo->set_model(optionModel);
    if (!available_options.empty()) {
        optionCombo->set_selected(0);
    }

    auto valueEntry = Gtk::make_managed<Gtk::Entry>();
    valueEntry->set_placeholder_text("Value...");
    valueEntry->set_hexpand(true);

    auto addButton = Gtk::make_managed<Gtk::Button>("Apply");
    addButton->signal_clicked().connect([device_store, on_add_device_config, deviceCombo, deviceModel, optionCombo, optionModel, valueEntry]() {
        auto deviceIdx = deviceCombo->get_selected();
        auto optionIdx = optionCombo->get_selected();
        if (deviceIdx == GTK_INVALID_LIST_POSITION || optionIdx == GTK_INVALID_LIST_POSITION) {
            return;
        }

        const std::string deviceName = deviceModel->get_string(deviceIdx);
        const std::string option = optionModel->get_string(optionIdx);
        const std::string value = valueEntry->get_text();
        if (value.empty()) {
            return;
        }

        on_add_device_config(deviceName, option, value);
        device_store->append(DeviceConfigItem::create(deviceName, option, value));
        valueEntry->set_text("");
    });

    addBox->append(*deviceCombo);
    addBox->append(*optionCombo);
    addBox->append(*valueEntry);
    addBox->append(*addButton);
    addFrame->set_child(*addBox);
    m_root->append(*addFrame);

    auto selectionModel = Gtk::SingleSelection::create(device_store);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(setup_device_name);
    factory_name->signal_bind().connect(bind_device_name);
    auto col_name = Gtk::ColumnViewColumn::create("Device", factory_name);
    col_name->set_fixed_width(200);
    columnView->append_column(col_name);

    auto factory_option = Gtk::SignalListItemFactory::create();
    factory_option->signal_setup().connect(setup_device_option);
    factory_option->signal_bind().connect(bind_device_option);
    auto col_option = Gtk::ColumnViewColumn::create("Option", factory_option);
    col_option->set_fixed_width(150);
    columnView->append_column(col_option);

    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(setup_device_value);
    factory_value->signal_bind().connect(bind_device_value);
    auto col_value = Gtk::ColumnViewColumn::create("Value", factory_value);
    col_value->set_expand(true);
    columnView->append_column(col_value);

    m_root->append(*columnView);
}

Gtk::Box* DevicesPanel::widget() const {
    return m_root;
}
}  // namespace ui
