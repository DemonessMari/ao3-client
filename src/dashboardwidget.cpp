#include "dashboardwidget.h"
#include "readinglibrary.h"
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QHBoxLayout>

DashboardWidget::DashboardWidget(QWidget *parent) : QWidget(parent)
{
    auto *outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);auto *scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);outer->addWidget(scroll);
    auto *body=new QWidget(scroll);scroll->setWidget(body);auto *layout=new QVBoxLayout(body);layout->setContentsMargins(24,24,24,24);layout->setSpacing(16);
    m_greetingLabel=new QLabel("Home",body);auto font=m_greetingLabel->font();font.setPointSize(20);font.setBold(true);m_greetingLabel->setFont(font);m_greetingLabel->setWordWrap(true);layout->addWidget(m_greetingLabel);
    m_infoLabel=new QLabel(body);m_infoLabel->setWordWrap(true);layout->addWidget(m_infoLabel);
    auto *actions=new QHBoxLayout;layout->addLayout(actions);
    auto *search=new QPushButton("Search",body);auto *library=new QPushButton("Library",body);auto *skin=new QPushButton("App skin",body);
    for(auto *b:{search,library,skin}){b->setMinimumHeight(44);actions->addWidget(b);}
    connect(search,&QPushButton::clicked,this,&DashboardWidget::browseRequested);connect(library,&QPushButton::clicked,this,&DashboardWidget::libraryRequested);connect(skin,&QPushButton::clicked,this,&DashboardWidget::skinRequested);
    auto *heading=new QLabel("Recent reads",body);layout->addWidget(heading);
    m_empty=new QLabel("Your recent reads will appear here.",body);m_empty->setWordWrap(true);layout->addWidget(m_empty);
    m_recent=new QListWidget(body);m_recent->setObjectName("homeRecentReads");m_recent->setMinimumHeight(160);m_recent->setMaximumHeight(360);m_recent->setWordWrap(true);layout->addWidget(m_recent);
    connect(m_recent,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){emit workRequested(item->data(Qt::UserRole).toString());});
    connect(m_recent,&QListWidget::itemActivated,this,[this](QListWidgetItem *item){emit workRequested(item->data(Qt::UserRole).toString());});
    layout->addStretch();refreshLibrary();
}
void DashboardWidget::refreshLibrary() {
    const auto entries=ReadingLibrary().entries();int offline=0,favorites=0;m_recent->clear();
    for(const auto &value:entries){const auto entry=value.toObject();offline+=entry.value("offline").toBool();favorites+=entry.value("favorite").toBool();if(m_recent->count()<5){const auto work=ReadingLibrary::work(entry);auto *item=new QListWidgetItem(work.title+"\nby "+work.author+(entry.value("offline").toBool()?" · Offline":""),m_recent);item->setData(Qt::UserRole,work.id);item->setSizeHint(QSize(0,64));}}
    m_recent->setFixedHeight(m_recent->count()*64+4);
    m_infoLabel->setText(QString("%1 recent works · %2 offline · %3 favorites").arg(entries.size()).arg(offline).arg(favorites));m_recent->setVisible(!entries.isEmpty());m_empty->setVisible(entries.isEmpty());
}
void DashboardWidget::showUserInfo(const UserInfo &user) {m_greetingLabel->setText(user.username);refreshLibrary();}
void DashboardWidget::showNoUser() {m_greetingLabel->setText("Home");refreshLibrary();}
