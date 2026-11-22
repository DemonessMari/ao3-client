#pragma once
#include <QUrl>
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
    void refreshSkin();
    void setTextOnly(bool enabled);
    void setBookMode(bool enabled);
    void setKudosStatus(const QString &status,bool guest=false);
    QString workId() const { return m_work.id; }
protected:
    void resizeEvent(QResizeEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *, QEvent *) override;
signals:
    void archiveRequested(const QUrl &url);
    void readingActionRequested(const QString &workId);
    void kudosRequested(const QString &workId);
    void workOpened(const QString &workId);
    void backRequested();
    void commentsRequested(const QString &workId);
    void fullscreenRequested();
    void preferencesChanged();
private:
    QPushButton *m_kudosButton=nullptr;
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
    QTimer m_progressTimer, m_autoScrollTimer;
    QPushButton *m_autoScroll=nullptr;
    QSpinBox *m_autoScrollSpeed=nullptr;
    double m_autoScrollRemainder=0;
    void stopAutoScroll();
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
