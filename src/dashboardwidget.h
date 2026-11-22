#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include "workmodel.h"

class QListWidget;
class DashboardWidget : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWidget(QWidget *parent = nullptr);

    void showUserInfo(const UserInfo &user);
    void showNoUser();
    void refreshLibrary();

signals:
    void browseRequested();
    void libraryRequested();
    void skinRequested();
    void workRequested(const QString &id);
private:
    QLabel *m_greetingLabel;
    QLabel *m_infoLabel;
    QListWidget *m_recent;
    QLabel *m_empty;
};
