#include <gtkmm.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <set>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "../ipc_common.hpp"

// Helper to split a string by delimiter
std::vector<std::string> split_path(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}

// Tree structure for nested sections
struct SectionNode {
    std::string name;
    std::string full_path;
    std::map<std::string, std::unique_ptr<SectionNode>> children;
    
    SectionNode(const std::string& n = "", const std::string& fp = "") : name(n), full_path(fp) {}
};

class ConfigWindow : public Gtk::Window
{
public:
    ConfigWindow();
    virtual ~ConfigWindow();

protected:
    void on_button_refresh();

    Gtk::HeaderBar m_HeaderBar;
    Gtk::Box m_MainVBox;
    Gtk::Box m_HBox;
    Gtk::TreeView m_TreeView;
    Gtk::Stack m_Stack;
    Gtk::Button m_Button_Refresh;

    // TreeModel for sidebar
    class SectionColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        SectionColumns() { add(m_col_name); add(m_col_full_path); }
        Gtk::TreeModelColumn<Glib::ustring> m_col_name;
        Gtk::TreeModelColumn<Glib::ustring> m_col_full_path;
    };
    SectionColumns m_SectionColumns;
    Glib::RefPtr<Gtk::TreeStore> m_SectionTreeStore;

    // Config item for the option list
    class ConfigItem : public Glib::Object {
    public:
        std::string m_name;
        std::string m_short_name;
        std::string m_value;
        std::string m_desc;
        bool m_setByUser = false;

        static Glib::RefPtr<ConfigItem> create(const std::string& name, const std::string& value, const std::string& desc, bool setByUser) {
            return Glib::make_refptr_for_instance<ConfigItem>(new ConfigItem(name, value, desc, setByUser));
        }

    protected:
        ConfigItem(const std::string& name, const std::string& value, const std::string& desc, bool setByUser) 
            : m_name(name), m_value(value), m_desc(desc), m_setByUser(setByUser) {
            // Short name is the last part after the last ':'
            size_t pos = name.rfind(':');
            if (pos != std::string::npos) {
                m_short_name = name.substr(pos + 1);
            } else {
                m_short_name = name;
            }
        }
    };

    // Keyword item for the keywords list
    class KeywordItem : public Glib::Object {
    public:
        std::string m_type;      // exec-once, execr, etc.
        std::string m_value;     // The command
        std::string m_filePath;  // Config file location
        int m_lineNumber;        // Line in the file

        static Glib::RefPtr<KeywordItem> create(const std::string& type, const std::string& value,
                                                  const std::string& filePath, int lineNumber) {
            return Glib::make_refptr_for_instance<KeywordItem>(new KeywordItem(type, value, filePath, lineNumber));
        }

    protected:
        KeywordItem(const std::string& type, const std::string& value,
                    const std::string& filePath, int lineNumber)
            : m_type(type), m_value(value), m_filePath(filePath), m_lineNumber(lineNumber) {}
    };

    // Device config item for per-device input configs
    class DeviceConfigItem : public Glib::Object {
    public:
        std::string m_deviceName;  // Device name from hyprctl
        std::string m_option;      // Config option (sensitivity, etc.)
        std::string m_value;       // The value
        std::string m_filePath;    // Config file location
        int m_startLine;           // Line where device section starts
        int m_optionLine;          // Line where this option is

        static Glib::RefPtr<DeviceConfigItem> create(const std::string& deviceName, const std::string& option,
                                                      const std::string& value, const std::string& filePath,
                                                      int startLine, int optionLine) {
            return Glib::make_refptr_for_instance<DeviceConfigItem>(
                new DeviceConfigItem(deviceName, option, value, filePath, startLine, optionLine));
        }

    protected:
        DeviceConfigItem(const std::string& deviceName, const std::string& option,
                         const std::string& value, const std::string& filePath,
                         int startLine, int optionLine)
            : m_deviceName(deviceName), m_option(option), m_value(value),
              m_filePath(filePath), m_startLine(startLine), m_optionLine(optionLine) {}
    };

    std::map<std::string, Glib::RefPtr<Gio::ListStore<ConfigItem>>> m_SectionStores;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_KeywordStore;
    Glib::RefPtr<Gio::ListStore<DeviceConfigItem>> m_DeviceConfigStore;
    std::vector<std::string> m_AvailableDevices;
    SectionNode m_RootSection;
    std::map<std::string, Gtk::TreeModel::iterator> m_SectionIters;

    void setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    // Keyword-specific methods
    void setup_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_keyword_actions(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_actions(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    // Device config methods
    void setup_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_actions(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_actions(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    void load_data();
    void load_keywords();
    void load_device_configs();
    void load_available_devices();
    void send_update(const std::string& name, const std::string& value);
    void send_keyword_add(const std::string& type, const std::string& value);
    void send_keyword_remove(const std::string& filePath, int lineNumber);
    void send_keyword_update(const std::string& filePath, int lineNumber, 
                              const std::string& type, const std::string& value);
    void send_device_config_add(const std::string& deviceName, const std::string& option, const std::string& value);
    void send_device_config_update(const std::string& filePath, int lineNumber,
                                    const std::string& deviceName, const std::string& option, const std::string& value);
    void send_device_config_remove(const std::string& filePath, int lineNumber, const std::string& deviceName);
    void create_section_view(const std::string& sectionPath);
    void create_executing_view();
    void create_device_configs_view();
    void on_section_selected();
    void add_section_to_tree(const std::string& sectionPath);
    std::string get_section_path(const std::string& optionName);
};

ConfigWindow::ConfigWindow()
: m_MainVBox(Gtk::Orientation::VERTICAL),
  m_HBox(Gtk::Orientation::HORIZONTAL)
{
    set_title("Hyprland Settings");
    set_default_size(950, 650);
    
    // Setup HeaderBar
    set_titlebar(m_HeaderBar);
    m_HeaderBar.set_show_title_buttons(true);
    
    m_Button_Refresh.set_icon_name("view-refresh-symbolic");
    m_Button_Refresh.set_tooltip_text("Refresh Options");
    m_Button_Refresh.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_button_refresh));
    m_HeaderBar.pack_start(m_Button_Refresh);

    set_child(m_MainVBox);

    // CSS Styling
    auto css_provider = Gtk::CssProvider::create();
    css_provider->load_from_data(
        "window { font-size: 11pt; }"
        "headerbar { padding: 0 6px; min-height: 46px; }"
        ".sidebar { background-color: alpha(@window_fg_color, 0.05); border-right: 1px solid alpha(@window_fg_color, 0.1); }"
        ".sidebar row { padding: 8px 12px; border-radius: 6px; margin: 2px 8px; }"
        ".sidebar row:selected { background-color: @accent_bg_color; color: @accent_fg_color; }"
        "columnview { background: transparent; }"
        "columnview row { padding: 8px; border-bottom: 1px solid alpha(@window_fg_color, 0.05); }"
        "columnview row:selected { background-color: alpha(@accent_bg_color, 0.1); color: @window_fg_color; }"
        "entry { padding: 6px; border-radius: 6px; }"
        "button { padding: 4px 12px; border-radius: 6px; }"
        "frame { border-radius: 12px; border: 1px solid alpha(@window_fg_color, 0.1); padding: 12px; }"
    );
    Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(), css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // HBox for Sidebar + Stack
    m_HBox.set_expand(true);
    m_MainVBox.append(m_HBox);

    // Setup TreeView for nested sections
    m_SectionTreeStore = Gtk::TreeStore::create(m_SectionColumns);
    m_TreeView.set_model(m_SectionTreeStore);
    m_TreeView.append_column("Section", m_SectionColumns.m_col_name);
    m_TreeView.set_headers_visible(false);
    m_TreeView.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &ConfigWindow::on_section_selected));
    m_TreeView.add_css_class("sidebar");
    
    // Wrap sidebar in a scrolled window
    auto sidebarScroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    sidebarScroll->set_child(m_TreeView);
    sidebarScroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    sidebarScroll->set_size_request(240, -1);
    sidebarScroll->add_css_class("sidebar-scroll");
    m_HBox.append(*sidebarScroll);

    // Stack
    m_Stack.set_expand(true);
    m_Stack.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
    m_Stack.set_margin(16);
    m_HBox.append(m_Stack);

    // Create keyword store
    m_KeywordStore = Gio::ListStore<KeywordItem>::create();
    
    // Create device config store
    m_DeviceConfigStore = Gio::ListStore<DeviceConfigItem>::create();

    load_data();
}

