#ifndef UI_KEYWORDS_PANEL_HPP
#define UI_KEYWORDS_PANEL_HPP

#include "ui/item_models.hpp"

#include <gtkmm.h>

#include <functional>
#include <string>

namespace ui {
class KeywordsPanel {
public:
    enum class Kind {
        Executing,
        EnvironmentVariables,
    };

    KeywordsPanel(Kind kind,
                  const Glib::RefPtr<Gio::ListStore<KeywordItem>>& store,
                  const std::function<void(const std::string&, const std::string&)>& on_add_keyword,
                  const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_keyword_type,
                  const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_keyword_value,
                  const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_keyword_type,
                  const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_keyword_value);

    Gtk::Box* widget() const;

private:
    Gtk::Box* m_root = nullptr;
};
}  // namespace ui

#endif
