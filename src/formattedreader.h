#pragma once
#include "workmodel.h"
class QWidget;
void showFormattedReader(QWidget *parent,const Work &work,const QVector<WorkChapter> &chapters,int initial);
bool saveFormattedImages(QWidget *parent,QVector<WorkChapter> &chapters,QString &error);