ConfigWindow::~ConfigWindow() {}

void ConfigWindow::create_section_view(const std::string& sectionPath) {
    auto listStore = Gio::ListStore<ConfigItem>::create();
    m_SectionStores[sectionPath] = listStore;

    auto selectionModel = Gtk::SingleSelection::create(listStore);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    // Columns
    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column_read));
    factory_name->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_name));
    auto col_name = Gtk::ColumnViewColumn::create("Option", factory_name);
    col_name->set_fixed_width(280);
    columnView->append_column(col_name);

    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column_edit));
    factory_value->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_value));
    auto col_val = Gtk::ColumnViewColumn::create("Value (Editable)", factory_value);
    col_val->set_expand(true);
    columnView->append_column(col_val);

    auto scrolledWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolledWindow->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolledWindow->set_child(*columnView);
    
    m_Stack.add(*scrolledWindow, sectionPath, sectionPath);
}

void ConfigWindow::create_executing_view() {
    auto mainBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    mainBox->set_spacing(10);
    mainBox->set_margin(10);

    // Add new keyword section
    auto addFrame = Gtk::make_managed<Gtk::Frame>("Add New Exec Command");
    auto addBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    addBox->set_spacing(5);
    addBox->set_margin(10);

    auto typeCombo = Gtk::make_managed<Gtk::DropDown>();
    auto typeModel = Gtk::StringList::create({"exec-once", "execr-once", "exec", "execr", "exec-shutdown"});
    typeCombo->set_model(typeModel);
    typeCombo->set_selected(0);

    auto valueEntry = Gtk::make_managed<Gtk::Entry>();
    valueEntry->set_placeholder_text("Command to execute...");
    valueEntry->set_hexpand(true);

    auto addButton = Gtk::make_managed<Gtk::Button>("Add");
    addButton->signal_clicked().connect([this, typeCombo, typeModel, valueEntry]() {
        auto selected = typeCombo->get_selected();
        if (selected != GTK_INVALID_LIST_POSITION) {
            auto typeStr = typeModel->get_string(selected);
            std::string value = valueEntry->get_text();
            if (!value.empty()) {
                send_keyword_add(typeStr, value);
                valueEntry->set_text("");
                load_keywords();
            }
        }
    });

    addBox->append(*typeCombo);
    addBox->append(*valueEntry);
    addBox->append(*addButton);
    addFrame->set_child(*addBox);
    mainBox->append(*addFrame);

    // Keywords list
    auto selectionModel = Gtk::SingleSelection::create(m_KeywordStore);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    // Type column
    auto factory_type = Gtk::SignalListItemFactory::create();
    factory_type->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_keyword_type));
    factory_type->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_keyword_type));
    auto col_type = Gtk::ColumnViewColumn::create("Type", factory_type);
    col_type->set_fixed_width(120);
    columnView->append_column(col_type);

    // Value column
    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_keyword_value));
    factory_value->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_keyword_value));
    auto col_value = Gtk::ColumnViewColumn::create("Command", factory_value);
    col_value->set_expand(true);
    columnView->append_column(col_value);

    // Actions column
    auto factory_actions = Gtk::SignalListItemFactory::create();
    factory_actions->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_keyword_actions));
    factory_actions->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_keyword_actions));
    auto col_actions = Gtk::ColumnViewColumn::create("Actions", factory_actions);
    col_actions->set_fixed_width(80);
    columnView->append_column(col_actions);

    auto scrolledWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolledWindow->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolledWindow->set_child(*columnView);
    scrolledWindow->set_vexpand(true);
    mainBox->append(*scrolledWindow);

    m_Stack.add(*mainBox, "__executing__", "Executing");
}

