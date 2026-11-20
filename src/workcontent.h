#pragma once
#include <QString>
#include "workmodel.h"

// Extract chapter bodies without summaries, navigation, or page furniture.
QString extractWorkText(const QString &html);

QVector<WorkChapter> extractWorkChapters(const QString &html);
