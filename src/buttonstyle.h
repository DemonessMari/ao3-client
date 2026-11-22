#pragma once
#include <QString>

inline QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color: palette(button); color: palette(text); border: 1px solid palette(mid); border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: palette(button); color: palette(text); }"
        "QPushButton:pressed { background-color: palette(button); color: palette(text); }"
        "QPushButton:focus { border-color: palette(highlight); }"
        "QPushButton:disabled { background-color: palette(button); color: palette(text); border-color: palette(mid); }");
}