void ConfigWindow::create_device_configs_view() {
    auto mainBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    mainBox->set_spacing(10);
    mainBox->set_margin(10);

    // Add new device config section
    auto addFrame = Gtk::make_managed<Gtk::Frame>("Add Per-Device Input Config");
    auto addBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    addBox->set_spacing(5);
    addBox->set_margin(10);

    auto deviceCombo = Gtk::make_managed<Gtk::DropDown>();
    auto deviceModel = Gtk::StringList::create({});
    deviceCombo->set_model(deviceModel);
    
    // Populate with available devices
    for (const auto& device : m_AvailableDevices) {
        deviceModel->append(device);
    }
    if (!m_AvailableDevices.empty()) {
        deviceCombo->set_selected(0);
    }

    auto optionCombo = Gtk::make_managed<Gtk::DropDown>();
    auto optionModel = Gtk::StringList::create({
        "sensitivity", "accel_profile", "natural_scroll", "left_handed",
        "scroll_method", "scroll_button", "scroll_button_lock", "scroll_factor",
        "tap_button_map", "clickfinger_behavior", "tap-to-click", "drag_lock",
        "tap-and-drag", "disable_while_typing", "middle_button_emulation",
        "transform", "output", "enabled", "keybinds", "kb_layout", "kb_variant",
        "kb_model", "kb_options", "kb_rules", "kb_file", "numlock_by_default",
        "resolve_binds_by_sym", "repeat_rate", "repeat_delay"
    });
    optionCombo->set_model(optionModel);
    optionCombo->set_selected(0);

    auto valueEntry = Gtk::make_managed<Gtk::Entry>();
    valueEntry->set_placeholder_text("Value...");
    valueEntry->set_hexpand(true);

    auto addButton = Gtk::make_managed<Gtk::Button>("Add");
    addButton->signal_clicked().connect([this, deviceCombo, deviceModel, optionCombo, optionModel, valueEntry]() {
        auto deviceIdx = deviceCombo->get_selected();
        auto optionIdx = optionCombo->get_selected();
        if (deviceIdx != GTK_INVALID_LIST_POSITION && optionIdx != GTK_INVALID_LIST_POSITION) {
            std::string deviceName = deviceModel->get_string(deviceIdx);
            std::string option = optionModel->get_string(optionIdx);
            std::string value = valueEntry->get_text();
            if (!value.empty()) {
                send_device_config_add(deviceName, option, value);
                valueEntry->set_text("");
                load_device_configs();
            }
        }
    });

    addBox->append(*deviceCombo);
    addBox->append(*optionCombo);
    addBox->append(*valueEntry);
    addBox->append(*addButton);
    addFrame->set_child(*addBox);
    mainBox->append(*addFrame);

    // Device configs list
    auto selectionModel = Gtk::SingleSelection::create(m_DeviceConfigStore);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    // Device name column
    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_device_name));
    factory_name->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_device_name));
    auto col_name = Gtk::ColumnViewColumn::create("Device", factory_name);
    col_name->set_fixed_width(200);
    columnView->append_column(col_name);

    // Option column
    auto factory_option = Gtk::SignalListItemFactory::create();
    factory_option->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_device_option));
    factory_option->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_device_option));
    auto col_option = Gtk::ColumnViewColumn::create("Option", factory_option);
    col_option->set_fixed_width(150);
    columnView->append_column(col_option);

    // Value column
    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_device_value));
    factory_value->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_device_value));
    auto col_value = Gtk::ColumnViewColumn::create("Value", factory_value);
    col_value->set_expand(true);
    columnView->append_column(col_value);

    // Actions column
    auto factory_actions = Gtk::SignalListItemFactory::create();
    factory_actions->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_device_actions));
    factory_actions->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_device_actions));
    auto col_actions = Gtk::ColumnViewColumn::create("Actions", factory_actions);
    col_actions->set_fixed_width(80);
    columnView->append_column(col_actions);

    auto scrolledWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolledWindow->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledWindow->set_child(*columnView);
    scrolledWindow->set_vexpand(true);
    mainBox->append(*scrolledWindow);

    m_Stack.add(*mainBox, "__device_configs__", "Per-device Input Configs");
}

