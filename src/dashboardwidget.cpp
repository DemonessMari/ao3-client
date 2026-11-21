#include "dashboardwidget.h"
#include "mobileui.h"
#include <QFont>
#include <QPushButton>

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    m_loggedOutWidget = new QWidget(this);
    auto *outLayout = new QVBoxLayout(m_loggedOutWidget);
    auto *outTitle = new QLabel(QStringLiteral("AO3 Client"), m_loggedOutWidget);
    QFont tf = outTitle->font();
    tf.setPointSize(18);
    tf.setBold(true);
    outTitle->setFont(tf);
    outTitle->setWordWrap(true);
    outTitle->setAlignment(Qt::AlignCenter);

    auto *outSub = new QLabel(
        QString(),
        m_loggedOutWidget
    );
    outSub->setWordWrap(true);
    outSub->setAlignment(Qt::AlignCenter);
    outSub->setStyleSheet(QStringLiteral("color: #888; font-size: 12pt;"));

    outLayout->addWidget(outTitle);
    outSub->hide();
    layout->addWidget(m_loggedOutWidget);

    m_welcomeWidget = new QWidget(this);
    auto *welLayout = new QVBoxLayout(m_welcomeWidget);

    m_greetingLabel = new QLabel(m_welcomeWidget);
    QFont gf = m_greetingLabel->font();
    gf.setPointSize(16);
    gf.setBold(true);
    m_greetingLabel->setFont(gf);
    welLayout->addWidget(m_greetingLabel);

    m_infoLabel = new QLabel(m_welcomeWidget);
    m_infoLabel->setStyleSheet(QStringLiteral("color: #666; font-size: 11pt;"));
    welLayout->addWidget(m_infoLabel);

    welLayout->addStretch();
    m_welcomeWidget->hide();
    layout->addWidget(m_welcomeWidget);
    if (mobileUi()) {
        outTitle->setText(QStringLiteral("AO3 Client"));
        layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(20);
        auto *browse = new QPushButton(QStringLiteral("Search"), this);
        browse->setMinimumHeight(52);
        browse->setStyleSheet(QStringLiteral("QPushButton { background: #900; color: white; border: none; border-radius: 16px; font-size: 12pt; font-weight: bold; padding: 12px; }"));
        layout->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, &DashboardWidget::browseRequested);
        m_greetingLabel->setWordWrap(true); m_infoLabel->setWordWrap(true);
    }
}

void DashboardWidget::showUserInfo(const UserInfo &user)
{
    m_loggedOutWidget->hide();
    m_welcomeWidget->show();
    m_greetingLabel->setText(user.username);
    m_infoLabel->hide();
}

void DashboardWidget::showNoUser()
{
    m_welcomeWidget->hide();
    m_loggedOutWidget->show();
}
