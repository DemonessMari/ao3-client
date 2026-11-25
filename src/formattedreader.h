#pragma once
#include "workmodel.h"
class QWidget;
struct FormattedPosition {int chapter=-1;double fraction=0;};
FormattedPosition showFormattedReader(QWidget *parent,const Work &work,const QVector<WorkChapter> &chapters,int initial,bool persistProgress=true);
bool saveFormattedImages(QWidget *parent,QVector<WorkChapter> &chapters,QString &error);