void ConfigWindow::setup_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    list_item->set_child(*label);
}

void ConfigWindow::setup_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::EditableLabel>();
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    
    label->property_editing().signal_changed().connect([this, label, list_item]() {
        if (!label->get_editing()) {
            auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
            if (item) {
                std::string newVal = label->get_text();
                if (newVal != item->m_value) {
                    send_keyword_update(item->m_filePath, item->m_lineNumber, item->m_type, newVal);
                    load_keywords();
                }
            }
        }
    });

    list_item->set_child(*label);
}

void ConfigWindow::setup_keyword_actions(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto button = Gtk::make_managed<Gtk::Button>();
    button->set_icon_name("user-trash-symbolic");
    button->set_tooltip_text("Delete");
    list_item->set_child(*button);
}

void ConfigWindow::bind_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_type);
        label->set_tooltip_text("File: " + item->m_filePath + "\nLine: " + std::to_string(item->m_lineNumber));
    }
}

void ConfigWindow::bind_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::EditableLabel*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_value);
    }
}

void ConfigWindow::bind_keyword_actions(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<KeywordItem>(list_item->get_item());
    auto button = dynamic_cast<Gtk::Button*>(list_item->get_child());
    if (item && button) {
        // Disconnect any previous handler
        button->signal_clicked().connect([this, item]() {
            send_keyword_remove(item->m_filePath, item->m_lineNumber);
            load_keywords();
        });
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
    auto label = Gtk::make_managed<Gtk::EditableLabel>();
    label->set_halign(Gtk::Align::START);
    label->set_hexpand(true);
    
    label->property_editing().signal_changed().connect([this, label, list_item]() {
        if (!label->get_editing()) {
            auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
            if (item) {
                std::string newVal = label->get_text();
                if (newVal != item->m_value) {
                    send_device_config_update(item->m_filePath, item->m_optionLine,
                                               item->m_deviceName, item->m_option, newVal);
                    load_device_configs();
                }
            }
        }
    });

    list_item->set_child(*label);
}

void ConfigWindow::setup_device_actions(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto button = Gtk::make_managed<Gtk::Button>();
    button->set_icon_name("user-trash-symbolic");
    button->set_tooltip_text("Delete");
    list_item->set_child(*button);
}

void ConfigWindow::bind_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_deviceName);
        label->set_tooltip_text("File: " + item->m_filePath + "\nSection starts at line: " + std::to_string(item->m_startLine));
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
    auto label = dynamic_cast<Gtk::EditableLabel*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_value);
    }
}

