#include "readerwidget.h"
#include "bookview.h"
#include "serviceurl.h"
#include <QShortcut>
#include "readinglibrary.h"
#include <QInputDialog>
#include <QFileDialog>
#include <QSaveFile>
#include <QDesktopServices>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QFontDatabase>
#include "mobileui.h"
#include <QGridLayout>
#include <QScroller>
#include <QScrollArea>
#include <QScreen>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QInputMethod>
#include <QGuiApplication>
#include "buttonstyle.h"
#include "readabletheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QSpinBox>
#include <QSettings>
#include <QScrollBar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QStyle>
#include <QRegularExpression>

namespace {
class ReadingSettingsDialog : public QDialog {
public:
    using QDialog::QDialog;
protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (mobileUi() && event->key() == Qt::Key_Back) {
            if (QGuiApplication::inputMethod()->isVisible()) QGuiApplication::inputMethod()->hide();
            else reject();
            event->accept(); return;
        }
        QDialog::keyPressEvent(event);
    }
};
void languages(QComboBox *box, const QString &selected)
{
    box->setEditable(true);
    const QVector<QPair<QString, QString>> choices = {{"English", "en"}, {"Eesti", "et"}, {"Русский", "ru"}, {"Español", "es"}, {"Français", "fr"}, {"Deutsch", "de"}, {"Português", "pt"}, {"Italiano", "it"}, {"Українська", "uk"}, {"Polski", "pl"}, {"日本語", "ja"}, {"中文", "zh"}, {"한국어", "ko"}, {"Suomi", "fi"}, {"Svenska", "sv"}, {"Türkçe", "tr"}, {"العربية", "ar"}, {"Nederlands", "nl"}};
    for (const auto &choice : choices) box->addItem(choice.first, choice.second);
    const int index = box->findData(selected);
    if (index >= 0) box->setCurrentIndex(index); else box->setEditText(selected);
    box->setToolTip(QStringLiteral("Select a language or enter its language code, such as en or et. Your service must support it."));
}
QString languageCode(QComboBox *box)
{
    if (box->currentIndex() >= 0 && box->currentText() == box->itemText(box->currentIndex())) return box->currentData().toString();
    return box->currentText().trimmed().toLower();
}
}
ReaderWidget::ReaderWidget(QWidget *parent) : QWidget(parent), m_translator(new TranslationClient(this))
{
    applyReadableTheme(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 12, 20, 12); layout->setSpacing(12);
    auto *top = new QGridLayout;
    auto *back = new QPushButton(QStringLiteral("Back to browsing"), this);
    back->setIcon(style()->standardIcon(QStyle::SP_ArrowBack)); top->addWidget(back, 0, 0);
    m_title = new QLabel(this); m_title->setTextFormat(Qt::PlainText); m_title->setWordWrap(true); m_title->setMinimumWidth(0);
    m_title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    top->setColumnStretch(0, 1);
    auto *restart = new QPushButton(QStringLiteral("Start over"), this);
    restart->setObjectName(QStringLiteral("restartReading")); top->addWidget(restart, mobileUi() ? 1 : 0, 1);
    connect(restart, &QPushButton::clicked, this, &ReaderWidget::resetProgress);
    auto *settings = new QPushButton(QStringLiteral("Reading settings"), this); top->addWidget(settings, mobileUi() ? 1 : 0, mobileUi() ? 0 : 2);
    m_fullscreen = new QPushButton(QStringLiteral("Fullscreen (F11)"), this); top->addWidget(m_fullscreen, 0, mobileUi() ? 1 : 3);
    if (mobileUi()) { back->setText(QStringLiteral("Back")); settings->setText(QStringLiteral("Settings")); }
    layout->addLayout(top);
    layout->addWidget(m_title);
    QHBoxLayout *desktopLibraryTools=nullptr;
    if (!mobileUi()) {
        auto *tools = new QHBoxLayout; desktopLibraryTools=tools;
        for (const auto &action : QVector<QPair<QString, void (ReaderWidget::*)()>>{
            {QStringLiteral("Save offline"), &ReaderWidget::saveOffline},
            {QStringLiteral("Export text"), &ReaderWidget::saveText},
            {QStringLiteral("Add bookmark"), &ReaderWidget::addBookmark},
            {QStringLiteral("Bookmarks"), &ReaderWidget::showBookmarks}}) {
            auto *button = new QPushButton(action.first, this); tools->addWidget(button);
            connect(button, &QPushButton::clicked, this, action.second);
        }
        tools->addStretch(); layout->addLayout(tools);
    }

    auto *navigation = new QHBoxLayout;
    m_previous = new QPushButton(QStringLiteral("Previous chapter"), this);
    m_previous->setObjectName(QStringLiteral("previousChapter")); m_previous->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    m_next = new QPushButton(QStringLiteral("Next chapter"), this);
    m_next->setObjectName(QStringLiteral("nextChapter")); m_next->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_chapter = new QComboBox(this); m_chapter->setMinimumContentsLength(15); m_chapter->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto *chapterRow = new QHBoxLayout;
    chapterRow->addWidget(new QLabel(QStringLiteral("Chapter"), this));
    m_chapter->setMinimumWidth(0);
    m_chapter->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chapterRow->addWidget(m_chapter, 1);
    m_notes = new QPushButton("Author notes",this); m_notes->setObjectName("chapterNotes"); m_notes->hide();
    chapterRow->addWidget(m_notes);
    connect(m_notes,&QPushButton::clicked,this,&ReaderWidget::showNotes);
    layout->addLayout(chapterRow);
    navigation->addWidget(m_previous); navigation->addWidget(m_next); navigation->addStretch();
    if (mobileUi()) { layout->addLayout(navigation); navigation = new QHBoxLayout; }
    navigation->addSpacing(mobileUi() ? 0 : 16); navigation->addWidget(new QLabel(QStringLiteral("Text size"), this));
    m_fontSize = new QSpinBox(this); m_fontSize->setRange(11, 32); m_fontSize->setValue(QSettings().value(QStringLiteral("reader/fontSize"), 15).toInt()); navigation->addWidget(m_fontSize);
    layout->addLayout(navigation);
    auto *translation = new QGridLayout;
    translation->addWidget(new QLabel(QStringLiteral("Translate to"), this), 0, 0);
    m_language = new QComboBox(this); languages(m_language, QSettings().value(QStringLiteral("reader/language"), QStringLiteral("en")).toString()); translation->addWidget(m_language, 0, 1);
    m_translate = new QPushButton(QStringLiteral("Translate chapter"), this);
    m_translate->setStyleSheet(secondaryButtonStyle());
    translation->addWidget(m_translate, mobileUi() ? 1 : 0, mobileUi() ? 0 : 2);
    m_original = new QPushButton(QStringLiteral("Show original"), this); translation->addWidget(m_original, mobileUi() ? 1 : 0, mobileUi() ? 1 : 3);
    m_status = new QLabel(this); m_status->setTextFormat(Qt::PlainText); m_status->setWordWrap(true); m_status->setMinimumWidth(0);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    translation->setColumnStretch(1, 1);
    layout->addLayout(translation);
    layout->addWidget(m_status);
    auto *page = new QHBoxLayout;
    page->addStretch(); m_browser = new QTextBrowser(this); m_browser->setOpenExternalLinks(true); m_browser->setMaximumWidth(1000);
    m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background: #faf8f3; color: #292723; border: 1px solid #ddd7ce; padding: 24px; }"));
    m_book = new BookView(this); m_book->setMaximumWidth(1000); m_book->hide();
    page->addWidget(m_book, 1);
    m_book->positionChanged = [this](double fraction) { auto *bar=m_browser->verticalScrollBar(); bar->setValue(qRound(fraction*bar->maximum())); saveProgress(); };
    m_book->chapterRequested = [this](int delta) { const int next=m_index+delta; if(next<0 || next>=m_chapters.size()) return; showChapter(next); renderText(m_chapters[next].text,delta<0?1.0:0.0); };
    m_book->centerClicked = [this] { if(m_textOnly) setTextOnly(false); };
    m_browser->viewport()->installEventFilter(this);
    page->addWidget(m_browser, 1); page->addStretch(); layout->addLayout(page, 1);
    QPushButton *desktopOnline=nullptr;
    auto *comments = new QPushButton("Comments",this); comments->setObjectName("readerComments"); layout->addWidget(comments);
    connect(comments,&QPushButton::clicked,this,[this] {saveProgress();if(!m_work.id.isEmpty())emit commentsRequested(m_work.id);});
    if(!mobileUi()) { auto *online=new QPushButton("Open on AO3 (browser)",this); desktopOnline=online; layout->addWidget(online); connect(online,&QPushButton::clicked,this,[this] { if(!m_work.id.isEmpty()) QDesktopServices::openUrl(QUrl("https://archiveofourown.org/works/"+m_work.id+"?show_comments=true#comments")); }); }
    auto *bottom = new QHBoxLayout;
    m_bottomPrevious = new QPushButton(QStringLiteral("Previous chapter"), this);
    m_bottomNext = new QPushButton(QStringLiteral("Next chapter"), this);
    bottom->addWidget(m_bottomPrevious); bottom->addStretch(); bottom->addWidget(m_bottomNext); layout->addLayout(bottom);
    connect(back, &QPushButton::clicked, this, &ReaderWidget::backRequested);
    connect(settings, &QPushButton::clicked, this, &ReaderWidget::showSettings);
    connect(m_fullscreen, &QPushButton::clicked, this, &ReaderWidget::fullscreenRequested);
    for (auto *button : {m_previous, m_bottomPrevious}) connect(button, &QPushButton::clicked, this, [this] { showChapter(m_index - 1); });
    for (auto *button : {m_next, m_bottomNext}) connect(button, &QPushButton::clicked, this, [this] { showChapter(m_index + 1); });
    connect(m_chapter, &QComboBox::activated, this, &ReaderWidget::showChapter);
    connect(m_translate, &QPushButton::clicked, this, &ReaderWidget::translateChapter);
    connect(m_original, &QPushButton::clicked, this, [this] {
        if (m_index < 0) return;
        m_translator->cancel(); m_busy = false; m_translate->setText(QStringLiteral("Translate chapter"));
        renderText(m_chapters[m_index].text); m_status->setText(QStringLiteral("Original text"));
    });
    connect(m_fontSize, &QSpinBox::valueChanged, this, [this](int size) {
        QSettings().setValue(QStringLiteral("reader/fontSize"), size);
        if (m_index >= 0) renderText(m_browser->toPlainText());
    });
    connect(m_language, &QComboBox::currentTextChanged, this, [this] { if (m_index >= 0) showChapter(m_index); });
    connect(m_translator, &TranslationClient::progress, this, [this](int done, int total) { m_status->setText(QStringLiteral("Translating %1/%2...").arg(done).arg(total)); });
    connect(m_translator, &TranslationClient::completed, this, [this](const QString &text) {
        if (m_index < 0) return;
        m_busy = false; m_translate->setText(QStringLiteral("Translate chapter")); m_translations[cacheKey()] = text;
        renderText(text); m_status->setText(QStringLiteral("Machine translation · %1").arg(targetLanguage()));
    });
    connect(m_translator, &TranslationClient::failed, this, [this](const QString &error) {
        m_busy = false; m_translate->setText(QStringLiteral("Retry translation")); m_status->setText(error);
    });
    m_progressTimer.setSingleShot(true);
    m_progressTimer.setInterval(400);
    connect(m_browser->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        if (!m_restoringPosition && m_index >= 0) m_progressTimer.start();
    });
    connect(&m_progressTimer, &QTimer::timeout, this, &ReaderWidget::saveProgress);
    if (mobileUi()) {
        layout->setContentsMargins(6, 6, 6, 6); layout->setSpacing(4);
        m_previous->setText(QStringLiteral("Previous")); m_next->setText(QStringLiteral("Next"));
        m_bottomPrevious->setText(QStringLiteral("Previous")); m_bottomNext->setText(QStringLiteral("Next"));
        m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background: #faf8f3; color: #292723; border: none; padding: 8px; }"));
        QScroller::grabGesture(m_browser->viewport(), QScroller::TouchGesture);
        for (auto *button : findChildren<QPushButton *>()) button->setMinimumHeight(40);
        m_chapter->setMinimumHeight(40);
        // Reading comes first on a phone. Secondary controls stay tucked away.
        m_mobileOptions = new QScrollArea(this);
        auto *optionsScroll = static_cast<QScrollArea *>(m_mobileOptions);
        optionsScroll->setWidgetResizable(true); optionsScroll->setFrameShape(QFrame::NoFrame);
        auto *optionsBody = new QWidget(optionsScroll);
        auto *optionsLayout = new QVBoxLayout(optionsBody);
        optionsLayout->setContentsMargins(8, 4, 8, 4); optionsLayout->setSpacing(8);
        optionsScroll->setWidget(optionsBody);
        top->removeWidget(restart); top->removeWidget(settings); top->removeWidget(m_fullscreen);
        m_optionsToggle = new QPushButton(QStringLiteral("Options"), this);
        m_optionsToggle->setObjectName(QStringLiteral("readerOptions")); m_optionsToggle->setCheckable(true);
        m_optionsToggle->setMinimumHeight(44);
        top->addWidget(m_optionsToggle, 0, 1); top->addWidget(m_fullscreen, 0, 2);
        auto *actions = new QHBoxLayout; actions->addWidget(settings); actions->addWidget(restart);
        optionsLayout->addLayout(actions);
        layout->removeItem(navigation); optionsLayout->addLayout(navigation);
        layout->removeItem(translation); optionsLayout->addLayout(translation);
        layout->removeWidget(m_status); optionsLayout->addWidget(m_status);
        m_previous->hide(); m_next->hide();
        layout->insertWidget(3, m_mobileOptions);
        m_mobileOptions->hide();
        connect(m_optionsToggle, &QPushButton::toggled, m_mobileOptions, &QWidget::setVisible);
        QScroller::grabGesture(optionsScroll->viewport(), QScroller::TouchGesture);
        m_title->setMaximumHeight(48); m_title->setToolTip(m_title->text());
        m_chapter->setMinimumContentsLength(0);
        setFullscreen(false);

    }
    auto *modes = new QBoxLayout(mobileUi()?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
    auto *hide = new QPushButton("Text only (F10)", this); hide->setObjectName("textOnlyMode"); modes->addWidget(hide);
    auto *book = new QCheckBox("Book mode", this); book->setObjectName("bookMode"); modes->addWidget(book);
    auto *flip = new QCheckBox("Animate page turns", this); flip->setChecked(QSettings().value("reader/pageAnimation", true).toBool()); modes->addWidget(flip);
    auto *previousPage = new QPushButton("Previous page", this); auto *nextPage = new QPushButton("Next page", this); modes->addWidget(previousPage); modes->addWidget(nextPage);
    if(mobileUi()) static_cast<QVBoxLayout *>(static_cast<QScrollArea *>(m_mobileOptions)->widget()->layout())->addLayout(modes);
    else layout->insertLayout(1,modes);
    connect(hide,&QPushButton::clicked,this,[this] { m_status->setText("F10, Escape, or double-click text to restore controls."); setTextOnly(true); });
    connect(book,&QCheckBox::toggled,this,&ReaderWidget::setBookMode);
    connect(flip,&QCheckBox::toggled,this,[this](bool value) { QSettings().setValue("reader/pageAnimation",value); m_book->animated=value; });
    m_book->animated=flip->isChecked();
    connect(previousPage,&QPushButton::clicked,this,[this] { if(m_bookMode) m_book->turn(-1); });
    connect(nextPage,&QPushButton::clicked,this,[this] { if(m_bookMode) m_book->turn(1); });
    auto *pageNumber=new QLabel(this); pageNumber->setObjectName("bookPageNumber"); modes->addWidget(pageNumber);
    m_book->pageChanged=[this,pageNumber] { pageNumber->setText(QStringLiteral("Page %1 / %2").arg(m_book->page()+1).arg(m_book->pages())); };
    connect(book,&QCheckBox::toggled,pageNumber,&QWidget::setVisible); pageNumber->hide();
    previousPage->setEnabled(false); nextPage->setEnabled(false);
    connect(book,&QCheckBox::toggled,previousPage,&QWidget::setEnabled); connect(book,&QCheckBox::toggled,nextPage,&QWidget::setEnabled);
    auto *toggle = new QShortcut(QKeySequence(Qt::Key_F10),this); connect(toggle,&QShortcut::activated,this,[this] { setTextOnly(!m_textOnly); });
    if(!mobileUi()) {
        auto *panel=new QScrollArea(this); m_desktopOptions=panel; panel->setObjectName("desktopReadingTools"); panel->setWidgetResizable(true); panel->setFrameShape(QFrame::NoFrame); panel->setMaximumHeight(260);
        auto *body=new QWidget(panel); auto *options=new QVBoxLayout(body); options->setContentsMargins(8,8,8,8); options->setSpacing(10); panel->setWidget(body);
        for(auto *row:QList<QLayout *>{desktopLibraryTools,navigation,translation,modes}) { layout->removeItem(row); options->addLayout(row); }
        if(desktopOnline) { layout->removeWidget(desktopOnline); options->addWidget(desktopOnline); }
        modes->removeWidget(previousPage); modes->removeWidget(nextPage); modes->removeWidget(pageNumber);
        auto *pages=new QWidget(this); auto *pageRow=new QHBoxLayout(pages); pageRow->setContentsMargins(0,0,0,0); pageRow->addWidget(previousPage); pageRow->addStretch(); pageRow->addWidget(pageNumber); pageRow->addStretch(); pageRow->addWidget(nextPage);
        pages->hide(); connect(book,&QCheckBox::toggled,pages,&QWidget::setVisible);
        layout->insertWidget(layout->count()-1,pages);
        layout->insertWidget(3,panel); panel->hide();
        m_desktopOptionsToggle=new QPushButton("Reading tools",this); m_desktopOptionsToggle->setObjectName("desktopReadingToolsToggle"); m_desktopOptionsToggle->setCheckable(true); top->addWidget(m_desktopOptionsToggle,0,4);
        connect(m_desktopOptionsToggle,&QPushButton::toggled,panel,&QWidget::setVisible);
    }
    clear();
    book->setChecked(QSettings().value("reader/bookMode",false).toBool());
}
ReaderWidget::~ReaderWidget() { saveProgress(); }
QString ReaderWidget::progressKey() const
{
    if (!QRegularExpression(QStringLiteral("^\\d+$")).match(m_work.id).hasMatch()) return {};
    return QStringLiteral("readingProgress/%1/").arg(m_work.id);
}
void ReaderWidget::saveProgress()
{
    if (m_index < 0 || m_restoringPosition || progressKey().isEmpty()) return;
    auto *bar = m_browser->verticalScrollBar();
    QSettings settings;
    settings.setValue(progressKey() + QStringLiteral("chapter"), m_index);
    settings.setValue(progressKey() + QStringLiteral("positions/%1").arg(m_index), m_bookMode ? m_book->fraction() : (bar->maximum() > 0 ? double(bar->value()) / bar->maximum() : 0.0));
}
void ReaderWidget::resetProgress()
{
    if (m_chapters.isEmpty() || progressKey().isEmpty()) return;
    m_progressTimer.stop();
    QSettings().remove(progressKey().chopped(1));
    m_restoringPosition = true; // Don't save the old chapter back over the reset.
    showChapter(0);
}
QString ReaderWidget::targetLanguage() const { return languageCode(m_language); }
QString ReaderWidget::cacheKey() const { return QString::number(m_index) + QChar(':') + targetLanguage(); }
void ReaderWidget::openWork(const Work &work, const QVector<WorkChapter> &chapters)
{
    clear(); m_work = work; m_chapters = chapters;
    m_title->setText(work.title + QStringLiteral(" — ") + work.author);
    m_title->setToolTip(m_title->text());
    closeOptions();
    if (!ReadingLibrary().remember(work)) m_status->setText(QStringLiteral("Could not save reading history"));
    for (int i = 0; i < chapters.size(); ++i) m_chapter->addItem(QStringLiteral("%1 / %2 · %3").arg(i + 1).arg(chapters.size()).arg(chapters[i].title));
    const int saved = progressKey().isEmpty() ? 0 : QSettings().value(progressKey() + QStringLiteral("chapter"), 0).toInt();
    if (!chapters.isEmpty()) showChapter(qBound(0, saved, int(chapters.size()) - 1));
}
void ReaderWidget::showChapter(int index)
{
    if (index < 0 || index >= m_chapters.size()) return;
    saveProgress();
    m_translator->cancel(); m_busy = false; m_index = index;
    m_translate->setEnabled(true); m_translate->setText(QStringLiteral("Translate chapter")); m_original->setEnabled(true);
    m_chapter->setCurrentIndex(index);
    m_notes->setVisible(!m_chapters[index].notesBefore.isEmpty() || !m_chapters[index].notesAfter.isEmpty());
    m_previous->setEnabled(index > 0); m_bottomPrevious->setEnabled(index > 0);
    m_next->setEnabled(index + 1 < m_chapters.size()); m_bottomNext->setEnabled(index + 1 < m_chapters.size());
    const double position = progressKey().isEmpty() ? 0.0 : QSettings().value(progressKey() + QStringLiteral("positions/%1").arg(index), 0.0).toDouble();
    renderText(m_chapters[index].text, qBound(0.0, position, 1.0)); m_status->setText(QStringLiteral("Original text · Chapter %1 of %2").arg(index + 1).arg(m_chapters.size()));
}
void ReaderWidget::renderText(const QString &text, double fraction)
{
    auto *bar = m_browser->verticalScrollBar();
    const int anchor=m_bookMode && fraction<0 ? m_book->anchor() : -1;
    if (fraction < 0 && m_bookMode) fraction = m_book->fraction();
    if (fraction < 0) fraction = bar->maximum() > 0 ? double(bar->value()) / bar->maximum() : 0.0;
    m_progressTimer.stop();
    m_restoringPosition = true;
    const quint64 revision = ++m_renderRevision;
    QSettings settings;
    const QString family = settings.value("reader/fontFamily", "serif").toString() == "sans-serif" ? "sans-serif" : "serif";
    const int spacing = qBound(120, settings.value("reader/lineSpacing", 160).toInt(), 220);
    applyAppearance();
    // QTextDocument does not implement browser CSS line-height percentages.
    // Apply its native font and block formats so every paragraph actually reflows.
    QFont font(family);
    font.setStyleHint(family == "sans-serif" ? QFont::SansSerif : QFont::Serif);
    font.setPointSize(m_fontSize->value());
    m_browser->document()->setDefaultFont(font);
    m_browser->setPlainText(text);
    QTextCursor cursor(m_browser->document());
    cursor.select(QTextCursor::Document);
    QTextBlockFormat block;
    block.setLineHeight(spacing, QTextBlockFormat::ProportionalHeight);
    cursor.mergeBlockFormat(block);
    QTimer::singleShot(0, this, [this, revision, fraction, anchor] {
        if (revision != m_renderRevision) return;
        auto *scroll = m_browser->verticalScrollBar();
        scroll->setValue(qRound(qBound(0.0, fraction, 1.0) * scroll->maximum()));
        m_restoringPosition = false;
        if(m_bookMode) { m_book->setContent(m_browser->document(), fraction, m_browser->palette().color(QPalette::Base), m_browser->palette().color(QPalette::Text)); if(anchor>=0) m_book->restoreAnchor(anchor); }
    });
}
void ReaderWidget::showNotes()
{
    if (m_index < 0 || m_index >= m_chapters.size()) return;
    const auto &chapter = m_chapters[m_index];
    QDialog dialog(this); dialog.setWindowTitle("Author notes"); dialog.resize(700,600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *text = new QTextBrowser(&dialog); text->setObjectName("authorNotesText");
    text->setFont(m_browser->document()->defaultFont());
    QStringList sections;
    if (!chapter.notesBefore.isEmpty()) sections << "Before chapter\n\n" + chapter.notesBefore;
    if (!chapter.notesAfter.isEmpty()) sections << "After chapter\n\n" + chapter.notesAfter;
    text->setPlainText(sections.join("\n\n")); layout->addWidget(text);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close,&dialog); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    dialog.exec();
}
void ReaderWidget::translateChapter()
{
    if (m_index < 0) return;
    if (m_busy) { m_translator->cancel(); m_busy = false; m_translate->setText(QStringLiteral("Translate chapter")); m_status->setText(QStringLiteral("Translation cancelled")); return; }
    if (!QRegularExpression(QStringLiteral("^[a-z]{2,3}(?:-[a-z]{2,4})?$")).match(targetLanguage()).hasMatch()) { m_status->setText(QStringLiteral("Choose a target language or enter a language code.")); return; }
    if (m_translations.contains(cacheKey())) { renderText(m_translations.value(cacheKey())); m_status->setText(QStringLiteral("Machine translation · %1").arg(targetLanguage())); return; }
    QSettings settings;
    const QUrl endpoint(settings.value(QStringLiteral("translation/url"), QString()).toString());
    if(!validTranslationService(endpoint)) { m_status->setText("Configure a trusted HTTPS LibreTranslate service in Reading settings first."); return; }
    if(endpoint.scheme()=="https" && !settings.value("translation/consented",false).toBool()) {
        if(QMessageBox::question(this,"Send chapter for translation?","The chapter text will be sent to your configured translation service. AO3 credentials are not sent. Continue?",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes) return;
        QSettings().setValue("translation/consented",true);
    }
    m_busy = true; m_translate->setText(QStringLiteral("Cancel translation"));
    m_translator->translate(m_chapters[m_index].text, targetLanguage(), endpoint, settings.value(QStringLiteral("translation/key")).toString());
}
void ReaderWidget::clear()
{
    saveProgress();
    m_progressTimer.stop();
    ++m_renderRevision;
    m_restoringPosition = true;
    m_translator->cancel(); m_busy = false; m_index = -1; m_work = {}; m_chapters.clear(); m_translations.clear();
    m_notes->hide(); m_chapter->clear(); m_browser->clear(); m_title->clear(); m_status->clear();
    for (auto *button : {m_previous, m_next, m_bottomPrevious, m_bottomNext, m_translate, m_original}) button->setEnabled(false);
    m_restoringPosition = false;
}
void ReaderWidget::setFullscreen(bool full) {
    m_fullscreen->setText(mobileUi() ? (full ? QStringLiteral("Exit fullscreen") : QStringLiteral("Fullscreen")) :
        (full ? QStringLiteral("Exit fullscreen (Esc)") : QStringLiteral("Fullscreen (F11)")));
}
void ReaderWidget::showSettings()
{
    QSettings settings;
    ReadingSettingsDialog dialog(this); dialog.setWindowTitle(QStringLiteral("Reading settings"));
    applyReadableTheme(&dialog);
    dialog.setMinimumSize(mobileUi() ? QSize(0, 0) : QSize(540, 380));
    dialog.resize(mobileUi() ? QSize(360, 640) : QSize(640, 430));
    auto *outer = new QVBoxLayout(&dialog);
    QVBoxLayout *layout = outer;
    {
        outer->setContentsMargins(8, 8, 8, 8);
        auto *heading = new QLabel(QStringLiteral("Reading settings"), &dialog);
        heading->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: bold; padding: 8px;"));
        outer->addWidget(heading);
        auto *scroll = new QScrollArea(&dialog); scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *body = new QWidget(scroll); layout = new QVBoxLayout(body); scroll->setWidget(body);
        outer->addWidget(scroll, 1);
        QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    }
    layout->setContentsMargins(mobileUi() ? 8 : 24, 24, mobileUi() ? 8 : 24, 24);
    layout->setSpacing(18);
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(mobileUi() ? QFormLayout::WrapAllRows : QFormLayout::WrapLongRows);
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(14);
    auto *language = new QComboBox(&dialog); language->setObjectName(QStringLiteral("defaultReadingLanguage")); languages(language, settings.value(QStringLiteral("reader/language"), QStringLiteral("en")).toString());
    auto *search = new QCheckBox(QStringLiteral("Use this language for new searches"), &dialog); search->setObjectName(QStringLiteral("filterDefaultLanguage")); search->setChecked(settings.value(QStringLiteral("reader/filterLanguage"), false).toBool());
    auto *url = new QLineEdit(settings.value(QStringLiteral("translation/url"), QString()).toString(), &dialog);
    url->setObjectName(QStringLiteral("translationServiceUrl"));
    url->setMinimumWidth(mobileUi() ? 0 : 280);
    auto *key = new QLineEdit(settings.value(QStringLiteral("translation/key")).toString(), &dialog); key->setEchoMode(QLineEdit::Password);
    auto *theme = new QComboBox(&dialog); theme->setObjectName("readingTheme");
    theme->addItems({"Light", "Sepia", "Dark"}); theme->setCurrentIndex(qBound(0, settings.value("reader/theme", 0).toInt(), 2));
    auto *family = new QComboBox(&dialog); family->setObjectName("readingFontFamily"); family->addItem("Serif", "serif"); family->addItem("Sans serif", "sans-serif"); family->setCurrentIndex(settings.value("reader/fontFamily", "serif").toString() == "sans-serif" ? 1 : 0);
    auto *spacing = new QSpinBox(&dialog); spacing->setObjectName("readingLineSpacing"); spacing->setRange(120, 220); spacing->setSingleStep(10); spacing->setSuffix("%"); spacing->setValue(settings.value("reader/lineSpacing", 160).toInt());
    form->addRow("Reading theme", theme); form->addRow("Font", family); form->addRow("Line spacing", spacing);
    form->addRow(QStringLiteral("Default language"), language); form->addRow(search);
    form->addRow(QStringLiteral("LibreTranslate URL"), url); form->addRow(QStringLiteral("API key (optional)"), key); layout->addLayout(form);
    auto *size = new QSpinBox(&dialog); size->setObjectName("readingTextSize"); size->setRange(11,32); size->setValue(m_fontSize->value()); form->addRow("Text size",size);
    auto *preview = new QTextBrowser(&dialog); preview->setObjectName("readingPreview"); preview->setMinimumHeight(120); preview->setMaximumHeight(180); layout->addWidget(preview);
    auto liveAppearance = [&, this] {
        settings.setValue("reader/theme",theme->currentIndex()); settings.setValue("reader/fontFamily",family->currentData()); settings.setValue("reader/lineSpacing",spacing->value());
        m_fontSize->setValue(size->value());
        if(m_index>=0) renderText(m_browser->toPlainText());
        preview->setStyleSheet(QStringLiteral("QTextBrowser { background: %1; color: %2; padding: 12px; }").arg(theme->currentIndex()==2?"#191b1e":theme->currentIndex()==1?"#f1e3c7":"#faf8f3",theme->currentIndex()==2?"#e7e1d8":"#292723"));
        QFont font(family->currentData().toString()); font.setPointSize(size->value()); preview->document()->setDefaultFont(font);
        preview->setPlainText("Text preview.\nSecond line of text.");
        QTextCursor cursor(preview->document()); cursor.select(QTextCursor::Document); QTextBlockFormat block; block.setLineHeight(spacing->value(),QTextBlockFormat::ProportionalHeight); cursor.mergeBlockFormat(block);
    };
    connect(theme,&QComboBox::currentIndexChanged,&dialog,liveAppearance); connect(family,&QComboBox::currentIndexChanged,&dialog,liveAppearance); connect(spacing,&QSpinBox::valueChanged,&dialog,liveAppearance); connect(size,&QSpinBox::valueChanged,&dialog,liveAppearance); liveAppearance();
    auto *note = new QLabel(QStringLiteral("HTTPS required except on localhost."), &dialog);
    note->setWordWrap(true); note->setMinimumWidth(0);
    note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred); layout->addWidget(note);
    auto *about=new QPushButton("About, privacy and licenses",&dialog); layout->addWidget(about);
    connect(about,&QPushButton::clicked,&dialog,[&] {
        QDialog info(&dialog); info.setWindowTitle("About AO3 Client 3.0.0"); info.resize(640,500); applyReadableTheme(&info); QVBoxLayout body(&info); QTextBrowser text(&info); text.setOpenExternalLinks(true);
        QString content="AO3 Client 3.0.0 · Unofficial; not affiliated with AO3 / OTW.\n\n";
        for(const auto &path:{":/docs/PRIVACY.md",":/docs/THIRD-PARTY-NOTICES.md",":/docs/LICENSE"}) { QFile file(path); if(file.open(QIODevice::ReadOnly)) content+=QString::fromUtf8(file.readAll())+"\n\n"; }
        text.setPlainText(content); body.addWidget(&text); QDialogButtonBox close(QDialogButtonBox::Close,&info); body.addWidget(&close); connect(&close,&QDialogButtonBox::rejected,&info,&QDialog::reject); info.exec();
    });
    layout->addStretch();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); outer->addWidget(buttons);
    buttons->setStyleSheet(secondaryButtonStyle());
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        const QString code = languageCode(language); const QUrl service(url->text().trimmed());
        if (!QRegularExpression(QStringLiteral("^[a-z]{2,3}(?:-[a-z]{2,4})?$")).match(code).hasMatch() || (!url->text().trimmed().isEmpty() && !validTranslationService(service))) {
            QMessageBox::warning(&dialog, QStringLiteral("Check settings"), QStringLiteral("Enter a language code and an HTTPS service URL (HTTP is allowed only for localhost).")); return;
        }
        settings.setValue("reader/theme", theme->currentIndex()); settings.setValue("reader/fontFamily", family->currentData()); settings.setValue("reader/lineSpacing", spacing->value());
        settings.setValue(QStringLiteral("reader/language"), code); settings.setValue(QStringLiteral("reader/filterLanguage"), search->isChecked());
        if(settings.value("translation/url").toString()!=service.toString()) settings.setValue("translation/consented",false);
        settings.setValue(QStringLiteral("translation/url"), service.toString()); settings.setValue(QStringLiteral("translation/key"), key->text());
        settings.sync(); QFile::setPermissions(settings.fileName(),QFile::ReadOwner|QFile::WriteOwner);
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (mobileUi()) dialog.setWindowState(Qt::WindowMaximized);
    if (dialog.exec() == QDialog::Accepted) {
        const QString code = settings.value(QStringLiteral("reader/language")).toString();
        const int index = m_language->findData(code); if (index >= 0) m_language->setCurrentIndex(index); else m_language->setEditText(code);
        m_translations.clear();
        if (m_index >= 0) showChapter(m_index);
        emit preferencesChanged();
    }
}

