#pragma once
#include <QString>

inline QString secondaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background-color: #f5f0eb; color: #292723; border: 1px solid #b9afa4; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #e8e0d6; color: #292723; }"
        "QPushButton:pressed { background-color: #d9cfc2; color: #292723; }"
        "QPushButton:focus { border-color: #900; }"
        "QPushButton:disabled { background-color: #eee9e2; color: #77716a; border-color: #d5ccc2; }");
}
