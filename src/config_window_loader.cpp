#include "config_window.hpp"

#include "ui/devices_panel.hpp"
#include "ui/keywords_panel.hpp"
#include "ui/variables_panel.hpp"

#include <set>
#include <sstream>

void ConfigWindow::load_data() {
    m_TreeView.get_selection()->unselect_all();
    m_OptionValues.clear();

    for (auto& kv : m_SectionStores) {
        kv.second->remove_all();
    }
    m_SectionTreeStore->clear();
    m_SectionStores.clear();
    m_SectionIters.clear();
    m_SectionWidgets.clear();
    m_OrderedSections.clear();
    m_VariablesPanels.clear();
    m_ExecutingPanel.reset();
    m_EnvVarsPanel.reset();
    m_DevicesPanel.reset();

    while (auto child = m_ContentVBox.get_first_child()) {
        m_ContentVBox.remove(*child);
    }

    SettingsSnapshot snapshot = m_SettingsController.load_snapshot();
    m_AvailableDevices = snapshot.available_devices;
    m_AvailableDeviceOptions.clear();

    std::set<std::string> uniqueDeviceOptions;
    for (const auto& option : snapshot.options) {
        if (option.name.empty()) {
            continue;
        }

        size_t pos = option.name.rfind(':');
        std::string shortName = (pos == std::string::npos) ? option.name : option.name.substr(pos + 1);
        if (!shortName.empty()) {
            uniqueDeviceOptions.insert(shortName);
        }
    }
    m_AvailableDeviceOptions.assign(uniqueDeviceOptions.begin(), uniqueDeviceOptions.end());

    auto variablesIter = m_SectionTreeStore->append();
    (*variablesIter)[m_SectionColumns.m_col_name] = "Variables";
    (*variablesIter)[m_SectionColumns.m_col_full_path] = "__variables__";
    m_SectionIters["__variables__"] = variablesIter;

    auto keywordsIter = m_SectionTreeStore->append();
    (*keywordsIter)[m_SectionColumns.m_col_name] = "Keywords";
    (*keywordsIter)[m_SectionColumns.m_col_full_path] = "__keywords_parent__";
    m_SectionIters["__keywords_parent__"] = keywordsIter;

    auto executingIter = m_SectionTreeStore->append(keywordsIter->children());
    (*executingIter)[m_SectionColumns.m_col_name] = "Executing";
    (*executingIter)[m_SectionColumns.m_col_full_path] = "__executing__";
    m_SectionIters["__executing__"] = executingIter;

    auto deviceConfigsIter = m_SectionTreeStore->append(keywordsIter->children());
    (*deviceConfigsIter)[m_SectionColumns.m_col_name] = "Per-device Input Configs";
    (*deviceConfigsIter)[m_SectionColumns.m_col_full_path] = "__device_configs__";
    m_SectionIters["__device_configs__"] = deviceConfigsIter;

    auto envVarsIter = m_SectionTreeStore->append(keywordsIter->children());
    (*envVarsIter)[m_SectionColumns.m_col_name] = "Environment Variables";
    (*envVarsIter)[m_SectionColumns.m_col_full_path] = "__env_vars__";
    m_SectionIters["__env_vars__"] = envVarsIter;

    auto varsHeader = Gtk::make_managed<Gtk::Label>("Variables");
    varsHeader->add_css_class("section-title");
    varsHeader->set_margin_top(20);
    varsHeader->set_margin_bottom(10);
    varsHeader->set_halign(Gtk::Align::START);
    varsHeader->add_css_class("main-category-title");
    m_ContentVBox.append(*varsHeader);

    for (const auto& sectionPath : snapshot.sections) {
        if (sectionPath.empty()) continue;

        std::stringstream ss(sectionPath);
        std::string part;
        std::string currentPath;
        Gtk::TreeModel::iterator parentIter = variablesIter;

        while (std::getline(ss, part, ':')) {
            if (part.empty()) continue;
            if (!currentPath.empty()) currentPath += ":";
            currentPath += part;

            if (m_SectionIters.find(currentPath) == m_SectionIters.end()) {
                auto newIter = m_SectionTreeStore->append(parentIter->children());
                (*newIter)[m_SectionColumns.m_col_name] = part;
                (*newIter)[m_SectionColumns.m_col_full_path] = currentPath;
                m_SectionIters[currentPath] = newIter;
            }
            parentIter = m_SectionIters[currentPath];
        }
    }

    if (snapshot.has_root_options && m_SectionIters.find("") == m_SectionIters.end()) {
        auto iter = m_SectionTreeStore->append(variablesIter->children());
        (*iter)[m_SectionColumns.m_col_name] = "(root)";
        (*iter)[m_SectionColumns.m_col_full_path] = "";
        m_SectionIters[""] = iter;
    }

    if (snapshot.has_root_options && m_SectionWidgets.find("") == m_SectionWidgets.end()) {
        create_section_view("");
    }

    for (const auto& sectionPath : snapshot.sections) {
        if (sectionPath.empty()) continue;
        if (m_SectionWidgets.find(sectionPath) == m_SectionWidgets.end()) {
            create_section_view(sectionPath);
        }
    }

    auto kwHeader = Gtk::make_managed<Gtk::Label>("Keywords");
    kwHeader->add_css_class("main-category-title");
    kwHeader->set_margin_top(40);
    kwHeader->set_margin_bottom(10);
    kwHeader->set_halign(Gtk::Align::START);
    m_ContentVBox.append(*kwHeader);

    create_executing_view();
    create_device_configs_view();
    create_env_vars_view();

    m_ExecutingStore->remove_all();
    m_EnvVarStore->remove_all();
    m_DeviceConfigStore->remove_all();

    m_TreeView.expand_row(Gtk::TreePath(keywordsIter), false);

    for (const auto& option : snapshot.options) {
        m_OptionValues[option.name] = option.value;
        auto it = m_SectionStores.find(option.section_path);
        if (it != m_SectionStores.end()) {
            it->second->append(ConfigItem::create(option.name, option.value, option.description,
                                                  option.set_by_user, option.value_type,
                                                  option.choice_values_csv,
                                                  option.has_range, option.range_min,
                                                  option.range_max));
        }
    }

    m_TreeView.expand_row(Gtk::TreePath(variablesIter), false);
}