void ConfigWindow::bind_device_actions(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<DeviceConfigItem>(list_item->get_item());
    auto button = dynamic_cast<Gtk::Button*>(list_item->get_child());
    if (item && button) {
        button->signal_clicked().connect([this, item]() {
            send_device_config_remove(item->m_filePath, item->m_optionLine, item->m_deviceName);
            load_device_configs();
        });
    }
}

void ConfigWindow::on_section_selected() {
    auto iter = m_TreeView.get_selection()->get_selected();
    if (iter) {
        Glib::ustring fullPath = (*iter)[m_SectionColumns.m_col_full_path];
        m_Stack.set_visible_child(fullPath.raw());
    }
}

std::string ConfigWindow::get_section_path(const std::string& optionName) {
    // For "decoration:blur:noise", return "decoration:blur"
    size_t pos = optionName.rfind(':');
    if (pos != std::string::npos) {
        return optionName.substr(0, pos);
    }
    return "";  // Root level option
}

void ConfigWindow::add_section_to_tree(const std::string& sectionPath) {
    if (sectionPath.empty() || m_SectionStores.find(sectionPath) != m_SectionStores.end()) {
        return;  // Already exists or empty
    }
    
    auto parts = split_path(sectionPath, ':');
    std::string currentPath;
    Gtk::TreeModel::iterator parentIter;
    
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) currentPath += ":";
        currentPath += parts[i];
        
        // Check if this path already exists in tree
        if (m_SectionIters.find(currentPath) == m_SectionIters.end()) {
            Gtk::TreeModel::iterator newIter;
            if (parentIter) {
                newIter = m_SectionTreeStore->append(parentIter->children());
            } else {
                newIter = m_SectionTreeStore->append();
            }
            (*newIter)[m_SectionColumns.m_col_name] = parts[i];
            (*newIter)[m_SectionColumns.m_col_full_path] = currentPath;
            m_SectionIters[currentPath] = newIter;
            
            // Create view for this section
            create_section_view(currentPath);
        }
        parentIter = m_SectionIters[currentPath];
    }
}

