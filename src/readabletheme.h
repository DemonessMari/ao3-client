#pragma once
#include <QPalette>
#include <QWidget>
#include "buttonstyle.h"
#include "mobileui.h"

inline void applyReadableTheme(QWidget *widget)
{
    widget->setAutoFillBackground(true);
    QString sheet = secondaryButtonStyle();
    if (mobileUi()) {
        sheet += QStringLiteral(
            "QPushButton { min-height: 40px; border-radius: 10px; padding: 4px 10px; }"
            "QLineEdit, QComboBox, QSpinBox { min-height: 40px; border-radius: 8px; border: 1px solid palette(mid); padding: 2px 8px; }"
            "QCheckBox { min-height: 40px; }"
            "QCheckBox::indicator { width: 22px; height: 22px; }"
            "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 28px; border: none; }"
            "QComboBox::down-arrow { image: url(:/icons/down.png); width: 14px; height: 14px; }"
            "QTabBar::tab { min-height: 40px; padding: 4px 10px; }"
            "QListWidget::item { min-height: 36px; }");
    }
    widget->setStyleSheet(sheet);
}
