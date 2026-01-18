#include <gtkmm.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "../ipc_common.hpp"

// Simplified approach using ColumnView for easier multi-column display
class ConfigWindow : public Gtk::Window
{
public:
    ConfigWindow();
    virtual ~ConfigWindow();

protected:
    void on_button_refresh();

    Gtk::Box m_VBox;
    Gtk::ScrolledWindow m_ScrolledWindow;
    Gtk::ColumnView m_ColumnView;
    Gtk::Button m_Button_Refresh;

    // Let's define the custom object for the model properly.
    class ConfigItem : public Glib::Object {
    public:
        std::string m_name;
        std::string m_value;
        std::string m_desc;

        static Glib::RefPtr<ConfigItem> create(const std::string& name, const std::string& value, const std::string& desc) {
            return Glib::make_refptr_for_instance<ConfigItem>(new ConfigItem(name, value, desc));
        }

    protected:
        ConfigItem(const std::string& name, const std::string& value, const std::string& desc) 
            : m_name(name), m_value(value), m_desc(desc) {}
    };

    Glib::RefPtr<Gio::ListStore<ConfigItem>> m_ListStore;
    Glib::RefPtr<Gtk::SingleSelection> m_SelectionModel;

    void setup_column(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_desc(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    void load_data();
};

ConfigWindow::ConfigWindow()
: m_VBox(Gtk::Orientation::VERTICAL),
  m_Button_Refresh("Refresh Options")
{
    set_title("Hyprland Settings Viewer");
    set_default_size(800, 600);

    m_VBox.set_margin(10);
    set_child(m_VBox);

    // Refresh Button
    m_Button_Refresh.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_button_refresh));
    m_VBox.append(m_Button_Refresh);

    // Scrolled Window
    m_ScrolledWindow.set_expand(true);
    m_VBox.append(m_ScrolledWindow);

    // Create Model
    m_ListStore = Gio::ListStore<ConfigItem>::create();
    m_SelectionModel = Gtk::SingleSelection::create(m_ListStore);

    // Create ColumnView
    m_ColumnView.set_model(m_SelectionModel);
    m_ColumnView.add_css_class("data-table");
    
    // Columns
    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column));
    factory_name->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_name));
    auto col_name = Gtk::ColumnViewColumn::create("Option", factory_name);
    col_name->set_expand(true);
    m_ColumnView.append_column(col_name);

    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column));
    factory_value->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_value));
    auto col_val = Gtk::ColumnViewColumn::create("Value", factory_value);
    col_val->set_expand(true);
    m_ColumnView.append_column(col_val);

    auto factory_desc = Gtk::SignalListItemFactory::create();
    factory_desc->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column));
    factory_desc->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_desc));
    auto col_desc = Gtk::ColumnViewColumn::create("Description", factory_desc);
    col_desc->set_expand(true);
    m_ColumnView.append_column(col_desc);

    m_ScrolledWindow.set_child(m_ColumnView);

    load_data();
}

ConfigWindow::~ConfigWindow() {}

void ConfigWindow::setup_column(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto label = Gtk::make_managed<Gtk::Label>();
    label->set_halign(Gtk::Align::START);
    label->set_ellipsize(Pango::EllipsizeMode::END);
    list_item->set_child(*label);
}

void ConfigWindow::bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) label->set_text(item->m_name);
}

void ConfigWindow::bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) label->set_text(item->m_value);
}

void ConfigWindow::bind_desc(const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto item = std::dynamic_pointer_cast<ConfigItem>(list_item->get_item());
    auto label = dynamic_cast<Gtk::Label*>(list_item->get_child());
    if (item && label) label->set_text(item->m_desc);
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

void ConfigWindow::load_data() {
    m_ListStore->remove_all();

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        m_ListStore->append(ConfigItem::create("Error", "Socket creation error", ""));
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        m_ListStore->append(ConfigItem::create("Error", "Connection failed", "Ensure plugin is loaded"));
        ::close(sock);
        return;
    }

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        IPC::OptionType type;
        readData(sock, &type, sizeof(type));

        std::string name = readString(sock);
        std::string desc = readString(sock);
        std::string valStr = "";

        if (type == IPC::OptionType::INT) {
            int64_t val;
            readData(sock, &val, sizeof(val));
            valStr = std::to_string(val);
        } else if (type == IPC::OptionType::FLOAT) {
            double val;
            readData(sock, &val, sizeof(val));
            valStr = std::to_string(val);
        } else if (type == IPC::OptionType::STRING) {
            valStr = readString(sock);
        } else if (type == IPC::OptionType::VEC2) {
            double vec[2];
            readData(sock, vec, sizeof(vec));
            std::stringstream ss;
            ss << "[" << vec[0] << ", " << vec[1] << "]";
            valStr = ss.str();
        } else {
            valStr = "Unknown Type";
        }
        
        m_ListStore->append(ConfigItem::create(name, valStr, desc));
    }
    ::close(sock);
}

int main(int argc, char* argv[])
{
  auto app = Gtk::Application::create("org.hyprland.settings");
  return app->make_window_and_run<ConfigWindow>(argc, argv);
}