void ConfigWindow::setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    list_item->set_child(*label);
}



void ConfigWindow::setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::EditableLabel>();
    label->set_halign(Gtk::Align::START);
    
    label->property_editing().signal_changed().connect([this, label, list_item]() {
        if (!label->get_editing()) {
            auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
            if (item) {
                std::string newVal = label->get_text();
                if (newVal != item->m_value) {
                    item->m_value = newVal;
                    send_update(item->m_name, newVal);
                }
            }
        }
    });

    list_item->set_child(*label);
}

void ConfigWindow::bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_short_name);
        label->set_tooltip_text(item->m_desc);
    }
}

void ConfigWindow::bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::EditableLabel*>(list_item->get_child());
    if (item && label) label->set_text(item->m_value);
}



void ConfigWindow::on_button_refresh() {
    load_data();
}

void readData(int fd, void* buffer, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t r = read(fd, (char*)buffer + total, size - total);
        if (r <= 0) break; 
        total += r;
    }
}

std::string readString(int fd) {
    uint32_t len = 0;
    readData(fd, &len, sizeof(len));
    if (len == 0) return "";
    std::vector<char> buf(len + 1);
    readData(fd, buf.data(), len);
    buf[len] = '\0';
    return std::string(buf.data());
}

void ConfigWindow::send_update(const std::string& name, const std::string& value) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::SET_OPTION_PERSIST;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(name);
    writeStr(value);
    
    // Read response
    std::string response = readString(sock);
    
    ::close(sock);
    std::cout << "Update " << name << " = " << value << " -> " << response << std::endl;
}

void ConfigWindow::send_keyword_add(const std::string& type, const std::string& value) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::ADD_KEYWORD;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(type);
    writeStr(value);
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Add keyword " << type << " = " << value << " -> " << response << std::endl;
}

void ConfigWindow::send_keyword_remove(const std::string& filePath, int lineNumber) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::REMOVE_KEYWORD;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(filePath);
    int32_t lineNum = lineNumber;
    write(sock, &lineNum, sizeof(lineNum));
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Remove keyword at " << filePath << ":" << lineNumber << " -> " << response << std::endl;
}

void ConfigWindow::send_keyword_update(const std::string& filePath, int lineNumber,
                                        const std::string& type, const std::string& value) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::UPDATE_KEYWORD;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(filePath);
    int32_t lineNum = lineNumber;
    write(sock, &lineNum, sizeof(lineNum));
    writeStr(type);
    writeStr(value);
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Update keyword at " << filePath << ":" << lineNumber << " -> " << response << std::endl;
}

void ConfigWindow::send_device_config_add(const std::string& deviceName, const std::string& option, const std::string& value) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::ADD_DEVICE_CONFIG;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(deviceName);
    writeStr(option);
    writeStr(value);
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Add device config " << deviceName << ":" << option << " = " << value << " -> " << response << std::endl;
}

