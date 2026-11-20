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
    auto *outTitle = new QLabel(QStringLiteral("Welcome to AO3 Client"), m_loggedOutWidget);
    QFont tf = outTitle->font();
    tf.setPointSize(18);
    tf.setBold(true);
    outTitle->setFont(tf);
    outTitle->setWordWrap(true);
    outTitle->setAlignment(Qt::AlignCenter);

    auto *outSub = new QLabel(
        QStringLiteral("Search and browse Archive of Our Own works.\n"
                       "Sign in to access your dashboard and works."),
        m_loggedOutWidget
    );
    outSub->setWordWrap(true);
    outSub->setAlignment(Qt::AlignCenter);
    outSub->setStyleSheet(QStringLiteral("color: #888; font-size: 12pt;"));

    outLayout->addWidget(outTitle);
    outLayout->addWidget(outSub);
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
        outTitle->setText(QStringLiteral("Your reading space"));
        outSub->setText(QStringLiteral("Search AO3, filter by the tags you love, and settle into a fic."));
        layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(20);
        auto *browse = new QPushButton(QStringLiteral("Browse stories"), this);
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
    m_greetingLabel->setText(QStringLiteral("Hi, %1!").arg(user.username));
    m_infoLabel->setText(mobileUi() ? QStringLiteral("Tap My works below to browse your fics.") : QStringLiteral("Choose My Works above to browse your works."));
}

void DashboardWidget::showNoUser()
{
    m_welcomeWidget->hide();
    m_loggedOutWidget->show();
}
