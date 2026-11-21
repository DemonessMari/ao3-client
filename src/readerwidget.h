#pragma once
#include <QWidget>
#include <QMap>
#include <QTimer>
#include "workmodel.h"
#include "translationclient.h"
class QLabel;
class QComboBox;
class QPushButton;
class QTextBrowser;
class QSpinBox;
class BookView;

class ReaderWidget : public QWidget {
    Q_OBJECT
    friend class FunctionalTest;
public:
    explicit ReaderWidget(QWidget *parent = nullptr);
    ~ReaderWidget() override;
    void saveProgress();
    void resetProgress();
    void openWork(const Work &work, const QVector<WorkChapter> &chapters);
    void clear();
    void cancelTranslation();
    void setFullscreen(bool fullscreen);
    void showSettings();
    bool closeOptions();
    void saveOffline();
    void saveText();
    void addBookmark();
    void showBookmarks();
    void applyAppearance();
    void setTextOnly(bool enabled);
    void setBookMode(bool enabled);
protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *, QEvent *) override;
signals:
    void backRequested();
    void commentsRequested(const QString &workId);
    void fullscreenRequested();
    void preferencesChanged();
private:
    void showChapter(int index);
    void showNotes();
    QPushButton *m_notes = nullptr;
    void renderText(const QString &text, double fraction = -1);
    QString progressKey() const;
    BookView *m_book = nullptr;
    bool m_textOnly = false, m_bookMode = false;
    QList<QWidget *> m_hiddenChrome;
    QWidget *m_desktopOptions = nullptr;
    QPushButton *m_desktopOptionsToggle = nullptr;
    QWidget *m_mobileOptions = nullptr;
    QPushButton *m_optionsToggle = nullptr;
    QTimer m_progressTimer;
    quint64 m_renderRevision = 0;
    bool m_restoringPosition = false;
    void translateChapter();
    QString targetLanguage() const;
    QString cacheKey() const;
    Work m_work;
    QVector<WorkChapter> m_chapters;
    int m_index = -1;
    bool m_busy = false;
    QMap<QString, QString> m_translations;
    QLabel *m_title, *m_status;
    QComboBox *m_chapter, *m_language;
    QPushButton *m_previous, *m_next, *m_bottomPrevious, *m_bottomNext, *m_fullscreen, *m_translate, *m_original;
    QTextBrowser *m_browser;
    QSpinBox *m_fontSize;
    TranslationClient *m_translator;
};
