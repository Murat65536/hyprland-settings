#ifndef FEATURES_NAVIGATION_FEATURE_HPP
#define FEATURES_NAVIGATION_FEATURE_HPP

#include <gtkmm.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace features {
void handle_section_selected(
    bool& selecting_programmatically,
    bool& scrolling_programmatically,
    Gtk::TreeView& tree_view,
    const Gtk::TreeModelColumn<Glib::ustring>& full_path_column,
    const std::map<std::string, Gtk::Widget*>& section_widgets,
    Gtk::Box& content_vbox,
    Gtk::ScrolledWindow& content_scroll);

void handle_scroll_changed(
    bool& scrolling_programmatically,
    bool& selecting_programmatically,
    const std::vector<std::pair<std::string, Gtk::Widget*>>& ordered_sections,
    const std::map<std::string, Gtk::TreeModel::iterator>& section_iters,
    Gtk::TreeView& tree_view,
    const Glib::RefPtr<Gtk::TreeStore>& section_tree_store,
    Gtk::Box& content_vbox,
    Gtk::ScrolledWindow& content_scroll);
}

#endif
