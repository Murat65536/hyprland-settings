#ifndef CONFIG_WINDOW_HPP
#define CONFIG_WINDOW_HPP

#include "features/settings_controller.hpp"
#include "ui/item_models.hpp"

#include <gtkmm.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ui {
class VariablesPanel;
class KeywordsPanel;
class DevicesPanel;
}

class ConfigWindow : public Gtk::Window
{
public:
    using ConfigItem = ui::ConfigItem;
    using KeywordItem = ui::KeywordItem;
    using DeviceConfigItem = ui::DeviceConfigItem;

    class SectionColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        SectionColumns() { add(m_col_name); add(m_col_full_path); }
        Gtk::TreeModelColumn<Glib::ustring> m_col_name;
        Gtk::TreeModelColumn<Glib::ustring> m_col_full_path;
    };

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
    Gtk::Label m_StatusLabel;
    Gtk::Button m_Button_Refresh;
    Gtk::Button m_Button_Hyprland;

    SectionColumns m_SectionColumns;
    Glib::RefPtr<Gtk::TreeStore> m_SectionTreeStore;

    std::map<std::string, Glib::RefPtr<Gio::ListStore<ConfigItem>>> m_SectionStores;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_ExecutingStore;
    Glib::RefPtr<Gio::ListStore<KeywordItem>> m_EnvVarStore;
    Glib::RefPtr<Gio::ListStore<DeviceConfigItem>> m_DeviceConfigStore;
    std::vector<std::unique_ptr<ui::VariablesPanel>> m_VariablesPanels;
    std::unique_ptr<ui::KeywordsPanel> m_ExecutingPanel;
    std::unique_ptr<ui::KeywordsPanel> m_EnvVarsPanel;
    std::unique_ptr<ui::DevicesPanel> m_DevicesPanel;
    std::vector<std::string> m_AvailableDevices;
    std::vector<std::string> m_AvailableDeviceOptions;
    SettingsController m_SettingsController;
    std::map<std::string, Gtk::TreeModel::iterator> m_SectionIters;

    std::map<std::string, Gtk::Widget*> m_SectionWidgets;
    std::vector<std::pair<std::string, Gtk::Widget*>> m_OrderedSections;
    bool m_scrolling_programmatically = false;
    bool m_selecting_programmatically = false;
    bool m_binding_programmatically = false;

    void setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    void setup_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_type(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_keyword_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    void setup_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_option(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_device_value(const Glib::RefPtr<Gtk::ListItem>& list_item);

    void load_data();
    void send_update(const std::string& name, const std::string& value);
    void send_runtime_update(const std::string& name, const std::string& value);
    void send_keyword_add(const std::string& type, const std::string& value);
    void send_device_config_add(const std::string& deviceName, const std::string& option,
                                const std::string& value);
    void set_status_message(const std::string& text, bool is_error);
    void create_section_view(const std::string& sectionPath);
    void create_executing_view();
    void create_device_configs_view();
    void create_env_vars_view();
    void on_section_selected();
};

#endif
