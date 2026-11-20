#pragma once
#include <QStackedWidget>

// Hidden pages must not impose desktop form dimensions on a phone window.
class MobileStack : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize minimumSizeHint() const override { return QSize(0, 0); }
};