void ReaderWidget::cancelTranslation()
{
    m_translator->cancel(); m_busy = false; m_translate->setText(QStringLiteral("Translate chapter"));
}

void ReaderWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if(m_desktopOptions) m_desktopOptions->setMaximumHeight(qMax(100,qMin(260,height()/2)));
    if (m_mobileOptions) m_mobileOptions->setMaximumHeight(qMax(100, qMin(260, height() / 2)));
}
bool ReaderWidget::closeOptions()
{
    if(m_textOnly) { setTextOnly(false); return true; }
    if(m_desktopOptionsToggle && m_desktopOptionsToggle->isChecked()) { m_desktopOptionsToggle->setChecked(false); return true; }
    if (!m_optionsToggle || !m_optionsToggle->isChecked()) return false;
    m_optionsToggle->setChecked(false); return true;
}

void ReaderWidget::applyAppearance()
{
    const int theme = QSettings().value("reader/theme", 0).toInt();
    const QString background = theme == 2 ? "#191b1e" : theme == 1 ? "#f1e3c7" : "#faf8f3";
    const QString foreground = theme == 2 ? "#e7e1d8" : "#292723";
    m_browser->setStyleSheet(QStringLiteral("QTextBrowser { background: %1; color: %2; border: none; padding: %3px; }").arg(background, foreground).arg(mobileUi() ? 8 : 24));
}
void ReaderWidget::saveText()
{
    if(m_chapters.isEmpty()) return;
    const auto path=QFileDialog::getSaveFileName(this,"Export original fic",m_work.title+".txt","Text files (*.txt)"); if(path.isEmpty()) return;
    QString text=m_work.title+"\nby "+m_work.author+"\n\n"; for(const auto &chapter:m_chapters) text+=chapter.exportText()+"\n\n";
    QSaveFile file(path); const auto bytes=text.toUtf8(); const bool saved=file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit();
    m_status->setText(saved?"Original fic exported":"Could not export fic. Check storage and permissions.");
}
void ReaderWidget::saveOffline()
{
    if (m_index < 0) return;
    m_status->setText(ReadingLibrary().saveOffline(m_work, m_chapters) ? QStringLiteral("Saved offline.") : QStringLiteral("Could not save offline copy. Check available storage."));
}
void ReaderWidget::addBookmark()
{
    if (m_index < 0) return;
    bool ok = false;
    const QString title = QInputDialog::getText(this, "Add bookmark", "Bookmark name", QLineEdit::Normal, m_chapters[m_index].title, &ok).trimmed();
    if (!ok || title.isEmpty()) return;
    const auto *bar = m_browser->verticalScrollBar();
    QVariantList entries = QSettings().value("bookmarks/" + m_work.id).toList();
    entries.append(QVariantMap{{"title", title}, {"chapter", m_index}, {"position", m_bookMode ? m_book->fraction() : (bar->maximum() > 0 ? double(bar->value()) / bar->maximum() : 0.0)}});
    QSettings().setValue("bookmarks/" + m_work.id, entries); m_status->setText("Bookmark added");
}
void ReaderWidget::showBookmarks()
{
    if (m_index < 0) return;
    QDialog dialog(this); dialog.setWindowTitle("Bookmarks"); dialog.resize(480, 360); applyReadableTheme(&dialog);
    auto *layout = new QVBoxLayout(&dialog); auto *list = new QListWidget(&dialog); list->setWordWrap(true); layout->addWidget(list);
    auto entries = QSettings().value("bookmarks/" + m_work.id).toList();
    auto fill = [&] { list->clear(); for (const auto &entry : entries) { const auto item = entry.toMap(); list->addItem(QStringLiteral("Chapter %1 · %2").arg(item.value("chapter").toInt() + 1).arg(item.value("title").toString())); } };
    fill(); auto *row = new QHBoxLayout; auto *open = new QPushButton("Go to bookmark", &dialog); auto *remove = new QPushButton("Delete bookmark", &dialog); row->addWidget(open); row->addWidget(remove); layout->addLayout(row);
    auto jump = [&] { const int index = list->currentRow(); if (index < 0) return; const auto item = entries[index].toMap(); const int chapter = item.value("chapter").toInt(); if (chapter < 0 || chapter >= m_chapters.size()) return; showChapter(chapter); renderText(m_chapters[chapter].text, item.value("position").toDouble()); dialog.accept(); };
    connect(open, &QPushButton::clicked, &dialog, jump); connect(list, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem *) { jump(); });
    connect(remove, &QPushButton::clicked, &dialog, [&] { if (list->currentRow() < 0) return; entries.removeAt(list->currentRow()); QSettings().setValue("bookmarks/" + m_work.id, entries); fill(); });
    dialog.exec();
}

