#include "logindialog.h"
#include "buttonstyle.h"
#include <QLineEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFont>

LoginDialog::LoginDialog(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);

    auto *card = new QFrame(this);
    card->setMaximumWidth(380);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: palette(base); border: 1px solid palette(mid); border-radius: 8px; }"
    ));

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setSpacing(12);
    cardLayout->setContentsMargins(24, 24, 24, 24);

    auto *title = new QLabel(QStringLiteral("Sign in to AO3"), card);
    QFont tf = title->font();
    tf.setPointSize(16);
    tf.setBold(true);
    title->setFont(tf);
    title->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(title);

    auto *formLayout = new QFormLayout();
    formLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_loginInput = new QLineEdit(card);
    m_loginInput->setPlaceholderText(QStringLiteral("Username or email"));
    m_loginInput->setMinimumHeight(32);
    formLayout->addRow(QStringLiteral("Login:"), m_loginInput);

    m_passwordInput = new QLineEdit(card);
    m_passwordInput->setPlaceholderText(QStringLiteral("Password"));
    m_passwordInput->setEchoMode(QLineEdit::Password);
    m_passwordInput->setMinimumHeight(32);
    formLayout->addRow(QStringLiteral("Password:"), m_passwordInput);

    cardLayout->addLayout(formLayout);

    m_statusLabel = new QLabel(card);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color: red;"));
    m_statusLabel->hide();
    cardLayout->addWidget(m_statusLabel);

    m_loginButton = new QPushButton(QStringLiteral("Sign in"), card);
    m_loginButton->setMinimumHeight(36);
    m_loginButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #900; color: white; border: none; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background: #b00; }"
        "QPushButton:pressed { background: #700; }"
    ));
    cardLayout->addWidget(m_loginButton);

    auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), card);
    cancelButton->setMinimumHeight(36);
    cancelButton->setStyleSheet(secondaryButtonStyle());
    cardLayout->addWidget(cancelButton);

    layout->addWidget(card);

    connect(m_loginButton, &QPushButton::clicked, this, [this]() {
        QString login = m_loginInput->text().trimmed();
        QString password = m_passwordInput->text();
        if (login.isEmpty() || password.isEmpty()) {
            setStatus(QStringLiteral("Please enter both login and password"));
            return;
        }
        emit loginRequested(login, password);
    });

    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::cancelled);

    connect(m_passwordInput, &QLineEdit::returnPressed, m_loginButton, &QPushButton::click);
    connect(m_loginInput, &QLineEdit::returnPressed, m_passwordInput, qOverload<>(&QWidget::setFocus));
}

void LoginDialog::setStatus(const QString &message)
{
    m_statusLabel->setText(message);
    m_statusLabel->setVisible(!message.isEmpty());
}

void LoginDialog::clearInputs()
{
    m_loginInput->clear();
    m_passwordInput->clear();
    m_statusLabel->hide();
}
