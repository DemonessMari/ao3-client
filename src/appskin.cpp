#include "appskin.h"
#include "readerwidget.h"
#include "readerfonts.h"
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
#include <QCheckBox>
#include <QStyleHints>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionButton>
#include <QPainter>
#include <QTimer>
#include <QEvent>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace {
class NativeSkinStyle final:public QProxyStyle {
public:
    NativeSkinStyle():QProxyStyle(QStyleFactory::create("Fusion")){}
    double scale=1;int radius=8;bool custom=false;
    int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,const QWidget *widget=nullptr)const override {
        const int value=QProxyStyle::pixelMetric(metric,option,widget);
        switch(metric){case PM_LayoutLeftMargin:case PM_LayoutTopMargin:case PM_LayoutRightMargin:case PM_LayoutBottomMargin:case PM_LayoutHorizontalSpacing:case PM_LayoutVerticalSpacing:case PM_ButtonMargin:return qRound(value*scale);default:return value;}
    }
    void drawControl(ControlElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr)const override {
        if(!custom || element!=CE_PushButton){QProxyStyle::drawControl(element,option,painter,widget);return;}
        const auto *button=qstyleoption_cast<const QStyleOptionButton *>(option);if(!button)return;
        const bool enabled=button->state & State_Enabled;
        const bool active=button->state & (State_Sunken|State_On|State_MouseOver);
        const auto group=enabled?QPalette::Active:QPalette::Disabled;
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(button->palette.color(group,button->state & State_HasFocus?QPalette::Highlight:QPalette::Mid),button->state & State_HasFocus?2:1));
        painter->setBrush(button->palette.color(group,active?QPalette::Highlight:QPalette::Button));
        painter->drawRoundedRect(QRectF(button->rect).adjusted(1,1,-1,-1),radius,radius);painter->restore();
        auto label=*button;label.palette.setColor(QPalette::ButtonText,button->palette.color(group,active?QPalette::HighlightedText:QPalette::ButtonText));
        QProxyStyle::drawControl(CE_PushButtonLabel,&label,painter,widget);
    }
};
NativeSkinStyle *nativeStyle=nullptr;
void skinButton(QWidget *widget,bool custom) {
    auto *button=qobject_cast<QPushButton *>(widget);if(!button)return;
    if(custom){if(!button->property("ao3OriginalStyle").isValid())button->setProperty("ao3OriginalStyle",button->styleSheet());if(!button->styleSheet().isEmpty())button->setStyleSheet({});}
    else if(button->property("ao3OriginalStyle").isValid()){button->setStyleSheet(button->property("ao3OriginalStyle").toString());button->setProperty("ao3OriginalStyle",QVariant());}
}
class SkinPolish final:public QObject {
public:using QObject::QObject;
    bool eventFilter(QObject *object,QEvent *event)override {
        if(event->type()==QEvent::Polish && nativeStyle && nativeStyle->custom)if(auto *widget=qobject_cast<QWidget *>(object))skinButton(widget,true);
        return false;
    }
};
}


AppSkin resolvedAppSkin(bool dark) {
    if(QSettings().value("appearance/followSystem",false).toBool())return AppSkin::preset(dark?1:0);
    AppSkin skin;if(!customAppSkin(skin))skin=AppSkin::preset(0);return skin;
}
bool readerAppSkin(AppSkin &skin) {
    if(QSettings().value("appearance/followSystem",false).toBool()){skin=resolvedAppSkin(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark);return true;}
    return customAppSkin(skin);
}

void applyReaderInterfaceFont() {
    static const QFont systemFont=QApplication::font();
    static QString applied;
    const auto selected=QSettings().value("reader/fontFamily","serif").toString();
    const bool enabled=QSettings().value("reader/useImportedFontInInterface",true).toBool();
    bool available=false;
    if(enabled && readerFontId(selected))for(const auto &font:readerFonts())if(font.id==selected){available=true;break;}
    const QString target=available?selected:QStringLiteral("system");
    const auto font=target=="system"?systemFont:readerFont(target);
    const QString resolved=target+":"+font.family();
    if(applied==resolved)return;
    applied=resolved;
    QFont interfaceFont=systemFont;interfaceFont.setFamilies(font.families());
    QApplication::setFont(interfaceFont);
    for(auto *widget:QApplication::allWidgets()) {
        auto current=widget->font();current.setFamilies(font.families());widget->setFont(current);
    }
}