void ConfigWindow::send_device_config_update(const std::string& filePath, int lineNumber,
                                              const std::string& deviceName, const std::string& option, const std::string& value) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::UPDATE_DEVICE_CONFIG;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(filePath);
    int32_t lineNum = lineNumber;
    write(sock, &lineNum, sizeof(lineNum));
    writeStr(deviceName);
    writeStr(option);
    writeStr(value);
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Update device config at " << filePath << ":" << lineNumber << " -> " << response << std::endl;
}

void ConfigWindow::send_device_config_remove(const std::string& filePath, int lineNumber, const std::string& deviceName) {
    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) return;

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::REMOVE_DEVICE_CONFIG;
    write(sock, &req, sizeof(req));
    
    auto writeStr = [&](const std::string& s) {
        uint32_t len = s.length();
        write(sock, &len, sizeof(len));
        if (len > 0) write(sock, s.c_str(), len);
    };

    writeStr(filePath);
    int32_t lineNum = lineNumber;
    write(sock, &lineNum, sizeof(lineNum));
    writeStr(deviceName);
    
    std::string response = readString(sock);
    ::close(sock);
    std::cout << "Remove device config at " << filePath << ":" << lineNumber << " -> " << response << std::endl;
}

void ConfigWindow::load_keywords() {
    m_KeywordStore->remove_all();

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed for keywords" << std::endl;
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::GET_KEYWORDS;
    write(sock, &req, sizeof(req));

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        std::string type = readString(sock);
        std::string value = readString(sock);
        std::string filePath = readString(sock);
        int32_t lineNumber;
        readData(sock, &lineNumber, sizeof(lineNumber));

        m_KeywordStore->append(KeywordItem::create(type, value, filePath, lineNumber));
    }
    ::close(sock);
}

void ConfigWindow::load_device_configs() {
    m_DeviceConfigStore->remove_all();

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed for device configs" << std::endl;
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::GET_DEVICE_CONFIGS;
    write(sock, &req, sizeof(req));

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        std::string deviceName = readString(sock);
        std::string option = readString(sock);
        std::string value = readString(sock);
        std::string filePath = readString(sock);
        int32_t startLine, optionLine;
        readData(sock, &startLine, sizeof(startLine));
        readData(sock, &optionLine, sizeof(optionLine));

        m_DeviceConfigStore->append(DeviceConfigItem::create(deviceName, option, value, filePath, startLine, optionLine));
    }
    ::close(sock);
}

void ConfigWindow::load_available_devices() {
    m_AvailableDevices.clear();

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed for devices" << std::endl;
        ::close(sock);
        return;
    }

    IPC::RequestType req = IPC::RequestType::GET_DEVICES;
    write(sock, &req, sizeof(req));

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));
    
    for (uint32_t i = 0; i < count; ++i) {
        std::string name = readString(sock);
        if (!name.empty()) {
            m_AvailableDevices.push_back(name);
        }
    }
    ::close(sock);
}

