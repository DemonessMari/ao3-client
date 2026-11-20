#pragma once
#include <QPalette>
#include <QWidget>
#include "buttonstyle.h"
#include "mobileui.h"

inline void applyReadableTheme(QWidget *widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, QColor("#faf8f3"));
    palette.setColor(QPalette::WindowText, QColor("#292723"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f5f0eb"));
    palette.setColor(QPalette::Text, QColor("#292723"));
    palette.setColor(QPalette::Button, QColor("#f5f0eb"));
    palette.setColor(QPalette::ButtonText, QColor("#292723"));
    palette.setColor(QPalette::Highlight, QColor("#990000"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor("#706b65"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#77716a"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#77716a"));
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
    QString sheet = QStringLiteral(
        "QLabel, QCheckBox { color: #292723; background: transparent; }"
        "QCheckBox { spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #8b8176; border-radius: 3px; background: #ffffff; }"
        "QCheckBox::indicator:checked { background: #990000; border-color: #990000; image: url(:/icons/check.png); }"
        "QCheckBox::indicator:focus { border-color: #990000; }"
        "QLineEdit, QComboBox, QSpinBox { color: #292723; background-color: #ffffff; selection-background-color: #990000; selection-color: #ffffff; min-height: 26px; }"
        "QComboBox QAbstractItemView { color: #292723; background: #ffffff; selection-background-color: #990000; selection-color: #ffffff; }") + secondaryButtonStyle();
    if (mobileUi()) {
        sheet += QStringLiteral(
            "QPushButton { min-height: 40px; border-radius: 10px; padding: 4px 10px; }"
            "QLineEdit, QComboBox, QSpinBox { min-height: 40px; border-radius: 8px; border: 1px solid #c9beb3; padding: 2px 8px; }"
            "QCheckBox { min-height: 40px; }"
            "QCheckBox::indicator { width: 22px; height: 22px; }"
            "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 28px; border: none; }"
            "QComboBox::down-arrow { image: url(:/icons/down.png); width: 14px; height: 14px; }"
            "QTabBar::tab { min-height: 40px; padding: 4px 10px; }"
            "QListWidget::item { min-height: 36px; }");
    }
    widget->setStyleSheet(sheet);
}
