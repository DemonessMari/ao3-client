#pragma once
#include <QSettings>
inline int readerTapAction(double fraction) {
    QSettings settings;const double edge=qBound(20,settings.value("reader/tapEdge",30).toInt(),40)/100.0;
    const QString region=fraction<edge?"Left":fraction>1-edge?"Right":"Center";
    const int fallback=region=="Left"?-1:region=="Right"?1:2;
    const int action=settings.value("reader/tap"+region,fallback).toInt();
    return action>=-1 && action<=2?action:fallback;
}
