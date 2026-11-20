#pragma once
#include <QtGlobal>

// The override lets desktop tests exercise the same phone layout.
inline bool mobileUi()
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return qEnvironmentVariableIntValue("AO3_MOBILE_UI") == 1;
#endif
}
