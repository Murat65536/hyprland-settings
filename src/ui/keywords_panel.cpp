#include "ui/keywords_panel.hpp"

namespace ui {
KeywordsPanel::KeywordsPanel(
    Kind kind,
    const Glib::RefPtr<Gio::ListStore<KeywordItem>>& store,
    const std::function<void(const std::string&, const std::string&)>& on_add_keyword,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_keyword_type,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& setup_keyword_value,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_keyword_type,
    const sigc::slot<void(const Glib::RefPtr<Gtk::ListItem>&)>& bind_keyword_value) {
    m_root = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    m_root->set_spacing(10);

    if (kind == Kind::Executing) {
        auto label = Gtk::make_managed<Gtk::Label>("Executing (Runtime)");
        label->add_css_class("section-title");
        label->set_halign(Gtk::Align::START);
        m_root->append(*label);

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
        addButton->signal_clicked().connect([store, on_add_keyword, typeCombo, typeModel, valueEntry]() {
            auto selected = typeCombo->get_selected();
            if (selected == GTK_INVALID_LIST_POSITION) {
                return;
            }

            const std::string typeStr = typeModel->get_string(selected);
            const std::string value = valueEntry->get_text();
            if (value.empty()) {
                return;
            }

            on_add_keyword(typeStr, value);
            store->append(KeywordItem::create(typeStr, value));
            valueEntry->set_text("");
        });

        addBox->append(*typeCombo);
        addBox->append(*valueEntry);
        addBox->append(*addButton);
        addFrame->set_child(*addBox);
        m_root->append(*addFrame);
    } else {
        auto label = Gtk::make_managed<Gtk::Label>("Environment Variables");
        label->add_css_class("section-title");
        label->set_halign(Gtk::Align::START);
        m_root->append(*label);

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
        addButton->signal_clicked().connect([store, on_add_keyword, typeCombo, typeModel, nameEntry, valueEntry]() {
            auto selected = typeCombo->get_selected();
            if (selected == GTK_INVALID_LIST_POSITION) {
                return;
            }

            const std::string typeStr = typeModel->get_string(selected);
            const std::string name = nameEntry->get_text();
            const std::string value = valueEntry->get_text();
            if (name.empty()) {
                return;
            }

            const std::string fullVal = name + "," + value;
            on_add_keyword(typeStr, fullVal);
            store->append(KeywordItem::create(typeStr, fullVal));
            nameEntry->set_text("");
            valueEntry->set_text("");
        });

        addBox->append(*typeCombo);
        addBox->append(*nameEntry);
        addBox->append(*valueEntry);
        addBox->append(*addButton);
        addFrame->set_child(*addBox);
        m_root->append(*addFrame);
    }

    auto selectionModel = Gtk::SingleSelection::create(store);
    auto columnView = Gtk::make_managed<Gtk::ColumnView>();
    columnView->set_model(selectionModel);
    columnView->add_css_class("data-table");

    auto factory_type = Gtk::SignalListItemFactory::create();
    factory_type->signal_setup().connect(setup_keyword_type);
    factory_type->signal_bind().connect(bind_keyword_type);
    auto col_type = Gtk::ColumnViewColumn::create("Type", factory_type);
    col_type->set_fixed_width(kind == Kind::Executing ? 120 : 80);
    columnView->append_column(col_type);

    auto factory_value = Gtk::SignalListItemFactory::create();
    factory_value->signal_setup().connect(setup_keyword_value);
    factory_value->signal_bind().connect(bind_keyword_value);
    auto col_value = Gtk::ColumnViewColumn::create(
        kind == Kind::Executing ? "Command" : "Variable (Name,Value)",
        factory_value);
    col_value->set_expand(true);
    columnView->append_column(col_value);

    m_root->append(*columnView);
}

Gtk::Box* KeywordsPanel::widget() const {
    return m_root;
}
}  // namespace ui
