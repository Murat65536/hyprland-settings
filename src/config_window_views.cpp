#include "config_window.hpp"

#include "ui/devices_panel.hpp"
#include "ui/keywords_panel.hpp"
#include "ui/option_name_cell.hpp"
#include "ui/option_value_editor.hpp"
#include "ui/variables_panel.hpp"

void ConfigWindow::create_section_view(const std::string& sectionPath) {
    auto listStore = Gio::ListStore<ConfigItem>::create();
    m_SectionStores[sectionPath] = listStore;

    auto panel = std::make_unique<ui::VariablesPanel>(
        sectionPath,
        listStore,
        sigc::mem_fun(*this, &ConfigWindow::setup_column_read),
        sigc::mem_fun(*this, &ConfigWindow::setup_column_edit),
        sigc::mem_fun(*this, &ConfigWindow::bind_name),
        sigc::mem_fun(*this, &ConfigWindow::bind_value));

    Gtk::Box* sectionBox = panel->widget();
    m_ContentVBox.append(*sectionBox);
    m_SectionWidgets[sectionPath] = sectionBox;
    m_OrderedSections.push_back({sectionPath, sectionBox});
    m_VariablesPanels.push_back(std::move(panel));
}

void ConfigWindow::create_executing_view() {
    const std::string sectionPath = "__executing__";
    m_ExecutingPanel = std::make_unique<ui::KeywordsPanel>(
        ui::KeywordsPanel::Kind::Executing,
        m_ExecutingStore,
        [this](const std::string& type, const std::string& value) { send_keyword_add(type, value); },
        sigc::mem_fun(*this, &ConfigWindow::setup_keyword_type),
        sigc::mem_fun(*this, &ConfigWindow::setup_keyword_value),
        sigc::mem_fun(*this, &ConfigWindow::bind_keyword_type),
        sigc::mem_fun(*this, &ConfigWindow::bind_keyword_value));

    Gtk::Box* mainBox = m_ExecutingPanel->widget();
    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
}

void ConfigWindow::create_env_vars_view() {
    const std::string sectionPath = "__env_vars__";
    m_EnvVarsPanel = std::make_unique<ui::KeywordsPanel>(
        ui::KeywordsPanel::Kind::EnvironmentVariables,
        m_EnvVarStore,
        [this](const std::string& type, const std::string& value) { send_keyword_add(type, value); },
        sigc::mem_fun(*this, &ConfigWindow::setup_keyword_type),
        sigc::mem_fun(*this, &ConfigWindow::setup_keyword_value),
        sigc::mem_fun(*this, &ConfigWindow::bind_keyword_type),
        sigc::mem_fun(*this, &ConfigWindow::bind_keyword_value));

    Gtk::Box* mainBox = m_EnvVarsPanel->widget();
    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
}

void ConfigWindow::create_device_configs_view() {
    const std::string sectionPath = "__device_configs__";
    m_DevicesPanel = std::make_unique<ui::DevicesPanel>(
        m_AvailableDevices,
        m_DeviceConfigStore,
        [this](const std::string& deviceName, const std::string& option, const std::string& value) {
            send_device_config_add(deviceName, option, value);
        },
        sigc::mem_fun(*this, &ConfigWindow::setup_device_name),
        sigc::mem_fun(*this, &ConfigWindow::setup_device_option),
        sigc::mem_fun(*this, &ConfigWindow::setup_device_value),
        sigc::mem_fun(*this, &ConfigWindow::bind_device_name),
        sigc::mem_fun(*this, &ConfigWindow::bind_device_option),
        sigc::mem_fun(*this, &ConfigWindow::bind_device_value));

    Gtk::Box* mainBox = m_DevicesPanel->widget();
    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
}

void ConfigWindow::setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    ui::setup_option_name_cell(list_item, *this);
}

void ConfigWindow::setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    ui::setup_option_value_editor(
        list_item,
        m_ContentScroll,
        m_binding_programmatically,
        [this](const std::string& name, const std::string& value) { send_update(name, value); },
        [this](const std::string& name, const std::string& value) { send_runtime_update(name, value); });
}

void ConfigWindow::bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    ui::bind_option_name_cell(list_item);
}

void ConfigWindow::bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    ui::bind_option_value_editor(list_item, m_binding_programmatically);
}

void ConfigWindow::setup_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    list_item->set_child(*label);
}

void ConfigWindow::setup_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    list_item->set_child(*label);
}

void ConfigWindow::bind_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_type);
    }
}

void ConfigWindow::bind_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_value);
    }
}

void ConfigWindow::setup_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    list_item->set_child(*label);
}

void ConfigWindow::setup_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    list_item->set_child(*label);
}

void ConfigWindow::setup_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    list_item->set_child(*label);
}

void ConfigWindow::bind_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_deviceName);
    }
}

void ConfigWindow::bind_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_option);
    }
}

void ConfigWindow::bind_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_value);
    }
}
