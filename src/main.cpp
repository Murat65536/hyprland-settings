#include <gtkmm.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <regex>
#include <iomanip>
#include <cmath>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <libgen.h>
#include <limits.h>
#include <json-glib/json-glib.h>
#include <filesystem>
#include "config_io.hpp"

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
    void on_hyprland_button_clicked();
    void on_scroll_changed();

    Gtk::HeaderBar m_HeaderBar;
    Gtk::Box m_MainVBox;
    Gtk::Stack m_MainStack;
    Gtk::Box m_MenuBox;
    Gtk::Box m_ContentBox;
    Gtk::Box m_HBox;
    Gtk::TreeView m_TreeView;
    Gtk::ScrolledWindow m_ContentScroll;
    Gtk::Box m_ContentVBox;
    Gtk::Button m_Button_Refresh;
    Gtk::Button m_Button_Hyprland;

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
        bool m_isBoolean = false;
        bool m_hasChoices = false;
        std::vector<std::pair<std::string, std::string>> m_choices;
        
        bool m_hasRange = false;
        double m_rangeMin = 0.0;
        double m_rangeMax = 1.0;
        bool m_isFloat = false;

        static Glib::RefPtr<ConfigItem> create(const std::string& name, const std::string& value, const std::string& desc,
                                               bool setByUser, bool isBoolean) {
            return Glib::make_refptr_for_instance<ConfigItem>(new ConfigItem(name, value, desc, setByUser, isBoolean));
        }

    protected:
        ConfigItem(const std::string& name, const std::string& value, const std::string& desc, bool setByUser, bool isBoolean)
            : m_name(name), m_value(value), m_desc(desc), m_setByUser(setByUser), m_isBoolean(isBoolean) {
            // Short name is the last part after the last ':'
            size_t pos = name.rfind(':');
            if (pos != std::string::npos) {
                m_short_name = name.substr(pos + 1);
            } else {
                m_short_name = name;
            }

            if (m_isBoolean) {
                if (m_value == "1" || m_value == "true") {
                    m_value = "true";
                } else if (m_value == "0" || m_value == "false") {
                    m_value = "false";
                }
            }

            auto trim = [](std::string s) {
                s = std::regex_replace(s, std::regex(R"(^\s+|\s+$)"), "");
                return s;
            };

            // Parse enum-like options from descriptions such as:
            // "0 - off, 1 - always, 2 - fullscreen"
            std::regex choiceRegex(R"((-?\d+)\s*-\s*([^,;\n]+))");
            std::vector<std::pair<std::string, std::string>> parsedChoices;
            std::vector<std::smatch> choiceMatches;
            for (std::sregex_iterator it(m_desc.begin(), m_desc.end(), choiceRegex), end; it != end; ++it) {
                const auto& match = *it;
                std::string choiceValue = match[1].str();
                std::string choiceLabel = trim(match[2].str());

                // Avoid treating simple numeric ranges as enum labels.
                if (!std::regex_search(choiceLabel, std::regex(R"([A-Za-z])"))) {
                    continue;
                }

                parsedChoices.emplace_back(choiceValue, choiceLabel);
                choiceMatches.push_back(match);
            }

            if (parsedChoices.size() >= 2 && !choiceMatches.empty()) {
                const auto& first = choiceMatches.front();
                const auto& last = choiceMatches.back();
                size_t firstPos = static_cast<size_t>(first.position());
                size_t tailPos = static_cast<size_t>(last.position() + last.length());

                // Only treat this as an enum list if the match block is at the end.
                std::string tail = m_desc.substr(tailPos);
                if (std::regex_match(tail, std::regex(R"(\s*[.)]?\s*)"))) {
                    m_hasChoices = true;
                    m_choices = parsedChoices;

                    m_desc = trim(m_desc.substr(0, firstPos));
                    m_desc = std::regex_replace(m_desc, std::regex(R"([:;,.\-]\s*$)"), "");
                }
            }

            // Parse range from description: [0.0 - 1.0] or similar
            std::regex rangeRegex(R"(\[\s*(-?\d+\.?\d*)\s*(?:-|\.\.|to|,)\s*(-?\d+\.?\d*)\s*\])");
            std::smatch match;
            if (std::regex_search(m_desc, match, rangeRegex)) {
                try {
                    m_rangeMin = std::stod(match[1].str());
                    m_rangeMax = std::stod(match[2].str());
                    m_hasRange = true;
                    
                    // Check if it's float or int by looking for dots in min/max
                    m_isFloat = (match[1].str().find('.') != std::string::npos) || 
                                (match[2].str().find('.') != std::string::npos);
                    
                    // Remove the range part from the description
                    m_desc = std::regex_replace(m_desc, rangeRegex, "");
                    // Clean up trailing/leading spaces
                    m_desc = std::regex_replace(m_desc, std::regex(R"(^\s+|\s+$)"), "");
                } catch (...) {
                    m_hasRange = false;
                }
            }
        }
    };

    // Keyword item for the keywords list
    class KeywordItem : public Glib::Object {
    public:
        std::string m_type;      // exec-once, execr, etc.
        std::string m_value;     // The command

        static Glib::RefPtr<KeywordItem> create(const std::string& type, const std::string& value) {
            return Glib::make_refptr_for_instance<KeywordItem>(new KeywordItem(type, value));
        }

    protected:
        KeywordItem(const std::string& type, const std::string& value)
            : m_type(type), m_value(value) {}
    };

    // Device config item for per-device input configs
    class DeviceConfigItem : public Glib::Object {
    public:
        std::string m_deviceName;  // Device name from hyprctl
        std::string m_option;      // Config option (sensitivity, etc.)
        std::string m_value;       // The value

        static Glib::RefPtr<DeviceConfigItem> create(const std::string& deviceName, const std::string& option,
                                                      const std::string& value) {
            return Glib::make_refptr_for_instance<DeviceConfigItem>(
                new DeviceConfigItem(deviceName, option, value));
        }

    protected:
        DeviceConfigItem(const std::string& deviceName, const std::string& option,
                         const std::string& value)
            : m_deviceName(deviceName), m_option(option), m_value(value) {}
    };

    std::map<std::string, Glib::RefPtr<Gio::ListStore<ConfigItem>>> m_SectionStores;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_KeywordStore;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_ExecutingStore;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_EnvVarStore;
    Glib::RefPtr<Gio::ListStore<DeviceConfigItem>> m_DeviceConfigStore;
    std::vector<std::string> m_AvailableDevices;
    SectionNode m_RootSection;
    std::map<std::string, Gtk::TreeModel::iterator> m_SectionIters;

    // Track widgets for scrolling
    std::map<std::string, Gtk::Widget*> m_SectionWidgets;
    std::vector<std::pair<std::string, Gtk::Widget*>> m_OrderedSections;
    bool m_scrolling_programmatically = false;
    bool m_selecting_programmatically = false;
    bool m_binding_programmatically = false;

    void setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    // Keyword-specific methods
    void setup_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    // Device config methods
    void setup_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    void load_data();
    void load_keywords();
    void load_device_configs();
    void load_available_devices();
    void send_update(const std::string& name, const std::string& value);
    void send_runtime_update(const std::string& name, const std::string& value);
    void send_keyword_add(const std::string& type, const std::string& value);
    void send_device_config_add(const std::string& deviceName, const std::string& option, const std::string& value);
    void create_section_view(const std::string& sectionPath);
    void create_executing_view();
    void create_device_configs_view();
    void create_env_vars_view();
    void on_section_selected();
    void add_section_to_tree(const std::string& sectionPath);
    std::string get_section_path(const std::string& optionName);
};

