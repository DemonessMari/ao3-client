#include "searchfilters.h"
#include "taglineedit.h"
#include "filtercatalogue.h"
#include <QSettings>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QHBoxLayout>

SearchFilterPanel::SearchFilterPanel(QWidget *parent) : QWidget(parent)
{
    setMinimumWidth(280);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("filterTabs"));
    layout->addWidget(tabs, 1);
    auto makePage = [&](const QString &title) {
        auto *scroll = new QScrollArea(tabs);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *page = new QWidget;
        auto *form = new QFormLayout(page);
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setVerticalSpacing(8);
        scroll->setWidget(page);
        tabs->addTab(scroll, title);
        return form;
    };
    auto *workForm = makePage(QStringLiteral("Work"));
    auto *tagForm = makePage(QStringLiteral("Tags"));
    auto *statsForm = makePage(QStringLiteral("Stats"));
    auto *excludeForm = makePage(QStringLiteral("Exclude"));
    auto *form = workForm;
    auto *body = this;
    auto text = [&](const QString &key, const QString &label, const QString &hint = {}) {
        const QMap<QString, QString> types = {
            {QStringLiteral("fandom_names"), QStringLiteral("fandom")},
            {QStringLiteral("character_names"), QStringLiteral("character")},
            {QStringLiteral("relationship_names"), QStringLiteral("relationship")},
            {QStringLiteral("freeform_names"), QStringLiteral("freeform")},
            {QStringLiteral("excluded_tag_names"), QStringLiteral("tag")}};
        QLineEdit *edit;
        if (types.contains(key)) {
            auto *tag = new TagLineEdit(types.value(key), body);
            connect(tag, &TagLineEdit::suggestionsRequested, this, [this, key](const QString &type, const QString &term, quint64 revision) {
                emit tagSuggestionsRequested(key, type, term, revision);
            });
            edit = tag;
        } else edit = new QLineEdit(body);
        edit->setObjectName(key); edit->setPlaceholderText(hint); edit->setMinimumHeight(32);
        m_text[key] = edit; form->addRow(label, edit);
    };
    text(QStringLiteral("title"), QStringLiteral("Title"));
    text(QStringLiteral("creators"), QStringLiteral("Author"));
    text(QStringLiteral("revised_at"), QStringLiteral("Date updated"), QStringLiteral("e.g. >2025-01-01 or 1 week ago"));
    form = tagForm;
    text(QStringLiteral("fandom_names"), QStringLiteral("Fandoms"), QStringLiteral("Comma-separated fandom names"));
    text(QStringLiteral("character_names"), QStringLiteral("Characters"), QStringLiteral("Comma-separated names"));
    text(QStringLiteral("relationship_names"), QStringLiteral("Relationships"), QStringLiteral("Comma-separated relationships"));
    text(QStringLiteral("freeform_names"), QStringLiteral("Additional tags"), QStringLiteral("Comma-separated tags"));
    form = excludeForm;
    text(QStringLiteral("excluded_tag_names"), QStringLiteral("Exclude tags"), QStringLiteral("Comma-separated tags to exclude"));
    form = statsForm;
    text(QStringLiteral("word_count"), QStringLiteral("Word count"), QStringLiteral("e.g. 1000-50000 or >10000"));
    text(QStringLiteral("kudos_count"), QStringLiteral("Kudos"), QStringLiteral("e.g. >100 or 100-1000"));
    text(QStringLiteral("hits"), QStringLiteral("Hits"), QStringLiteral("e.g. >1000"));
    text(QStringLiteral("comments_count"), QStringLiteral("Comments"), QStringLiteral("e.g. >10 or 10-100"));
    text(QStringLiteral("bookmarks_count"), QStringLiteral("Bookmarks"), QStringLiteral("e.g. >50 or 50-500"));
    form = workForm;
    auto combo = [&](const QString &key, const QString &label, const QVector<QPair<QString, QString>> &choices) {
        auto *box = new QComboBox(body); box->setObjectName(key); box->setMinimumHeight(32);
        box->addItem(QStringLiteral("Any / default"), QString());
        for (const auto &choice : choices) box->addItem(choice.first, choice.second);
        m_combos[key] = box; form->addRow(label, box);
    };
    combo(QStringLiteral("complete"), QStringLiteral("Completion"), {{QStringLiteral("Complete only"), "T"}, {QStringLiteral("In progress only"), "F"}});
    combo(QStringLiteral("crossover"), QStringLiteral("Crossovers"), {{QStringLiteral("Exclude crossovers"), "F"}, {QStringLiteral("Only crossovers"), "T"}});
    combo(QStringLiteral("single_chapter"), QStringLiteral("Chapter count"), {{QStringLiteral("Only single-chapter works"), "1"}, {QStringLiteral("Exclude single-chapter works"), "exclude"}});
    form = tagForm;
    combo(QStringLiteral("rating_ids"), QStringLiteral("Rating"), {});
    form = workForm;
    combo(QStringLiteral("language_id"), QStringLiteral("Language"), {});
    m_combos[QStringLiteral("language_id")]->setEditable(true);
    m_combos[QStringLiteral("language_id")]->setInsertPolicy(QComboBox::NoInsert);
    m_combos[QStringLiteral("language_id")]->setToolTip(QStringLiteral("Select a language or enter its AO3 language code."));
    combo(QStringLiteral("sort_column"), QStringLiteral("Sort by"), {
        {QStringLiteral("Best match"), "_score"}, {QStringLiteral("Author"), "authors_to_sort_on"},
        {QStringLiteral("Title"), "title_to_sort_on"}, {QStringLiteral("Date posted"), "created_at"},
        {QStringLiteral("Date updated"), "revised_at"}, {QStringLiteral("Word count"), "word_count"},
        {QStringLiteral("Hits"), "hits"}, {QStringLiteral("Kudos"), "kudos_count"},
        {QStringLiteral("Comments"), "comments_count"}, {QStringLiteral("Bookmarks"), "bookmarks_count"}});
    combo(QStringLiteral("sort_direction"), QStringLiteral("Sort direction"), {{QStringLiteral("Descending"), "desc"}, {QStringLiteral("Ascending"), "asc"}});
    form = tagForm;
    for (const auto &key : {QStringLiteral("archive_warning_ids[]"), QStringLiteral("category_ids[]")}) {
        auto *list = new QListWidget(body); list->setObjectName(key); list->setMinimumHeight(150);
        list->setWordWrap(true);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_lists[key] = list;
        form->addRow(key.startsWith("archive") ? QStringLiteral("Include warnings") : QStringLiteral("Include categories"), list);
    }

    for (const auto &entry : QVector<QPair<QString, QString>>{
        {QStringLiteral("excluded_rating_ids[]"), QStringLiteral("Exclude ratings")},
        {QStringLiteral("excluded_warning_ids[]"), QStringLiteral("Exclude warnings")},
        {QStringLiteral("excluded_category_ids[]"), QStringLiteral("Exclude categories")}}) {
        auto *list = new QListWidget(this);
        list->setObjectName(entry.first);
        list->setMinimumHeight(150);
        list->setWordWrap(true);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_lists[entry.first] = list;
        excludeForm->addRow(entry.second, list);
    }
    auto *buttons = new QHBoxLayout;
    auto *reset = new QPushButton(QStringLiteral("Reset"), this);
    reset->setObjectName(QStringLiteral("resetFilters"));
    auto *apply = new QPushButton(QStringLiteral("Apply and search"), this);
    apply->setObjectName(QStringLiteral("applyFilters"));
    buttons->addWidget(reset); buttons->addWidget(apply, 1);
    layout->addLayout(buttons);
    connect(apply, &QPushButton::clicked, this, &SearchFilterPanel::searchRequested);
    connect(reset, &QPushButton::clicked, this, [this] { setFilters({}); emit searchRequested(); });
    for (auto *edit : m_text) connect(edit, &QLineEdit::returnPressed, this, &SearchFilterPanel::searchRequested);
    m_catalogue = standardFilterChoices();
    QSettings settings;
    for (const auto &key : m_catalogue.choices.keys()) {
        const auto cached = settings.value(QStringLiteral("filters/catalogue/") + key).toList();
        for (const auto &value : cached) {
            const auto choice = value.toStringList();
            if (choice.size() != 2 || choice[0].isEmpty() || choice[1].isEmpty()) continue;
            bool found = false;
            for (auto &entry : m_catalogue.choices[key]) if (entry.second == choice[1]) { entry.first = choice[0]; found = true; break; }
            if (!found) m_catalogue.choices[key].append({choice[0], choice[1]});
        }
    }
    setOptions(m_catalogue);
}

