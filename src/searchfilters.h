#pragma once
#include <QWidget>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <QPair>

using SearchFilters = QMap<QString, QStringList>;
struct SearchOptions {
    QMap<QString, QVector<QPair<QString, QString>>> choices; // field -> label/value
    QString error;
};
class QLineEdit;
class QComboBox;
class QListWidget;
class QLabel;
class QPushButton;

class SearchFilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchFilterPanel(QWidget *parent = nullptr);
    SearchFilters filters() const;
    void setFilters(const SearchFilters &filters);
    void setOptions(const SearchOptions &options);
    void setLoading();
    void showTagSuggestions(const QString &field, quint64 revision, const QStringList &suggestions, const QString &error);
signals:
    void optionsRequested();
    void searchRequested();
    void tagSuggestionsRequested(const QString &field, const QString &type, const QString &term, quint64 revision);
private:
    QMap<QString, QLineEdit *> m_text;
    QMap<QString, QComboBox *> m_combos;
    QMap<QString, QListWidget *> m_lists;
    QLabel *m_status;
    SearchOptions m_catalogue;
};
