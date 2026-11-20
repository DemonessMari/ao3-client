#pragma once

#include <QWidget>
#include <QLabel>
#include "workmodel.h"

class WorkItemWidget : public QWidget {
    Q_OBJECT
public:
    explicit WorkItemWidget(const Work &work, QWidget *parent = nullptr);
    const Work &work() const { return m_work; }

signals:
    void selected();

private:
    Work m_work;
};