void ReaderWidget::setBookMode(bool enabled) {
    QSettings().setValue("reader/bookMode",enabled);
    m_bookMode=enabled; m_browser->setVisible(!enabled); m_book->setVisible(enabled);
    if(enabled) { auto *bar=m_browser->verticalScrollBar(); m_book->setContent(m_browser->document(),bar->maximum()>0?double(bar->value())/bar->maximum():0,m_browser->palette().color(QPalette::Base),m_browser->palette().color(QPalette::Text)); m_book->setFocus(); }
}
void ReaderWidget::setTextOnly(bool enabled) {
    if(m_textOnly==enabled) return;
    m_textOnly=enabled;
    if(enabled) { m_hiddenChrome.clear(); for(auto *child:findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly)) if(child!=m_browser && child!=m_book && !child->isHidden()) { m_hiddenChrome.append(child); child->hide(); } layout()->setContentsMargins(0,0,0,0); layout()->setSpacing(0); }
    else { for(auto *child:m_hiddenChrome) child->show(); m_hiddenChrome.clear(); layout()->setContentsMargins(20,12,20,12); layout()->setSpacing(12); }
    m_browser->setVerticalScrollBarPolicy(enabled?Qt::ScrollBarAlwaysOff:Qt::ScrollBarAsNeeded);
    if(m_bookMode) m_book->setFocus(); else m_browser->setFocus();
}
bool ReaderWidget::eventFilter(QObject *object,QEvent *event) {
    if(object==m_browser->viewport() && event->type()==QEvent::MouseButtonDblClick && m_textOnly) { setTextOnly(false); return true; }
    return QWidget::eventFilter(object,event);
}
