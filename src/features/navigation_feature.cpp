#include "features/navigation_feature.hpp"

namespace features {
void handle_section_selected(
    bool& selecting_programmatically,
    bool& scrolling_programmatically,
    Gtk::TreeView& tree_view,
    const Gtk::TreeModelColumn<Glib::ustring>& full_path_column,
    const std::map<std::string, Gtk::Widget*>& section_widgets,
    Gtk::Box& content_vbox,
    Gtk::ScrolledWindow& content_scroll) {
    if (selecting_programmatically) {
        return;
    }

    auto iter = tree_view.get_selection()->get_selected();
    if (!iter) {
        return;
    }

    Glib::ustring fullPath = (*iter)[full_path_column];
    auto sectionIt = section_widgets.find(fullPath.raw());
    if (sectionIt == section_widgets.end()) {
        return;
    }

    Gtk::Widget* target = sectionIt->second;
    double x;
    double y;
    if (target->translate_coordinates(content_vbox, 0, 0, x, y)) {
        scrolling_programmatically = true;
        content_scroll.get_vadjustment()->set_value(y);
        scrolling_programmatically = false;
    }
}

void handle_scroll_changed(
    bool& scrolling_programmatically,
    bool& selecting_programmatically,
    const std::vector<std::pair<std::string, Gtk::Widget*>>& ordered_sections,
    const std::map<std::string, Gtk::TreeModel::iterator>& section_iters,
    Gtk::TreeView& tree_view,
    const Glib::RefPtr<Gtk::TreeStore>& section_tree_store,
    Gtk::Box& content_vbox,
    Gtk::ScrolledWindow& content_scroll) {
    if (scrolling_programmatically || ordered_sections.empty()) {
        return;
    }

    const double scrollY = content_scroll.get_vadjustment()->get_value();
    std::string currentPath;

    for (const auto& pair : ordered_sections) {
        Gtk::Widget* widget = pair.second;
        double x;
        double y;
        if (widget->translate_coordinates(content_vbox, 0, 0, x, y)) {
            const double height = widget->get_height();
            if (y <= scrollY + 100 && (y + height) > scrollY + 50) {
                currentPath = pair.first;
                break;
            }
        }
    }

    if (currentPath.empty()) {
        return;
    }

    auto iterIt = section_iters.find(currentPath);
    if (iterIt == section_iters.end()) {
        return;
    }

    selecting_programmatically = true;
    tree_view.get_selection()->select(iterIt->second);
    tree_view.scroll_to_row(section_tree_store->get_path(iterIt->second));
    selecting_programmatically = false;
}
}  // namespace features
