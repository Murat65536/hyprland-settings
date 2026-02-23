#include "config_window.hpp"

#include "features/navigation_feature.hpp"
#include "ui/devices_panel.hpp"
#include "ui/keywords_panel.hpp"
#include "ui/variables_panel.hpp"

#include <filesystem>
#include <iostream>
#include <gtkmm/settings.h>

ConfigWindow::ConfigWindow()
: m_MainVBox(Gtk::Orientation::VERTICAL),
  m_MenuBox(Gtk::Orientation::VERTICAL),
  m_ContentBox(Gtk::Orientation::VERTICAL),
  m_HBox(Gtk::Orientation::HORIZONTAL),
  m_ContentVBox(Gtk::Orientation::VERTICAL)
{
    // Force Adwaita theme to avoid system theme interference
    auto settings = Gtk::Settings::get_default();
    if (settings) {
        settings->property_gtk_theme_name().set_value("Adwaita");
    }

    set_title("Hyprland Settings");
    set_default_size(950, 650);

    set_titlebar(m_HeaderBar);
    m_HeaderBar.set_show_title_buttons(true);

    auto* back_button = Gtk::make_managed<Gtk::Button>("Back");
    back_button->set_tooltip_text("Return to Main Menu");
    back_button->signal_clicked().connect([this]() {
        m_MainStack.set_visible_child("menu");
    });
    m_HeaderBar.pack_start(*back_button);

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
        std::cerr << "Warning: style.css not found! UI might look unstyled.\n";
    }

    auto display = Gdk::Display::get_default();
    if (display) {
        Gtk::StyleContext::add_provider_for_display(display, css_provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } else {
        std::cerr << "Warning: No default display found for CSS!\n";
    }

    m_MainStack.set_expand(true);
    m_MainStack.set_transition_type(Gtk::StackTransitionType::SLIDE_LEFT_RIGHT);
    m_MainVBox.append(m_MainStack);

    m_MenuBox.set_spacing(20);
    m_MenuBox.set_halign(Gtk::Align::CENTER);
    m_MenuBox.set_valign(Gtk::Align::CENTER);

    m_Button_Hyprland.set_label("Hyprland");
    m_Button_Hyprland.set_size_request(200, 200);
    m_Button_Hyprland.add_css_class("hyprland-button");
    m_Button_Hyprland.signal_clicked().connect(sigc::mem_fun(*this, &ConfigWindow::on_hyprland_button_clicked));

    m_MenuBox.append(m_Button_Hyprland);
    m_MainStack.add(m_MenuBox, "menu", "Main Menu");

    m_HBox.set_expand(true);
    m_ContentBox.append(m_HBox);

    m_SectionTreeStore = Gtk::TreeStore::create(m_SectionColumns);
    m_TreeView.set_model(m_SectionTreeStore);
    m_TreeView.append_column("Section", m_SectionColumns.m_col_name);
    m_TreeView.set_headers_visible(false);
    m_TreeView.get_selection()->signal_changed().connect(sigc::mem_fun(*this, &ConfigWindow::on_section_selected));
    m_TreeView.add_css_class("sidebar");

    auto sidebarScroll = Gtk::make_managed<Gtk::ScrolledWindow>();
    sidebarScroll->set_child(m_TreeView);
    sidebarScroll->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    sidebarScroll->set_size_request(240, -1);
    sidebarScroll->add_css_class("sidebar-scroll");
    m_HBox.append(*sidebarScroll);

    m_ContentScroll.set_expand(true);
    m_ContentScroll.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_ContentScroll.set_child(m_ContentVBox);
    m_ContentVBox.set_margin(20);
    m_ContentVBox.set_spacing(40);

    auto v_adj = m_ContentScroll.get_vadjustment();
    if (v_adj) {
        v_adj->signal_value_changed().connect(sigc::mem_fun(*this, &ConfigWindow::on_scroll_changed));
    }

    m_HBox.append(m_ContentScroll);
    m_MainStack.add(m_ContentBox, "content", "Settings");
    m_StatusLabel.set_halign(Gtk::Align::START);
    m_StatusLabel.set_margin_start(12);
    m_StatusLabel.set_margin_end(12);
    m_StatusLabel.set_margin_bottom(8);
    m_StatusLabel.set_text("Ready");
    m_MainVBox.append(m_StatusLabel);

    m_ExecutingStore = Gio::ListStore<KeywordItem>::create();
    m_EnvVarStore = Gio::ListStore<KeywordItem>::create();
    m_DeviceConfigStore = Gio::ListStore<DeviceConfigItem>::create();

    m_MainStack.set_visible_child("menu");
}

void ConfigWindow::on_hyprland_button_clicked() {
    load_data();
    m_MainStack.set_visible_child("content");
}

void ConfigWindow::on_section_selected() {
    features::handle_section_selected(
        m_selecting_programmatically,
        m_scrolling_programmatically,
        m_TreeView,
        m_SectionColumns.m_col_full_path,
        m_SectionWidgets,
        m_ContentVBox,
        m_ContentScroll);
}

void ConfigWindow::on_scroll_changed() {
    features::handle_scroll_changed(
        m_scrolling_programmatically,
        m_selecting_programmatically,
        m_OrderedSections,
        m_SectionIters,
        m_TreeView,
        m_SectionTreeStore,
        m_ContentVBox,
        m_ContentScroll);
}

void ConfigWindow::on_button_refresh() {
    load_data();
}

void ConfigWindow::set_status_message(const std::string& text, bool is_error) {
    m_StatusLabel.set_text(text);
    if (is_error) {
        m_StatusLabel.remove_css_class("status-ok");
        m_StatusLabel.add_css_class("status-error");
    } else {
        m_StatusLabel.remove_css_class("status-error");
        m_StatusLabel.add_css_class("status-ok");
    }
}