SearchFilters SearchFilterPanel::filters() const
{
    SearchFilters result;
    for (auto it = m_text.cbegin(); it != m_text.cend(); ++it)
        if (!it.value()->text().trimmed().isEmpty()) result[it.key()] = {it.value()->text().trimmed()};
    for (auto it = m_combos.cbegin(); it != m_combos.cend(); ++it) {
        auto *box = it.value();
        QString value = box->currentData().toString();
        if (box->isEditable() && (box->currentIndex() < 0 || box->currentText() != box->itemText(box->currentIndex())))
            value = box->currentText().trimmed();
        if (!value.isEmpty()) result[it.key()] = {value};
    }
    for (auto it = m_lists.cbegin(); it != m_lists.cend(); ++it)
        for (int i = 0; i < it.value()->count(); ++i) {
            auto *item = it.value()->item(i);
            if (item->checkState() == Qt::Checked) result[it.key().startsWith(QStringLiteral("excluded_")) ? QStringLiteral("excluded_tag_ids[]") : it.key()].append(item->data(Qt::UserRole).toString());
        }
    return result;
}

void SearchFilterPanel::setFilters(const SearchFilters &filters)
{
    for (auto it = m_text.cbegin(); it != m_text.cend(); ++it) it.value()->setText(filters.value(it.key()).value(0));
    for (auto it = m_combos.cbegin(); it != m_combos.cend(); ++it) {
        const QString value = filters.value(it.key()).value(0);
        const int index = it.value()->findData(value);
        if (index < 0 && it.value()->isEditable() && !value.isEmpty()) it.value()->setEditText(value);
        else it.value()->setCurrentIndex(qMax(0, index));
    }
    for (auto it = m_lists.cbegin(); it != m_lists.cend(); ++it)
        for (int i = 0; i < it.value()->count(); ++i) {
            auto *item = it.value()->item(i);
            item->setCheckState(filters.value(it.key().startsWith(QStringLiteral("excluded_")) ? QStringLiteral("excluded_tag_ids[]") : it.key()).contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
        }
}

void SearchFilterPanel::setOptions(const SearchOptions &options)
{
    if (!options.error.isEmpty()) return;
    // Merge optional imported choices without deleting the bundled catalogue.
    for (auto it = options.choices.cbegin(); it != options.choices.cend(); ++it) {
        for (const auto &choice : it.value()) {
            bool found = false;
            for (auto &entry : m_catalogue.choices[it.key()])
                if (entry.second == choice.second) { entry.first = choice.first; found = true; break; }
            if (!found && !choice.second.isEmpty()) m_catalogue.choices[it.key()].append(choice);
        }
    }
    QSettings settings;
    for (auto it = m_catalogue.choices.cbegin(); it != m_catalogue.choices.cend(); ++it) {
        QVariantList cached;
        for (const auto &choice : it.value()) cached.append(QVariant(QStringList{choice.first, choice.second}));
        settings.setValue(QStringLiteral("filters/catalogue/") + it.key(), cached);
    }
    const auto selected = filters();
    for (const auto &key : {QStringLiteral("rating_ids"), QStringLiteral("language_id")}) {
        auto *box = m_combos[key]; box->clear(); box->addItem(QStringLiteral("Any"), QString());
        for (const auto &choice : m_catalogue.choices.value(key)) box->addItem(choice.first, choice.second);
    }
    for (auto it = m_lists.cbegin(); it != m_lists.cend(); ++it) {
        it.value()->clear();
        QString source = it.key();
        if (source == QStringLiteral("excluded_rating_ids[]")) source = QStringLiteral("rating_ids");
        if (source == QStringLiteral("excluded_warning_ids[]")) source = QStringLiteral("archive_warning_ids[]");
        if (source == QStringLiteral("excluded_category_ids[]")) source = QStringLiteral("category_ids[]");
        for (const auto &choice : m_catalogue.choices.value(source)) {
            auto *item = new QListWidgetItem(choice.first, it.value());
            item->setData(Qt::UserRole, choice.second); item->setCheckState(Qt::Unchecked);
        }
    }
    setFilters(selected);
}

void SearchFilterPanel::showTagSuggestions(const QString &field, quint64 revision, const QStringList &suggestions, const QString &error)
{
    if (auto *edit = qobject_cast<TagLineEdit *>(m_text.value(field))) edit->showSuggestions(revision, suggestions, error);
}
