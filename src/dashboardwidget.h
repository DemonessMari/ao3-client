#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "workmodel.h"

class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    void showUserInfo(const UserInfo &user);
    void showNoUser();

signals:
    void browseRequested();
private:
    QLabel *m_greetingLabel;
    QLabel *m_infoLabel;
    QWidget *m_welcomeWidget;
    QWidget *m_loggedOutWidget;
};
