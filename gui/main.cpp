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

    std::map<std::string, Glib::RefPtr<Gio::ListStore<ConfigItem>>> m_SectionStores;
    SectionNode m_RootSection;
    std::map<std::string, Gtk::TreeModel::iterator> m_SectionIters;

    void setup_column_read(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void setup_column_edit(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_name(const Glib::RefPtr<Gtk::ListItem>& list_item);
    void bind_value(const Glib::RefPtr<Gtk::ListItem>& list_item);
    
    void load_data();
    void send_update(const std::string& name, const std::string& value);
    void create_section_view(const std::string& sectionPath);
    void on_section_selected();
    void add_section_to_tree(const std::string& sectionPath);
    std::string get_section_path(const std::string& optionName);
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

    // Setup TreeView for nested sections
    m_SectionTreeStore = Gtk::TreeStore::create(m_SectionColumns);
    m_TreeView.set_model(m_SectionTreeStore);
    m_TreeView.append_column("Section", m_SectionColumns.m_col_name);
    m_TreeView.set_headers_visible(false);
    m_TreeView.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &ConfigWindow::on_section_selected));
    
    // Wrap sidebar in a scrolled window
    auto sidebarScroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    sidebarScroll->set_child(m_TreeView);
    sidebarScroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    sidebarScroll->set_size_request(200, -1);
    m_HBox.append(*sidebarScroll);

    // Stack
    m_Stack.set_expand(true);
    m_Stack.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
    m_HBox.append(m_Stack);

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
    
    m_Stack.add(*scrolledWindow, sectionPath, sectionPath);
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

    // Create all section views in the tree (sorted order for consistent display)
    for (const auto& sectionPath : allSections) {
        add_section_to_tree(sectionPath);
    }
    
    // Expand all rows in the tree for visibility
    m_TreeView.expand_all();

    // Second pass: add options to their sections
    for (const auto& [name, valStr, desc, setByUser] : allOptions) {
        std::string sectionPath = get_section_path(name);
        
        if (sectionPath.empty()) {
            // Root level option - create a special root section if needed
            if (m_SectionStores.find("") == m_SectionStores.end()) {
                auto iter = m_SectionTreeStore->prepend();
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
}

int main(int argc, char* argv[])
{
  auto app = Gtk::Application::create("org.hyprland.settings");
  return app->make_window_and_run<ConfigWindow>(argc, argv);
}