void applyAppSkin() {
    applyReaderInterfaceFont();
    const auto skin=resolvedAppSkin(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark);
    if(!nativeStyle && skin.version==2){nativeStyle=new NativeSkinStyle;qApp->setStyle(nativeStyle);qApp->installEventFilter(new SkinPolish(qApp));}
    if(nativeStyle){nativeStyle->custom=skin.version==2;nativeStyle->scale=skin.uiScale;nativeStyle->radius=skin.cornerRadius;}
    static const double baseSize=QApplication::font().pointSizeF();
    static double previousScale=1;auto font=QApplication::font();font.setPointSizeF(baseSize*skin.uiScale);
    for(auto *widget:QApplication::allWidgets()){
        skinButton(widget,skin.version==2);
        bool reader=false;for(auto *parent=widget;parent;parent=parent->parentWidget())if(qobject_cast<ReaderWidget *>(parent)){reader=true;break;}
        if(reader)continue;
        auto current=widget->font();if(!widget->property("ao3BaseFontSize").isValid())widget->setProperty("ao3BaseFontSize",current.pointSizeF()/previousScale);
        auto size=widget->property("ao3BaseFontSize").toDouble();if(!std::isfinite(size) || size<=0){size=current.pointSizeF();widget->setProperty("ao3BaseFontSize",size);}
        current.setPointSizeF(size*skin.uiScale);widget->setFont(current);widget->setProperty("ao3FontScale",skin.uiScale);
    }
    QApplication::setFont(font);previousScale=skin.uiScale;
    qApp->setPalette(skin.palette());
    for(auto *w:QApplication::allWidgets()) {w->setPalette(skin.palette());w->update();}
    qApp->setStyleSheet(qApp->styleSheet());
    for(auto *w:QApplication::allWidgets())if(!w->styleSheet().isEmpty())w->setStyleSheet(w->styleSheet());
    for(auto *window:QApplication::topLevelWidgets())for(auto *reader:window->findChildren<ReaderWidget *>())reader->refreshSkin();
}
void editAppSkin(QWidget *parent) {
    QDialog dialog(parent);dialog.setWindowTitle("App skin");dialog.resize(480,600);
    auto *layout=new QVBoxLayout(&dialog);auto *presets=new QComboBox(&dialog);presets->addItems({"Custom","Light","Dark","Sepia"});layout->addWidget(presets);
    auto *followSystem=new QCheckBox("Follow system theme",&dialog);followSystem->setObjectName("followSystemTheme");followSystem->setChecked(QSettings().value("appearance/followSystem",false).toBool());layout->addWidget(followSystem);
    auto *form=new QFormLayout;layout->addLayout(form);QList<QLineEdit *> fields;
    for(const auto &label:{"Name","Background","Surface","Text","Accent","Text on accent"}){auto *field=new QLineEdit(&dialog);field->setMaxLength(fields.isEmpty()?60:7);form->addRow(label,field);fields.append(field);}
    auto *scale=new QDoubleSpinBox(&dialog);scale->setRange(1,1.5);scale->setSingleStep(.05);scale->setDecimals(2);form->addRow("Interface scale",scale);
    auto *radius=new QSpinBox(&dialog);radius->setRange(0,24);form->addRow("Control corners",radius);
    auto *preview=new QWidget(&dialog);preview->setObjectName("skinPreview");preview->setAutoFillBackground(true);auto *previewLayout=new QVBoxLayout(preview);auto *sample=new QLabel("Chapter title\n\nA preview of your reading text.",preview);sample->setWordWrap(true);previewLayout->addWidget(sample);auto *button=new QPushButton("Button",preview);button->setEnabled(false);auto *previewStyle=new NativeSkinStyle;previewStyle->setParent(preview);previewStyle->custom=true;button->setStyle(previewStyle);previewLayout->addWidget(button);layout->addWidget(preview);
    auto *errorLabel=new QLabel(&dialog);errorLabel->setWordWrap(true);layout->addWidget(errorLabel);
    auto *files=new QHBoxLayout;auto *importButton=new QPushButton("Import",&dialog);auto *exportButton=new QPushButton("Export",&dialog);auto *reset=new QPushButton("Reset",&dialog);files->addWidget(importButton);files->addWidget(exportButton);files->addWidget(reset);layout->addLayout(files);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Save)->setText("Apply skin");layout->addWidget(buttons);
    auto read=[&](AppSkin &skin,QString &error){AppSkin draft{fields[0]->text(),fields[1]->text(),fields[2]->text(),fields[3]->text(),fields[4]->text(),fields[5]->text()};draft.version=2;draft.uiScale=scale->value();draft.cornerRadius=radius->value();return AppSkin::parse(draft.json(),skin,error);};
    auto update=[&]{AppSkin skin;QString error;const bool ok=read(skin,error);errorLabel->setText(error);buttons->button(QDialogButtonBox::Save)->setEnabled(ok);exportButton->setEnabled(ok);if(ok){preview->setPalette(skin.palette());sample->setPalette(skin.palette());button->setPalette(skin.palette());previewStyle->scale=skin.uiScale;previewStyle->radius=skin.cornerRadius;auto sampleFont=dialog.font();sampleFont.setPointSizeF(sampleFont.pointSizeF()*skin.uiScale);sample->setFont(sampleFont);button->setFont(sampleFont);button->update();}};
    auto fill=[&](const AppSkin &skin){const QStringList values={skin.name,skin.background,skin.surface,skin.text,skin.accent,skin.onAccent};for(int i=0;i<fields.size();++i)fields[i]->setText(values[i]);scale->setValue(skin.uiScale);radius->setValue(skin.cornerRadius);update();};
    for(auto *field:fields)QObject::connect(field,&QLineEdit::textChanged,&dialog,[&]{update();});
    QObject::connect(scale,&QDoubleSpinBox::valueChanged,&dialog,[&]{update();});QObject::connect(radius,&QSpinBox::valueChanged,&dialog,[&]{update();});
    AppSkin manual;customAppSkin(manual);
    auto updateMode=[&]{scale->setEnabled(!followSystem->isChecked());radius->setEnabled(!followSystem->isChecked());presets->setEnabled(!followSystem->isChecked());for(auto *field:fields)field->setEnabled(!followSystem->isChecked());if(followSystem->isChecked())fill(AppSkin::preset(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark?1:0));};
    AppSkin initial=resolvedAppSkin(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark);fill(initial);updateMode();
    QObject::connect(followSystem,&QCheckBox::toggled,&dialog,[&]{if(followSystem->isChecked()){QString error;read(manual,error);}else fill(manual);updateMode();});
    QObject::connect(presets,&QComboBox::activated,&dialog,[&](int i){if(i>0)fill(AppSkin::preset(i-1));});
    QObject::connect(reset,&QPushButton::clicked,&dialog,[&]{followSystem->setChecked(false);presets->setCurrentIndex(1);fill(AppSkin::preset(0));});
    QObject::connect(importButton,&QPushButton::clicked,&dialog,[&]{const auto path=QFileDialog::getOpenFileName(&dialog,"Import app skin",{},"Skin files (*.json)");if(path.isEmpty())return;QFile file(path);AppSkin skin;QString error;if(!file.open(QIODevice::ReadOnly)||!AppSkin::parse(file.read(16385),skin,error)){QMessageBox::warning(&dialog,"Cannot import skin",error.isEmpty()?"Cannot read the file.":error);return;}followSystem->setChecked(false);presets->setCurrentIndex(0);fill(skin);});
    QObject::connect(exportButton,&QPushButton::clicked,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;const auto path=QFileDialog::getSaveFileName(&dialog,"Export app skin","ao3-skin.json","Skin files (*.json)");if(path.isEmpty())return;QSaveFile file(path);const auto bytes=skin.json();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(&dialog,"Export failed","Could not save the skin file.");});
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;QSettings settings;settings.setValue("appearance/followSystem",followSystem->isChecked());if(!followSystem->isChecked())settings.setValue("appearance/skin",skin.json());applyAppSkin();dialog.accept();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();
}
