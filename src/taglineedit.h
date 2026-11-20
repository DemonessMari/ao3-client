#pragma once
#include <QLineEdit>
#include <QTimer>
class QCompleter;
class QStringListModel;

class TagLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit TagLineEdit(const QString &type, QWidget *parent = nullptr);
    void showSuggestions(quint64 revision, const QStringList &suggestions, const QString &error = {});
signals:
    void suggestionsRequested(const QString &type, const QString &term, quint64 revision);
private:
    QString currentTerm() const;
    void invalidate();
    QString m_type;
    QTimer m_debounce;
    QCompleter *m_completer;
    QStringListModel *m_model;
    quint64 m_revision = 0;
};
