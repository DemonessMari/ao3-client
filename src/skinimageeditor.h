#pragma once
#include "skinimagepaint.h"
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QBuffer>
#include <functional>
#include <memory>

inline std::function<void()> addSkinImageEditor(QWidget *parent,QFormLayout *form,QJsonObject &images,QJsonArray &layers,std::function<void()> changed) {
    auto loading=std::make_shared<bool>(true);
    auto *add=new QPushButton("Add image",parent);auto *addLayer=new QPushButton("Add layer",parent);auto *removeLayer=new QPushButton("Remove layer",parent);auto *removeImage=new QPushButton("Remove image",parent);form->addRow(add,addLayer);
    auto *list=new QComboBox(parent);list->setObjectName("skinImageLayers");form->addRow("Layer",list);
    auto *image=new QComboBox(parent);form->addRow("Image",image);
    form->addRow(removeLayer,removeImage);
    auto *target=new QComboBox(parent);target->addItems({"window","header","cards"});form->addRow("Target",target);
    auto *placement=new QComboBox(parent);placement->addItems({"background","overlay","banner"});form->addRow("Placement",placement);
    auto *fit=new QComboBox(parent);fit->addItems({"contain","cover","tile"});form->addRow("Fit",fit);
    auto *anchor=new QComboBox(parent);anchor->addItems({"top-left","top-right","center","bottom-left","bottom-right"});form->addRow("Anchor",anchor);
    auto *width=new QSpinBox(parent);width->setRange(16,1024);form->addRow("Width",width);
    auto *height=new QSpinBox(parent);height->setRange(16,1024);form->addRow("Height",height);
    auto *opacity=new QDoubleSpinBox(parent);opacity->setRange(0,1);opacity->setSingleStep(.05);opacity->setDecimals(2);form->addRow("Opacity",opacity);
    auto *x=new QSpinBox(parent);x->setRange(-256,256);form->addRow("Horizontal offset",x);auto *y=new QSpinBox(parent);y->setRange(-256,256);form->addRow("Vertical offset",y);
    auto *credit=new QLineEdit(parent);credit->setMaxLength(1024);form->addRow("Image credit",credit);auto *source=new QLineEdit(parent);source->setMaxLength(1024);form->addRow("Image source",source);auto *license=new QLineEdit(parent);license->setMaxLength(1024);form->addRow("Image license",license);
    auto refresh=std::make_shared<std::function<void()>>();
    *refresh=[=,&images,&layers]{
        *loading=true;const int selected=qBound(0,list->currentIndex(),qMax(0,int(layers.size())-1));list->clear();for(int i=0;i<layers.size();++i){const auto l=layers[i].toObject();list->addItem(QString::number(i+1)+". "+l.value("target").toString()+" · "+l.value("placement").toString());}list->setCurrentIndex(layers.isEmpty()?-1:selected);
        image->clear();image->addItems(images.keys());const auto l=(selected<layers.size()?layers[selected].toObject():QJsonObject());if(!l.isEmpty())image->setCurrentText(l.value("image").toString());
        target->setCurrentText(l.value("target").toString("header"));placement->setCurrentText(l.value("placement").toString("banner"));fit->setCurrentText(l.value("fit").toString("cover"));anchor->setCurrentText(l.value("anchor").toString("center"));width->setValue(l.value("width").toInt(256));height->setMaximum(placement->currentText()=="banner"?240:1024);height->setValue(l.value("height").toInt(96));opacity->setValue(l.value("opacity").toDouble(1));x->setValue(l.value("offsetX").toInt());y->setValue(l.value("offsetY").toInt());
        const auto a=images.value(image->currentText()).toObject();credit->setText(a.value("credit").toString());source->setText(a.value("source").toString());license->setText(a.value("license").toString());
        for(QWidget *w:QList<QWidget *>{target,placement,fit,anchor,width,height,opacity,x,y,removeLayer})w->setEnabled(!layers.isEmpty());for(QWidget *w:QList<QWidget *>{image,credit,source,license,removeImage})w->setEnabled(!images.isEmpty());add->setEnabled(images.size()<4);addLayer->setEnabled(!images.isEmpty() && layers.size()<8);const bool fillsArea=placement->currentText()=="background" && fit->currentText()=="cover";width->setEnabled(!layers.isEmpty() && placement->currentText()!="banner" && !fillsArea);height->setEnabled(!layers.isEmpty() && !fillsArea);x->setEnabled(!layers.isEmpty() && placement->currentText()!="banner" && !fillsArea);y->setEnabled(!layers.isEmpty() && placement->currentText()!="banner" && !fillsArea);anchor->setEnabled(!layers.isEmpty() && placement->currentText()!="banner" && fit->currentText()!="tile" && !fillsArea);*loading=false;
    };
    auto save=[=,&images,&layers]{if(*loading)return;const int i=list->currentIndex();
        if(images.contains(image->currentText())){auto a=images.value(image->currentText()).toObject();a["credit"]=credit->text();a["source"]=source->text();a["license"]=license->text();images[image->currentText()]=a;}
        if(i<0 || i>=layers.size()){changed();return;}
        if(placement->currentText()=="banner"){*loading=true;target->setCurrentText("header");if(fit->currentText()=="tile")fit->setCurrentText("cover");height->setMaximum(240);*loading=false;}else height->setMaximum(1024);
        layers[i]=QJsonObject{{"image",image->currentText()},{"target",target->currentText()},{"placement",placement->currentText()},{"fit",fit->currentText()},{"anchor",anchor->currentText()},{"width",width->value()},{"height",height->value()},{"opacity",opacity->value()},{"offsetX",x->value()},{"offsetY",y->value()}};
        if(images.contains(image->currentText())){auto a=images.value(image->currentText()).toObject();a["credit"]=credit->text();a["source"]=source->text();a["license"]=license->text();images[image->currentText()]=a;}
        changed();
    };
    QObject::connect(list,&QComboBox::activated,parent,[=](int){(*refresh)();});
    QObject::connect(target,&QComboBox::activated,parent,[=](int){if(target->currentText()!="header" && placement->currentText()=="banner")placement->setCurrentText("overlay");save();(*refresh)();});
    for(auto *combo:{placement,fit,anchor})QObject::connect(combo,&QComboBox::activated,parent,[=](int){save();(*refresh)();});
    QObject::connect(image,&QComboBox::activated,parent,[=,&images](int){*loading=true;const auto a=images.value(image->currentText()).toObject();credit->setText(a.value("credit").toString());source->setText(a.value("source").toString());license->setText(a.value("license").toString());*loading=false;save();});
    for(auto *spin:{width,height,x,y})QObject::connect(spin,&QSpinBox::valueChanged,parent,[=](int){save();});QObject::connect(opacity,&QDoubleSpinBox::valueChanged,parent,[=](double){save();});for(auto *field:{credit,source,license})QObject::connect(field,&QLineEdit::textChanged,parent,[=]{save();});
    auto append=[=,&images,&layers](const QString &id){if(layers.size()>=8){(*refresh)();changed();return;}bool banner=true;for(const auto &v:layers)if(v.toObject().value("placement")=="banner")banner=false;layers.append(QJsonObject{{"image",id},{"target",banner?"header":"cards"},{"placement",banner?"banner":"overlay"},{"fit",banner?"cover":"contain"},{"anchor",banner?"center":"bottom-right"},{"width",128},{"height",96},{"opacity",banner?1.0:.2},{"offsetX",0},{"offsetY",0}});(*refresh)();list->setCurrentIndex(layers.size()-1);(*refresh)();changed();};
    QObject::connect(addLayer,&QPushButton::clicked,parent,[=]{append(image->currentText());});
    QObject::connect(removeLayer,&QPushButton::clicked,parent,[=,&layers]{if(list->currentIndex()>=0)layers.removeAt(list->currentIndex());(*refresh)();changed();});
    QObject::connect(removeImage,&QPushButton::clicked,parent,[=,&images,&layers]{const auto id=image->currentText();images.remove(id);QJsonArray kept;for(const auto &v:layers)if(v.toObject().value("image")!=id)kept.append(v);layers=kept;(*refresh)();changed();});
    QObject::connect(add,&QPushButton::clicked,parent,[=,&images]{
        const auto path=QFileDialog::getOpenFileName(parent,"Add skin image",{},"Images (*.png *.jpg *.jpeg)");if(path.isEmpty())return;QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>8*1024*1024){QMessageBox::warning(parent,"Cannot add image","Choose an image smaller than 8 MB.");return;}
        const auto prefix=file.peek(8);if(!prefix.startsWith(QByteArray::fromHex("89504e470d0a1a0a")) && !prefix.startsWith(QByteArray::fromHex("ffd8"))){QMessageBox::warning(parent,"Cannot add image","Choose a PNG or JPEG image.");return;}
        QImageReader reader(&file);reader.setAutoTransform(true);const auto size=reader.size();if(size.width()<=0 || size.height()<=0 || size.width()>16384 || size.height()>16384 || qint64(size.width())*size.height()>64000000){QMessageBox::warning(parent,"Cannot add image","Choose a PNG or JPEG image up to 64 megapixels.");return;}reader.setScaledSize(size.scaled(1024,1024,Qt::KeepAspectRatio).expandedTo(QSize(1,1)));auto bitmap=reader.read();if(bitmap.isNull()){QMessageBox::warning(parent,"Cannot add image","Cannot decode this image.");return;}
        const bool png=bitmap.hasAlphaChannel();QByteArray bytes;for(int attempt=0;attempt<5;++attempt){bytes.clear();QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);bitmap.save(&buffer,png?"PNG":"JPEG",85);if(!bytes.isEmpty() && bytes.size()<=512*1024)break;bitmap=bitmap.scaled(qMax(1,bitmap.width()/2),qMax(1,bitmap.height()/2),Qt::KeepAspectRatio,Qt::SmoothTransformation);}if(bytes.isEmpty() || bytes.size()>512*1024)return;
        QString id;for(int n=1;n<=5;++n){id="image-"+QString::number(n);if(!images.contains(id))break;}images[id]=QJsonObject{{"data",QString("data:image/")+(png?"png":"jpeg")+";base64,"+bytes.toBase64()},{"credit","User supplied"},{"source",""},{"license","User supplied"}};append(id);
    });
    (*refresh)();return [refresh]{(*refresh)();};
}