ConfigWindow::ConfigWindow()
: m_MainVBox(Gtk::Orientation::VERTICAL),
  m_MenuBox(Gtk::Orientation::VERTICAL),
  m_ContentBox(Gtk::Orientation::VERTICAL),
  m_HBox(Gtk::Orientation::HORIZONTAL),
  m_ContentVBox(Gtk::Orientation::VERTICAL)
{
    set_title("Hyprland Settings");
    set_default_size(950, 650);

    // Setup HeaderBar
    set_titlebar(m_HeaderBar);
    m_HeaderBar.set_show_title_buttons(true);

    // Back button to return to main menu
    Gtk::Button* backButton = Gtk::manage(new Gtk::Button("Back"));
    backButton->set_tooltip_text("Return to Main Menu");
    backButton->signal_clicked().connect([this]() {
        m_MainStack.set_visible_child("menu");
    });
    m_HeaderBar.pack_start(*backButton);

    m_Button_Refresh.set_icon_name("view-refresh-symbolic");
    m_Button_Refresh.set_tooltip_text("Refresh Options");
    m_Button_Refresh.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_button_refresh));
    m_HeaderBar.pack_start(m_Button_Refresh);

    set_child(m_MainVBox);

    auto css_provider = Gtk::CssProvider::create();
    if (std::filesystem::exists("style.css")) {
        css_provider->load_from_path("style.css");
    } else if (std::filesystem::exists("src/style.css")) {
        css_provider->load_from_path("src/style.css");
    } else {
        std::cerr << "Warning: style.css not found! UI might look unstyled." << std::endl;
    }
    
    auto display = Gdk::Display::get_default();
    if (display) {
        Gtk::StyleContext::add_provider_for_display(display, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } else {
        std::cerr << "Warning: No default display found for CSS!" << std::endl;
    }

    // Initialize main stack
    m_MainStack.set_expand(true);
    m_MainStack.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
    m_MainVBox.append(m_MainStack);

    // Create main menu
    m_MenuBox.set_spacing(20);
    m_MenuBox.set_halign(Gtk::Align::CENTER);
    m_MenuBox.set_valign(Gtk::Align::CENTER);

    // Create Hyprland button (square shape)
    m_Button_Hyprland.set_label("Hyprland");
    m_Button_Hyprland.set_size_request(200, 200);  // Square shape
    m_Button_Hyprland.add_css_class("hyprland-button");
    m_Button_Hyprland.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_hyprland_button_clicked));

    // Add only the button to the menu box (no other text)
    m_MenuBox.append(m_Button_Hyprland);

    // Add menu to main stack
    m_MainStack.add(m_MenuBox, "menu", "Main Menu");

    // Setup content area
    // HBox for Sidebar + Content
    m_HBox.set_expand(true);
    m_ContentBox.append(m_HBox);

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

    // Content Scroll Area
    m_ContentScroll.set_expand(true);
    m_ContentScroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_ContentScroll.set_child(m_ContentVBox);
    m_ContentVBox.set_margin(20);
    m_ContentVBox.set_spacing(40); // Spacing between sections

    // Scroll listener
    auto vAdj = m_ContentScroll.get_vadjustment();
    vAdj->signal_value_changed().connect(sigc::mem_fun(*this, &ConfigWindow::on_scroll_changed));

    m_HBox.append(m_ContentScroll);

    // Add content box to main stack
    m_MainStack.add(m_ContentBox, "content", "Settings");

    // Create keyword store
    m_KeywordStore = Gio::ListStore<KeywordItem>::create();
    m_ExecutingStore = Gio::ListStore<KeywordItem>::create();
    m_EnvVarStore = Gio::ListStore<KeywordItem>::create();

    // Create device config store
    m_DeviceConfigStore = Gio::ListStore<DeviceConfigItem>::create();

    // Initially show the menu
    m_MainStack.set_visible_child("menu");
}

