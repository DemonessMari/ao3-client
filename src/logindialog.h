#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QVBoxLayout>

class LoginDialog : public QWidget {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);

    void setStatus(const QString &message);
    void clearInputs();

signals:
    void loginRequested(const QString &login, const QString &password);
    void cancelled();

private:
    QLineEdit *m_loginInput;
    QLineEdit *m_passwordInput;
    QPushButton *m_loginButton;
    QLabel *m_statusLabel;
};