void ConfigWindow::load_data() {
    // Clear existing stores and tree
    for (auto& kv : m_SectionStores) {
        kv.second->remove_all();
    }
    m_SectionTreeStore->clear();
    m_SectionStores.clear();
    m_SectionIters.clear();
    
    // Remove all children from stack
    while (auto child = m_Stack.get_first_child()) {
        m_Stack.remove(*child);
    }
    
    // Load available devices first (needed for device configs view)
    load_available_devices();

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        ::close(sock);
        return;
    }

    // Send GET request
    IPC::RequestType req = IPC::RequestType::GET_ALL;
    write(sock, &req, sizeof(req));

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));

    // First pass: collect all section paths
    std::set<std::string> allSections;
    std::vector<std::tuple<std::string, std::string, std::string, bool>> allOptions;
    
    for (uint32_t i = 0; i < count; ++i) {
        std::string name = readString(sock);
        std::string desc = readString(sock);
        std::string valStr = readString(sock);
        
        uint8_t setByUser = 0;
        readData(sock, &setByUser, sizeof(setByUser));
        
        std::string sectionPath = get_section_path(name);
        if (!sectionPath.empty()) {
            allSections.insert(sectionPath);
        }
        allOptions.emplace_back(name, valStr, desc, setByUser != 0);
    }
    ::close(sock);

    // Create Variables parent section
    auto variablesIter = m_SectionTreeStore->append();
    (*variablesIter)[m_SectionColumns.m_col_name] = "Variables";
    (*variablesIter)[m_SectionColumns.m_col_full_path] = "__variables__";
    m_SectionIters["__variables__"] = variablesIter;

    // Create all section views under Variables in the tree
    for (const auto& sectionPath : allSections) {
        auto parts = split_path(sectionPath, ':');
        std::string currentPath;
        Gtk::TreeModel::iterator parentIter = variablesIter;
        
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) currentPath += ":";
            currentPath += parts[i];
            
            if (m_SectionIters.find(currentPath) == m_SectionIters.end()) {
                auto newIter = m_SectionTreeStore->append(parentIter->children());
                (*newIter)[m_SectionColumns.m_col_name] = parts[i];
                (*newIter)[m_SectionColumns.m_col_full_path] = currentPath;
                m_SectionIters[currentPath] = newIter;
                create_section_view(currentPath);
            }
            parentIter = m_SectionIters[currentPath];
        }
    }
    
    // Expand Variables section
    m_TreeView.expand_row(Gtk::TreePath(variablesIter), false);

    // Second pass: add options to their sections
    for (const auto& [name, valStr, desc, setByUser] : allOptions) {
        std::string sectionPath = get_section_path(name);
        
        if (sectionPath.empty()) {
            // Root level option - add under Variables
            if (m_SectionStores.find("") == m_SectionStores.end()) {
                auto iter = m_SectionTreeStore->append(variablesIter->children());
                (*iter)[m_SectionColumns.m_col_name] = "(root)";
                (*iter)[m_SectionColumns.m_col_full_path] = "";
                m_SectionIters[""] = iter;
                create_section_view("");
            }
        }
        
        if (m_SectionStores.find(sectionPath) != m_SectionStores.end()) {
            m_SectionStores[sectionPath]->append(ConfigItem::create(name, valStr, desc, setByUser));
        }
    }

    // Add Keywords parent section to the sidebar
    auto keywordsIter = m_SectionTreeStore->append();
    (*keywordsIter)[m_SectionColumns.m_col_name] = "Keywords";
    (*keywordsIter)[m_SectionColumns.m_col_full_path] = "__keywords_parent__";
    m_SectionIters["__keywords_parent__"] = keywordsIter;
    
    // Add "Executing" subsection under Keywords
    auto executingIter = m_SectionTreeStore->append(keywordsIter->children());
    (*executingIter)[m_SectionColumns.m_col_name] = "Executing";
    (*executingIter)[m_SectionColumns.m_col_full_path] = "__executing__";
    m_SectionIters["__executing__"] = executingIter;
    
    // Add "Per-device Input Configs" subsection under Keywords
    auto deviceConfigsIter = m_SectionTreeStore->append(keywordsIter->children());
    (*deviceConfigsIter)[m_SectionColumns.m_col_name] = "Per-device Input Configs";
    (*deviceConfigsIter)[m_SectionColumns.m_col_full_path] = "__device_configs__";
    m_SectionIters["__device_configs__"] = deviceConfigsIter;
    
    // Create and populate views
    create_executing_view();
    create_device_configs_view();
    load_keywords();
    load_device_configs();
    
    // Expand Keywords section
    m_TreeView.expand_row(Gtk::TreePath(keywordsIter), false);
}

int main(int argc, char* argv[])
{
  auto app = Gtk::Application::create("org.hyprland.settings");
  return app->make_window_and_run<ConfigWindow>(argc, argv);
}