ConfigWindow::~ConfigWindow() {}

void ConfigWindow::on_hyprland_button_clicked() {
    // Load the data and switch to content view
    load_data();
    m_MainStack.set_visible_child("content");
}

void ConfigWindow::create_section_view(const std::string& sectionPath) {
    auto listStore = Gio::ListStore<ConfigItem>::create();
    m_SectionStores[sectionPath] = listStore;

    // Create Section Container
    auto sectionBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    sectionBox->set_spacing(10);
    
    // Header
    std::string title = sectionPath;
    auto label = Gtk::make_managed<Gtk::Label>(title);
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    sectionBox->append(*label);

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
    auto col_val = Gtk::ColumnViewColumn::create("Value", factory_value);
    col_val->set_expand(true);
    columnView->append_column(col_val);

    // Add ColumnView directly to box (no internal scroll)
    sectionBox->append(*columnView);
    
    m_ContentVBox.append(*sectionBox);
    m_SectionWidgets[sectionPath] = sectionBox;
    m_OrderedSections.push_back({sectionPath, sectionBox});
}

void ConfigWindow::create_executing_view() {
    std::string sectionPath = "__executing__";
    auto mainBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    mainBox->set_spacing(10);

    // Header
    auto label = Gtk::make_managed<Gtk::Label>("Executing (Runtime)");
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    mainBox->append(*label);

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
                m_ExecutingStore->append(KeywordItem::create(typeStr, value));
                valueEntry->set_text("");
            }
        }
    });

    addBox->append(*typeCombo);
    addBox->append(*valueEntry);
    addBox->append(*addButton);
    addFrame->set_child(*addBox);
    mainBox->append(*addFrame);

    // List for executing commands
    auto selectionModel = Gtk::SingleSelection::create(m_ExecutingStore);
    
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

    mainBox->append(*columnView);

    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
}

void ConfigWindow::create_env_vars_view() {
    std::string sectionPath = "__env_vars__";
    auto mainBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    mainBox->set_spacing(10);

    // Header
    auto label = Gtk::make_managed<Gtk::Label>("Environment Variables");
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    mainBox->append(*label);

    // Add new environment variable section
    auto addFrame = Gtk::make_managed<Gtk::Frame>("Add New Environment Variable");
    auto addBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    addBox->set_spacing(5);
    addBox->set_margin(10);

    auto typeCombo = Gtk::make_managed<Gtk::DropDown>();
    auto typeModel = Gtk::StringList::create({"env", "envd"});
    typeCombo->set_model(typeModel);
    typeCombo->set_selected(0);

    auto nameEntry = Gtk::make_managed<Gtk::Entry>();
    nameEntry->set_placeholder_text("Name...");
    nameEntry->set_width_chars(20);

    auto valueEntry = Gtk::make_managed<Gtk::Entry>();
    valueEntry->set_placeholder_text("Value...");
    valueEntry->set_hexpand(true);

    auto addButton = Gtk::make_managed<Gtk::Button>("Add");
    addButton->signal_clicked().connect([this, typeCombo, typeModel, nameEntry, valueEntry]() {
        auto selected = typeCombo->get_selected();
        if (selected != GTK_INVALID_LIST_POSITION) {
            auto typeStr = typeModel->get_string(selected);
            std::string name = nameEntry->get_text();
            std::string value = valueEntry->get_text();
            if (!name.empty()) {
                std::string fullVal = name + "," + value;
                send_keyword_add(typeStr, fullVal);
                m_EnvVarStore->append(KeywordItem::create(typeStr, fullVal));
                nameEntry->set_text("");
                valueEntry->set_text("");
            }
        }
    });

    addBox->append(*typeCombo);
    addBox->append(*nameEntry);
    addBox->append(*valueEntry);
    addBox->append(*addButton);
    addFrame->set_child(*addBox);
    mainBox->append(*addFrame);

    // List for environment variables
    auto selectionModel = Gtk::SingleSelection::create(m_EnvVarStore);
    
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    // Type column
    auto factory_type = Gtk::SignalListItemFactory::create();
    factory_type->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_keyword_type));
    factory_type->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_keyword_type));
    auto col_type = Gtk::ColumnViewColumn::create("Type", factory_type);
    col_type->set_fixed_width(80);
    columnView->append_column(col_type);

    // Value column (Name,Value)
    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_keyword_value));
    factory_value->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_keyword_value));
    auto col_value = Gtk::ColumnViewColumn::create("Variable (Name,Value)", factory_value);
    col_value->set_expand(true);
    columnView->append_column(col_value);

    mainBox->append(*columnView);

    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
}

