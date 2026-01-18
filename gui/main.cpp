#include <gtkmm.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
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

    Gtk::Box m_MainVBox;
    Gtk::Box m_HBox;
    Gtk::StackSidebar m_StackSidebar;
    Gtk::Stack m_Stack;
    Gtk::Button m_Button_Refresh;

    // Let's define the custom object for the model properly.
    class ConfigItem : public Glib::Object {
    public:
        std::string m_name;
        std::string m_short_name;
        std::string m_value;
        std::string m_desc;
        bool m_setByUser = false;  // Whether this was explicitly set in config

        static Glib::RefPtr<ConfigItem> create(const std::string& name, const std::string& value, const std::string& desc, bool setByUser) {
            return Glib::make_refptr_for_instance<ConfigItem>(new ConfigItem(name, value, desc, setByUser));
        }

    protected:
        ConfigItem(const std::string& name, const std::string& value, const std::string& desc, bool setByUser) 
            : m_name(name), m_value(value), m_desc(desc), m_setByUser(setByUser) {
            size_t pos = name.find(':');
            if (pos != std::string::npos) {
                m_short_name = name.substr(pos + 1);
            } else {
                m_short_name = name;
            }
        }
    };

    std::map<std::string, Glib::RefPtr<Gio::ListStore<ConfigItem>>> m_SectionStores;

    void setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    void load_data();
    void send_update(const std::string& name, const std::string& value);
    void create_section_view(const std::string& sectionName);
};

ConfigWindow::ConfigWindow()
: m_MainVBox(Gtk::Orientation::VERTICAL),
  m_HBox(Gtk::Orientation::HORIZONTAL),
  m_Button_Refresh("Refresh Options")
{
    set_title("Hyprland Settings Viewer");
    set_default_size(900, 600);

    m_MainVBox.set_margin(10);
    set_child(m_MainVBox);

    // Refresh Button
    m_Button_Refresh.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_button_refresh));
    m_MainVBox.append(m_Button_Refresh);

    // HBox for Sidebar + Stack
    m_HBox.set_expand(true);
    m_HBox.set_margin_top(10);
    m_MainVBox.append(m_HBox);

    // Sidebar
    m_StackSidebar.set_stack(m_Stack);
    m_StackSidebar.set_size_request(150, -1);
    
    // Wrap sidebar in a scrolled window just in case
    auto sidebarScroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    sidebarScroll->set_child(m_StackSidebar);
    sidebarScroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    m_HBox.append(*sidebarScroll);

    // Stack
    m_Stack.set_expand(true);
    m_Stack.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
    m_HBox.append(m_Stack);

    load_data();
}

ConfigWindow::~ConfigWindow() {}

void ConfigWindow::create_section_view(const std::string& sectionName) {
    auto listStore = Gio::ListStore<ConfigItem>::create();
    m_SectionStores[sectionName] = listStore;

    auto selectionModel = Gtk::SingleSelection::create(listStore);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    // Columns
    auto factory_name = Gtk::SignalListItemFactory::create();
    factory_name->signal_setup().connect(sigc::mem_fun(*this, &ConfigWindow::setup_column_read));
    factory_name->signal_bind().connect(sigc::mem_fun(*this, &ConfigWindow::bind_name));
    auto col_name = Gtk::ColumnViewColumn::create("Option", factory_name);
    col_name->set_expand(true);
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
    
    m_Stack.add(*scrolledWindow, sectionName, sectionName);
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

    // Use SET_OPTION_PERSIST to write to config file
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

void ConfigWindow::load_data() {
    // Clear existing stores
    for (auto& kv : m_SectionStores) {
        kv.second->remove_all();
    }

    int sock = 0;
    struct sockaddr_un serv_addr;
    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        // Fallback if socket fails: show error in a "General" or "Error" tab?
        // For simplicity, just return or print to stderr.
        std::cerr << "Socket creation error" << std::endl;
        return;
    }

    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, IPC::SOCKET_PATH, sizeof(serv_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        // Handle connection error
        std::cerr << "Connection failed" << std::endl;
        ::close(sock);
        return;
    }

    // Send GET request
    IPC::RequestType req = IPC::RequestType::GET_ALL;
    write(sock, &req, sizeof(req));

    uint32_t count = 0;
    readData(sock, &count, sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        std::string name = readString(sock);
        std::string desc = readString(sock);
        std::string valStr = readString(sock);
        
        // Read m_bSetByUser flag
        uint8_t setByUser = 0;
        readData(sock, &setByUser, sizeof(setByUser));
        
        // Determine section
        std::string section = "";
        size_t pos = name.find(':');
        if (pos != std::string::npos) {
            section = name.substr(0, pos);
        }

        // Create section if not exists
        if (m_SectionStores.find(section) == m_SectionStores.end()) {
            create_section_view(section);
        }

        m_SectionStores[section]->append(ConfigItem::create(name, valStr, desc, setByUser != 0));
    }
    ::close(sock);
}

int main(int argc, char* argv[])
{
  auto app = Gtk::Application::create("org.hyprland.settings");
  return app->make_window_and_run<ConfigWindow>(argc, argv);
}
