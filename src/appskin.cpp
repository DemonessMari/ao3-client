#include "appskin.h"
#include "readerwidget.h"
#include <QApplication>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QMessageBox>

void applyAppSkin() {
    AppSkin skin;if(!customAppSkin(skin))skin=AppSkin::preset(0);
    qApp->setPalette(skin.palette());
    for(auto *w:QApplication::allWidgets()) {w->setPalette(skin.palette());w->update();}
    qApp->setStyleSheet(qApp->styleSheet());
    for(auto *w:QApplication::allWidgets())if(!w->styleSheet().isEmpty())w->setStyleSheet(w->styleSheet());
    for(auto *window:QApplication::topLevelWidgets())for(auto *reader:window->findChildren<ReaderWidget *>())reader->refreshSkin();
}
void editAppSkin(QWidget *parent) {
    QDialog dialog(parent);dialog.setWindowTitle("App skin");dialog.resize(480,600);
    auto *layout=new QVBoxLayout(&dialog);auto *presets=new QComboBox(&dialog);presets->addItems({"Custom","Light","Dark","Sepia"});layout->addWidget(presets);
    auto *form=new QFormLayout;layout->addLayout(form);QList<QLineEdit *> fields;
    for(const auto &label:{"Name","Background","Surface","Text","Accent","Text on accent"}){auto *field=new QLineEdit(&dialog);field->setMaxLength(fields.isEmpty()?60:7);form->addRow(label,field);fields.append(field);}
    auto *preview=new QWidget(&dialog);preview->setObjectName("skinPreview");preview->setAutoFillBackground(true);auto *previewLayout=new QVBoxLayout(preview);auto *sample=new QLabel("Chapter title\n\nA preview of your reading text.",preview);sample->setWordWrap(true);previewLayout->addWidget(sample);auto *button=new QPushButton("Button",preview);button->setEnabled(false);previewLayout->addWidget(button);layout->addWidget(preview);
    auto *errorLabel=new QLabel(&dialog);errorLabel->setWordWrap(true);layout->addWidget(errorLabel);
    auto *files=new QHBoxLayout;auto *importButton=new QPushButton("Import",&dialog);auto *exportButton=new QPushButton("Export",&dialog);auto *reset=new QPushButton("Reset",&dialog);files->addWidget(importButton);files->addWidget(exportButton);files->addWidget(reset);layout->addLayout(files);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Save)->setText("Apply skin");layout->addWidget(buttons);
    auto read=[&](AppSkin &skin,QString &error){AppSkin draft{fields[0]->text(),fields[1]->text(),fields[2]->text(),fields[3]->text(),fields[4]->text(),fields[5]->text()};return AppSkin::parse(draft.json(),skin,error);};
    auto update=[&]{AppSkin skin;QString error;const bool ok=read(skin,error);errorLabel->setText(error);buttons->button(QDialogButtonBox::Save)->setEnabled(ok);exportButton->setEnabled(ok);if(ok){preview->setPalette(skin.palette());sample->setPalette(skin.palette());button->setPalette(skin.palette());preview->setStyleSheet(QString("QWidget#skinPreview { background:%1; color:%2; padding:12px; } QLabel { color:%2; background:%5; padding:8px; } QPushButton { background:%3; color:%4; padding:8px; }").arg(skin.background,skin.text,skin.accent,skin.onAccent,skin.surface));}};
    auto fill=[&](const AppSkin &skin){const QStringList values={skin.name,skin.background,skin.surface,skin.text,skin.accent,skin.onAccent};for(int i=0;i<fields.size();++i)fields[i]->setText(values[i]);update();};
    for(auto *field:fields)QObject::connect(field,&QLineEdit::textChanged,&dialog,[&]{update();});
    AppSkin initial;customAppSkin(initial);fill(initial);
    QObject::connect(presets,&QComboBox::activated,&dialog,[&](int i){if(i>0)fill(AppSkin::preset(i-1));});
    QObject::connect(reset,&QPushButton::clicked,&dialog,[&]{presets->setCurrentIndex(1);fill(AppSkin::preset(0));});
    QObject::connect(importButton,&QPushButton::clicked,&dialog,[&]{const auto path=QFileDialog::getOpenFileName(&dialog,"Import app skin",{},"Skin files (*.json)");if(path.isEmpty())return;QFile file(path);AppSkin skin;QString error;if(!file.open(QIODevice::ReadOnly)||!AppSkin::parse(file.read(16385),skin,error)){QMessageBox::warning(&dialog,"Cannot import skin",error.isEmpty()?"Cannot read the file.":error);return;}presets->setCurrentIndex(0);fill(skin);});
    QObject::connect(exportButton,&QPushButton::clicked,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;const auto path=QFileDialog::getSaveFileName(&dialog,"Export app skin","ao3-skin.json","Skin files (*.json)");if(path.isEmpty())return;QSaveFile file(path);const auto bytes=skin.json();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(&dialog,"Export failed","Could not save the skin file.");});
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;QSettings().setValue("appearance/skin",skin.json());applyAppSkin();dialog.accept();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();
}