void ConfigWindow::create_device_configs_view() {
    std::string sectionPath = "__device_configs__";
    auto mainBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    mainBox->set_spacing(10);

    // Header
    auto label = Gtk::make_managed<Gtk::Label>("Per-device Input Configs");
    label->add_css_class("section-title");
    label->set_halign(Gtk::Align::START);
    mainBox->append(*label);

    // Add new device config section
    auto addFrame = Gtk::make_managed<Gtk::Frame>("Set Per-Device Config");
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

    auto addButton = Gtk::make_managed<Gtk::Button>("Apply");
    addButton->signal_clicked().connect([this, deviceCombo, deviceModel, optionCombo, optionModel, valueEntry]() {
        auto deviceIdx = deviceCombo->get_selected();
        auto optionIdx = optionCombo->get_selected();
        if (deviceIdx != GTK_INVALID_LIST_POSITION && optionIdx != GTK_INVALID_LIST_POSITION) {
            std::string deviceName = deviceModel->get_string(deviceIdx);
            std::string option = optionModel->get_string(optionIdx);
            std::string value = valueEntry->get_text();
            if (!value.empty()) {
                send_device_config_add(deviceName, option, value);
                m_DeviceConfigStore->append(DeviceConfigItem::create(deviceName, option, value));
                valueEntry->set_text("");
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

    mainBox->append(*columnView);

    m_ContentVBox.append(*mainBox);
    m_SectionWidgets[sectionPath] = mainBox;
    m_OrderedSections.push_back({sectionPath, mainBox});
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

void ConfigWindow::on_section_selected() {
    if (m_selecting_programmatically) return;

    auto iter = m_TreeView.get_selection()->get_selected();
    if (iter) {
        Glib::ustring fullPath = (*iter)[m_SectionColumns.m_col_full_path];
        // Scroll to widget if exists
        auto it = m_SectionWidgets.find(fullPath.raw());
        if (it != m_SectionWidgets.end()) {
            Gtk::Widget* target = it->second;
            
            // To be precise, we need the position relative to m_ContentVBox
            // Since they are direct children, get_allocation should give us the right y (if already allocated)
            // But we need to use translate_coordinates to be safe if hierarchy changes
            double x, y;
            if (target->translate_coordinates(m_ContentVBox, 0, 0, x, y)) {
                m_scrolling_programmatically = true;
                m_ContentScroll.get_vadjustment()->set_value(y);
                
                // Process events to prevent immediate feedback loop if necessary
                // but m_scrolling_programmatically should handle it
                
                // Reset flag after a short delay or immediately?
                // The set_value signal is synchronous.
                m_scrolling_programmatically = false; 
            }
        }
    }
}

void ConfigWindow::on_scroll_changed() {
    if (m_scrolling_programmatically || m_OrderedSections.empty()) return;

    double scrollY = m_ContentScroll.get_vadjustment()->get_value();
    
    // Find the section currently at the top
    std::string currentPath;
    
    for (auto& pair : m_OrderedSections) {
        Gtk::Widget* widget = pair.second;
        double x, y;
        if (widget->translate_coordinates(m_ContentVBox, 0, 0, x, y)) {
            double height = widget->get_height();
            // Check if this widget contains the scrollY point (with some tolerance/header offset)
            // Or simply find the last one that started before scrollY + offset
            
            // If top of widget is visible or above view top, and bottom is below view top
            if (y <= scrollY + 100 && (y + height) > scrollY + 50) {
                 currentPath = pair.first;
                 // Don't break immediately, we might want the "most visible" one?
                 // But first one intersecting top is standard spy scroll behavior.
                 break; 
            }
        }
    }

    if (!currentPath.empty()) {
        auto it = m_SectionIters.find(currentPath);
        if (it != m_SectionIters.end()) {
            m_selecting_programmatically = true;
            m_TreeView.get_selection()->select(it->second);
            // Also ensure the treeview scrolls to show the selected item
            m_TreeView.scroll_to_row(m_SectionTreeStore->get_path(it->second));
            m_selecting_programmatically = false;
        }
    }
}

std::string ConfigWindow::get_section_path(const std::string& optionName) {
    size_t pos = optionName.rfind(':');
    if (pos != std::string::npos) {
        return optionName.substr(0, pos);
    }
    return "";  // Root level option
}

void ConfigWindow::setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    
    // Add click gesture for description
    auto gesture = Gtk::GestureClick::create();
    gesture->set_button(GDK_BUTTON_PRIMARY);
    gesture->signal_released().connect([this, list_item](int, double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && !item->m_desc.empty()) {
            auto dialog = new Gtk::Dialog();
            dialog->set_transient_for(*this);
            dialog->set_modal(true);
            dialog->set_title("Description");
            
            auto content_area = dialog->get_content_area();
            content_area->set_margin(20);
            content_area->set_spacing(10);

            auto titleLabel = Gtk::make_managed<Gtk::Label>(item->m_short_name);
            titleLabel->add_css_class("dialog-title");
            titleLabel->set_halign(Gtk::Align::START);
            content_area->append(*titleLabel);

            auto label = Gtk::make_managed<Gtk::Label>(item->m_desc);
            label->set_wrap(true);
            label->set_max_width_chars(60);
            label->set_halign(Gtk::Align::START);
            content_area->append(*label);
            
            dialog->add_button("Close", Gtk::ResponseType::CLOSE);
            dialog->signal_response().connect([dialog](int) {
                dialog->hide();
                Glib::signal_idle().connect_once([dialog]() {
                    delete dialog;
                });
            });
            dialog->show();
        }
    });
    label->add_controller(gesture);
    
    // Set cursor to pointer to indicate clickability
    label->set_cursor(Gdk::Cursor::create("pointer"));

    list_item->set_child(*label);
}

void ConfigWindow::setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item) {
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

    auto rangeBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    rangeBox->set_spacing(10);
    rangeBox->set_hexpand(true);
    
    auto slider = Gtk::make_managed<Gtk::Scale>(Gtk::Orientation::HORIZONTAL);
    slider->set_hexpand(true);
    slider->set_draw_value(false);

    // Prevent accidental value changes from mouse-wheel scrolling.
    auto sliderScrollBlocker = Gtk::EventControllerScroll::create();
    sliderScrollBlocker->set_flags(
        Gtk::EventControllerScroll::Flags::VERTICAL |
        Gtk::EventControllerScroll::Flags::HORIZONTAL |
        Gtk::EventControllerScroll::Flags::DISCRETE
    );
    sliderScrollBlocker->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    sliderScrollBlocker->signal_scroll().connect(
        [this](double dx, double dy) {
            auto vadj = m_ContentScroll.get_vadjustment();
            if (vadj && dy != 0.0) {
                const double step = vadj->get_step_increment() > 0.0 ? vadj->get_step_increment() : 40.0;
                const double lower = vadj->get_lower();
                const double upper = vadj->get_upper() - vadj->get_page_size();
                const double target = std::clamp(vadj->get_value() + (dy * step), lower, upper);
                vadj->set_value(target);
            }

            auto hadj = m_ContentScroll.get_hadjustment();
            if (hadj && dx != 0.0) {
                const double step = hadj->get_step_increment() > 0.0 ? hadj->get_step_increment() : 40.0;
                const double lower = hadj->get_lower();
                const double upper = hadj->get_upper() - hadj->get_page_size();
                const double target = std::clamp(hadj->get_value() + (dx * step), lower, upper);
                hadj->set_value(target);
            }

            // Consume so Gtk::Scale doesn't treat wheel input as value changes.
            return true;
        },
        false
    );
    slider->add_controller(sliderScrollBlocker);

    rangeBox->append(*slider);
    
    auto entry = Gtk::make_managed<Gtk::Entry>();
    entry->set_width_chars(8);
    entry->set_valign(Gtk::Align::CENTER);
    rangeBox->append(*entry);
    
    container->append(*rangeBox);
    list_item->set_child(*container);

    boolButton->signal_clicked().connect([this, boolButton, list_item]() {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_isBoolean) {
            const bool nextValue = (item->m_value != "true");
            item->m_value = nextValue ? "true" : "false";
            boolButton->set_label(item->m_value);
            send_update(item->m_name, item->m_value);
        }
    });

    choiceDropDown->property_selected().signal_changed().connect([this, choiceDropDown, list_item]() {
        if (m_binding_programmatically) return;
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

    // Signals for EditableLabel
    label->property_editing().signal_changed().connect([this, label, list_item]() {
        if (!label->get_editing()) {
            auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
            if (item && !item->m_hasRange && !item->m_isBoolean && !item->m_hasChoices) {
                std::string newVal = label->get_text();
                if (newVal != item->m_value) {
                    item->m_value = newVal;
                    send_update(item->m_name, newVal);
                }
            }
        }
    });

    // Signals for Slider
    slider->signal_value_changed().connect([this, slider, entry, list_item]() {
        if (m_binding_programmatically) return;
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange) {
            double val = slider->get_value();
            std::string valStr;
            if (item->m_isFloat) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << val;
                valStr = ss.str();
                // Remove trailing zeros and dot if possible
                if (!valStr.empty()) {
                    valStr.erase(valStr.find_last_not_of('0') + 1, std::string::npos);
                    if (!valStr.empty() && valStr.back() == '.') valStr.pop_back();
                }
            } else {
                valStr = std::to_string((int)std::round(val));
            }
            
            if (valStr != item->m_value) {
                entry->set_text(valStr);
                item->m_value = valStr;
                send_runtime_update(item->m_name, valStr);
            }
        }
    });

    auto dragGesture = Gtk::GestureDrag::create();
    dragGesture->signal_drag_end().connect([this, list_item](double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item) {
            send_update(item->m_name, item->m_value);
        }
    });
    slider->add_controller(dragGesture);

    auto clickGesture = Gtk::GestureClick::create();
    clickGesture->signal_released().connect([this, list_item](int, double, double) {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item) {
            send_update(item->m_name, item->m_value);
        }
    });
    slider->add_controller(clickGesture);

    // Signals for Entry
    entry->signal_activate().connect([this, slider, entry, list_item]() {
        auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
        if (item && item->m_hasRange) {
            try {
                double val = std::stod(entry->get_text());
                // Clamp
                if (val < item->m_rangeMin) val = item->m_rangeMin;
                if (val > item->m_rangeMax) val = item->m_rangeMax;
                
                slider->set_value(val);
                // Trigger update if slider didn't change (e.g. was already at clamped value)
                if (slider->get_value() == val) {
                    std::string valStr;
                    if (item->m_isFloat) {
                        std::stringstream ss;
                        ss << std::fixed << std::setprecision(2) << val;
                        valStr = ss.str();
                        valStr.erase(valStr.find_last_not_of('0') + 1, std::string::npos);
                        if (valStr.back() == '.') valStr.pop_back();
                    } else {
                        valStr = std::to_string((int)std::round(val));
                    }
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

void ConfigWindow::bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) {
        label->set_text(item->m_short_name);
    }
}

void ConfigWindow::bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto container = dynamic_cast<Gtk::Box*>(list_item->get_child());
    if (!item || !container) return;

    auto boolButton = dynamic_cast<Gtk::Button*>(container->get_first_child());
    auto choiceDropDown = dynamic_cast<Gtk::DropDown*>(boolButton->get_next_sibling());
    auto label = dynamic_cast<Gtk::EditableLabel*>(choiceDropDown->get_next_sibling());
    auto rangeBox = dynamic_cast<Gtk::Box*>(container->get_last_child());

    if (item->m_isBoolean) {
        boolButton->set_visible(true);
        choiceDropDown->set_visible(false);
        label->set_visible(false);
        rangeBox->set_visible(false);

        if (item->m_value == "1" || item->m_value == "true") {
            item->m_value = "true";
        } else if (item->m_value == "0" || item->m_value == "false") {
            item->m_value = "false";
        }
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
        m_binding_programmatically = true;
        choiceDropDown->set_model(model);
        choiceDropDown->set_selected(selected);
        m_binding_programmatically = false;
    } else if (item->m_hasRange) {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(false);
        label->set_visible(false);
        rangeBox->set_visible(true);
        
        auto slider = dynamic_cast<Gtk::Scale*>(rangeBox->get_first_child());
        auto entry = dynamic_cast<Gtk::Entry*>(rangeBox->get_last_child());
        
        slider->set_range(item->m_rangeMin, item->m_rangeMax);
        if (item->m_isFloat) {
            slider->set_increments(0.01, 0.1);
            slider->set_digits(2);
        } else {
            slider->set_increments(1.0, 10.0);
            slider->set_digits(0);
        }
        
        m_binding_programmatically = true;
        try {
            double val = std::stod(item->m_value);
            slider->set_value(val);
            entry->set_text(item->m_value);
        } catch (...) {
            slider->set_value(item->m_rangeMin);
            entry->set_text(std::to_string(item->m_rangeMin));
        }
        m_binding_programmatically = false;
    } else {
        boolButton->set_visible(false);
        choiceDropDown->set_visible(false);
        label->set_visible(true);
        rangeBox->set_visible(false);
        label->set_text(item->m_value);
    }
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
    const char* home = std::getenv("HOME");
    std::string configPath = home ? std::string(home) + "/.config/hypr/hyprland.conf" : "hyprland.conf";
    
    if (ConfigIO::updateOption(configPath, name, value)) {
        std::cout << "Updated config file: " << name << " = " << value << std::endl;
    } else {
        std::cerr << "Failed to update config file for: " << name << std::endl;
    }

    std::string escapedValue;
    for (char c : value) {
        if (c == '"') escapedValue += "\\\"";
        else escapedValue += c;
    }
    
    std::string cmd = "hyprctl keyword " + name + " \"" + escapedValue + "\"";
    std::cout << "Executing: " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to execute hyprctl command" << std::endl;
    }
}

void ConfigWindow::send_runtime_update(const std::string& name, const std::string& value) {
    std::string escapedValue;
    for (char c : value) {
        if (c == '"') escapedValue += "\\\"";
        else escapedValue += c;
    }
    
    std::string cmd = "hyprctl keyword " + name + " \"" + escapedValue + "\"";
    std::system(cmd.c_str());
}

void ConfigWindow::send_keyword_add(const std::string& type, const std::string& value) {
    std::string escapedValue;
    for (char c : value) {
        if (c == '"') escapedValue += "\\\"";
        else escapedValue += c;
    }
    
    std::string cmd = "hyprctl keyword " + type + " \"" + escapedValue + "\"";
    std::cout << "Executing: " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to execute hyprctl command" << std::endl;
    }
}

