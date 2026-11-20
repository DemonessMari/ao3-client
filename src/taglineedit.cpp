#include "taglineedit.h"
#include <QCompleter>
#include <QStringListModel>
#include <QAbstractItemView>

namespace {
class TagCompleter : public QCompleter {
public:
    TagCompleter(QLineEdit *edit, QAbstractItemModel *model) : QCompleter(model, edit), m_edit(edit) {}
    QString pathFromIndex(const QModelIndex &index) const override {
        const QString text = m_edit->text();
        const int cursor = m_edit->cursorPosition();
        const int start = cursor > 0 ? text.lastIndexOf(',', cursor - 1) + 1 : 0;
        int end = text.indexOf(',', cursor);
        if (end < 0) end = text.size();
        return text.left(start) + (start ? QStringLiteral(" ") : QString())
            + index.data(Qt::DisplayRole).toString() + text.mid(end);
    }
private:
    QLineEdit *m_edit;
};
}

TagLineEdit::TagLineEdit(const QString &type, QWidget *parent)
    : QLineEdit(parent), m_type(type), m_model(new QStringListModel(this))
{
    m_completer = new TagCompleter(this, m_model);
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setMaxVisibleItems(8);
    setCompleter(m_completer);
    setToolTip(QStringLiteral("Type at least two characters for AO3 tag suggestions. Separate multiple tags with commas."));
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(300);
    connect(this, &QLineEdit::textChanged, this, [this] { invalidate(); });
    connect(this, &QLineEdit::cursorPositionChanged, this, [this] { invalidate(); });
    connect(this, &QLineEdit::textEdited, this, [this] {
        QTimer::singleShot(0, this, [this] {
            if (hasFocus() && currentTerm().size() >= 2) m_debounce.start();
        });
    });
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        if (hasFocus() && currentTerm().size() >= 2)
            emit suggestionsRequested(m_type, currentTerm(), m_revision);
    });
}

QString TagLineEdit::currentTerm() const
{
    const int cursor = cursorPosition();
    const int start = cursor > 0 ? text().lastIndexOf(',', cursor - 1) + 1 : 0;
    int end = text().indexOf(',', cursor);
    if (end < 0) end = text().size();
    return text().mid(start, end - start).trimmed();
}

void TagLineEdit::invalidate()
{
    ++m_revision;
    m_debounce.stop();
    m_completer->popup()->hide();
    m_model->setStringList({});
}

void TagLineEdit::showSuggestions(quint64 revision, const QStringList &suggestions, const QString &error)
{
    if (revision != m_revision || !hasFocus()) return;
    m_completer->popup()->hide();
    m_model->setStringList(suggestions);
    setToolTip(error.isEmpty() ? QStringLiteral("Choose an AO3 tag with the mouse or arrow keys and Enter. Use commas for multiple tags.")
                              : QStringLiteral("Suggestions unavailable: %1. You can still enter tags manually.").arg(error));
    if (!suggestions.isEmpty()) {
        m_completer->setCompletionPrefix(QString());
        m_completer->complete();
    }
}