void ConfigWindow::send_device_config_add(const std::string& deviceName, const std::string& option, const std::string& value) {
    std::string escapedValue;
    for (char c : value) {
        if (c == '"') escapedValue += "\\\"";
        else escapedValue += c;
    }
    
    std::string cmd = "hyprctl keyword device:" + deviceName + ":" + option + " \"" + escapedValue + "\"";
    std::cout << "Executing: " << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to execute hyprctl command" << std::endl;
    }
}

void ConfigWindow::load_keywords() {
    m_KeywordStore->remove_all();
    m_ExecutingStore->remove_all();
    m_EnvVarStore->remove_all();
}

void ConfigWindow::load_device_configs() {
    m_DeviceConfigStore->remove_all();
}

void ConfigWindow::load_available_devices() {
    m_AvailableDevices.clear();
    
    std::string output;
    try {
        FILE* pipe = popen("hyprctl -j devices", "r");
        if (!pipe) return;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            output += buffer;
        }
        pclose(pipe);
    } catch (...) { return; }

    std::regex name_regex("\"name\":\\s*\"([^\"]+)\"");
    std::smatch match;
    std::string::const_iterator searchStart(output.cbegin());
    while (std::regex_search(searchStart, output.cend(), match, name_regex)) {
        m_AvailableDevices.push_back(match[1]);
        searchStart = match.suffix().first;
    }
}

void ConfigWindow::load_data() {
    // Unselect to avoid signals during clear
    m_TreeView.get_selection()->unselect_all();

    // Clear existing stores and tree
    for (auto& kv : m_SectionStores) {
        kv.second->remove_all();
    }
    m_SectionTreeStore->clear();
    m_SectionStores.clear();
    m_SectionIters.clear();
    m_SectionWidgets.clear();
    m_OrderedSections.clear();

    // Remove all children from content box
    while (auto child = m_ContentVBox.get_first_child()) {
        m_ContentVBox.remove(*child);
    }

    // Load available devices
    load_available_devices();

    // Create Variables parent section
    auto variablesIter = m_SectionTreeStore->append();
    (*variablesIter)[m_SectionColumns.m_col_name] = "Variables";
    (*variablesIter)[m_SectionColumns.m_col_full_path] = "__variables__";
    m_SectionIters["__variables__"] = variablesIter;

    // Add Keywords parent section
    auto keywordsIter = m_SectionTreeStore->append();
    (*keywordsIter)[m_SectionColumns.m_col_name] = "Keywords";
    (*keywordsIter)[m_SectionColumns.m_col_full_path] = "__keywords_parent__";
    m_SectionIters["__keywords_parent__"] = keywordsIter;

    // Add "Executing" subsection
    auto executingIter = m_SectionTreeStore->append(keywordsIter->children());
    (*executingIter)[m_SectionColumns.m_col_name] = "Executing";
    (*executingIter)[m_SectionColumns.m_col_full_path] = "__executing__";
    m_SectionIters["__executing__"] = executingIter;

    // Add "Per-device Input Configs" subsection
    auto deviceConfigsIter = m_SectionTreeStore->append(keywordsIter->children());
    (*deviceConfigsIter)[m_SectionColumns.m_col_name] = "Per-device Input Configs";
    (*deviceConfigsIter)[m_SectionColumns.m_col_full_path] = "__device_configs__";
    m_SectionIters["__device_configs__"] = deviceConfigsIter;

    // Add "Environment Variables" subsection
    auto envVarsIter = m_SectionTreeStore->append(keywordsIter->children());
    (*envVarsIter)[m_SectionColumns.m_col_name] = "Environment Variables";
    (*envVarsIter)[m_SectionColumns.m_col_full_path] = "__env_vars__";
    m_SectionIters["__env_vars__"] = envVarsIter;

    // --- Headers and Views Creation Order ---

    // 1. Variables Header
    auto varsHeader = Gtk::make_managed<Gtk::Label>("Variables");
    varsHeader->add_css_class("section-title"); // Use existing style or create a bigger one?
    // Make it look like a main category header
    varsHeader->set_margin_top(20);
    varsHeader->set_margin_bottom(10);
    varsHeader->set_halign(Gtk::Align::START);
    // Maybe make it larger via attributes if CSS isn't enough, but CSS is better.
    // Reusing section-title is fine, or add a new class.
    varsHeader->add_css_class("main-category-title"); 
    m_ContentVBox.append(*varsHeader);

    // Get config descriptions
    std::set<std::string> allSections;
    std::vector<std::tuple<std::string, std::string, std::string, bool, bool>> allOptions;
    bool hasRootOptions = false;

    FILE* pipe = popen("hyprctl descriptions -j", "r");
    if (pipe) {
        std::string jsonOutput;
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            jsonOutput += buffer;
        }
        pclose(pipe);

        GError* error = nullptr;
        JsonParser* parser = json_parser_new();
        
        if (json_parser_load_from_data(parser, jsonOutput.c_str(), -1, &error)) {
            JsonNode* root = json_parser_get_root(parser);
            if (root && JSON_NODE_HOLDS_ARRAY(root)) {
                JsonArray* array = json_node_get_array(root);
                guint length = json_array_get_length(array);

                for (guint i = 0; i < length; i++) {
                    JsonObject* obj = json_array_get_object_element(array, i);
                    
                    const char* name = json_object_get_string_member(obj, "value");
                    const char* desc = json_object_get_string_member(obj, "description");
                    
                    if (name && desc) {
                        std::string currentVal;
                        bool isSetByUser = false;
                        bool isBoolean = false;

                        if (json_object_has_member(obj, "type")) {
                            isBoolean = json_object_get_int_member(obj, "type") == 0;
                        }
                        
                        if (json_object_has_member(obj, "data")) {
                            JsonObject* dataObj = json_object_get_object_member(obj, "data");
                            if (json_object_has_member(dataObj, "current")) {
                                JsonNode* currentNode = json_object_get_member(dataObj, "current");
                                if (JSON_NODE_HOLDS_VALUE(currentNode)) {
                                    GType type = json_node_get_value_type(currentNode);
                                    if (type == G_TYPE_STRING) {
                                        currentVal = json_object_get_string_member(dataObj, "current");
                                    } else if (type == G_TYPE_INT64) {
                                        currentVal = std::to_string(json_object_get_int_member(dataObj, "current"));
                                    } else if (type == G_TYPE_DOUBLE) {
                                        currentVal = std::to_string(json_object_get_double_member(dataObj, "current"));
                                    } else if (type == G_TYPE_BOOLEAN) {
                                        currentVal = json_object_get_boolean_member(dataObj, "current") ? "true" : "false";
                                    }
                                }
                            } else if (json_object_has_member(dataObj, "value")) {
                                JsonNode* valueNode = json_object_get_member(dataObj, "value");
                                if (JSON_NODE_HOLDS_VALUE(valueNode)) {
                                    GType type = json_node_get_value_type(valueNode);
                                    if (type == G_TYPE_STRING) {
                                        currentVal = json_object_get_string_member(dataObj, "value");
                                    } else if (type == G_TYPE_INT64) {
                                        currentVal = std::to_string(json_object_get_int_member(dataObj, "value"));
                                    } else if (type == G_TYPE_DOUBLE) {
                                        currentVal = std::to_string(json_object_get_double_member(dataObj, "value"));
                                    } else if (type == G_TYPE_BOOLEAN) {
                                        currentVal = json_object_get_boolean_member(dataObj, "value") ? "true" : "false";
                                    }
                                }
                            }
                            if (json_object_has_member(dataObj, "explicit")) {
                                isSetByUser = json_object_get_boolean_member(dataObj, "explicit");
                            }
                        }

                        std::string sectionPath = get_section_path(name);
                        if (!sectionPath.empty()) {
                            allSections.insert(sectionPath);
                        } else {
                            hasRootOptions = true;
                        }
                        if (isBoolean) {
                            if (currentVal == "1" || currentVal == "true") {
                                currentVal = "true";
                            } else if (currentVal == "0" || currentVal == "false") {
                                currentVal = "false";
                            }
                        }
                        allOptions.emplace_back(name, currentVal, desc, isSetByUser, isBoolean);
                    }
                }
            }
        }
        g_object_unref(parser);
    }

    // Build the tree nodes (but don't create views yet)
    // Create sections in tree
    for (const auto& sectionPath : allSections) {
        if (sectionPath.empty()) continue; // Handle root separately or it might be handled here?

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
            }
            parentIter = m_SectionIters[currentPath];
        }
    }
    
    // Root level section node
    if (hasRootOptions && m_SectionIters.find("") == m_SectionIters.end()) {
        auto iter = m_SectionTreeStore->append(variablesIter->children());
        (*iter)[m_SectionColumns.m_col_name] = "(root)";
        (*iter)[m_SectionColumns.m_col_full_path] = "";
        m_SectionIters[""] = iter;
    }

    // 2. Create Views for Variables (root and others)
    // Create root view first if it exists or we just want it at top
    if (hasRootOptions && m_SectionWidgets.find("") == m_SectionWidgets.end()) {
         create_section_view("");
    }

    for (const auto& sectionPath : allSections) {
        if (sectionPath.empty()) continue; // Already created root
        if (m_SectionWidgets.find(sectionPath) == m_SectionWidgets.end()) {
            create_section_view(sectionPath);
        }
    }

    // 3. Keywords Header
    auto kwHeader = Gtk::make_managed<Gtk::Label>("Keywords");
    kwHeader->add_css_class("main-category-title");
    kwHeader->set_margin_top(40); // More spacing before this block
    kwHeader->set_margin_bottom(10);
    kwHeader->set_halign(Gtk::Align::START);
    m_ContentVBox.append(*kwHeader);

    // 4. Create Views for Keywords
    create_executing_view();
    create_device_configs_view();
    create_env_vars_view();
    
    load_keywords();
    load_device_configs();

    // Expand Keywords section
    m_TreeView.expand_row(Gtk::TreePath(keywordsIter), false);

    // Add options to stores
    for (const auto& [name, valStr, desc, setByUser, isBoolean] : allOptions) {
        std::string sectionPath = get_section_path(name);
        if (m_SectionStores.find(sectionPath) != m_SectionStores.end()) {
            m_SectionStores[sectionPath]->append(ConfigItem::create(name, valStr, desc, setByUser, isBoolean));
        }
    }

    // Expand Variables section
    m_TreeView.expand_row(Gtk::TreePath(variablesIter), false);
}

int main(int argc, char* argv[])
{
  auto app = Gtk::Application::create("org.hyprland.settings");
  return app->make_window_and_run<ConfigWindow>(argc, argv);
}
