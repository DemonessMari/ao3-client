#include "appskin.h"
#include "skinimagepaint.h"
#include "skinimageeditor.h"
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
#include <QTextEdit>
#include <QAbstractSpinBox>
#include <QScrollArea>
#include <QJsonArray>
#include <QTabWidget>
#include <QListWidget>
#include <QPointer>
#include <QMenu>
#include <algorithm>
#include <QScreen>

namespace {
QString skinRole(const QWidget *widget) {
    for(auto *w=widget;w;w=w->parentWidget()){if(w->objectName()=="skinPreview")break;if(qobject_cast<const QMenu *>(w) || w->property("skinEditor").toBool())return {};}
    if(qobject_cast<const QPushButton *>(widget))return "buttons";
    if(qobject_cast<const QLineEdit *>(widget)||qobject_cast<const QComboBox *>(widget)||qobject_cast<const QAbstractSpinBox *>(widget)||qobject_cast<const QTextEdit *>(widget))return "inputs";
    for(auto *w=widget;w;w=w->parentWidget())if(!w->property("skinRole").toString().isEmpty())return w->property("skinRole").toString();
    return {};
}
class NativeSkinStyle final:public QProxyStyle {
public:
    NativeSkinStyle():QProxyStyle(QStyleFactory::create("Fusion")){}
    double scale=1;int radius=8;bool custom=false;AppSkin skin;
    int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,const QWidget *widget=nullptr)const override {
        const auto rule=skin.component(skinRole(widget));
        if(metric==PM_ButtonMargin && rule.contains("padding"))return rule.value("padding").toInt()*2;
        const int value=QProxyStyle::pixelMetric(metric,option,widget);
        switch(metric){case PM_LayoutLeftMargin:case PM_LayoutTopMargin:case PM_LayoutRightMargin:case PM_LayoutBottomMargin:case PM_LayoutHorizontalSpacing:case PM_LayoutVerticalSpacing:case PM_ButtonMargin:return qRound(value*scale);default:return value;}
    }
    QSize sizeFromContents(ContentsType type,const QStyleOption *option,const QSize &contents,const QWidget *widget=nullptr)const override {
        auto result=QProxyStyle::sizeFromContents(type,option,contents,widget);
        const auto rule=skin.component(skinRole(widget));
        if(rule.contains("padding") && (type==CT_LineEdit || type==CT_ComboBox || type==CT_SpinBox))result+=QSize(2*rule.value("padding").toInt(),2*rule.value("padding").toInt());
        return result;
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr)const override {
        const auto rule=skin.component("inputs");
        if(custom && !rule.isEmpty() && skinRole(widget)=="inputs" && (element==PE_PanelLineEdit || element==PE_FrameLineEdit)){
            painter->save();painter->setRenderHint(QPainter::Antialiasing);const double width=rule.value("borderWidth").toDouble(1);
            painter->setPen(width>0?QPen(QColor(rule.value("border").toString(skin.text)),width):QPen(Qt::NoPen));painter->setBrush(QColor(rule.value("background").toString(skin.surface)));
            const int r=rule.value("cornerRadius").toInt(radius);painter->drawRoundedRect(QRectF(option->rect).adjusted(1,1,-1,-1),r,r);painter->restore();return;
        }
        QProxyStyle::drawPrimitive(element,option,painter,widget);
    }
    void drawComplexControl(ComplexControl control,const QStyleOptionComplex *option,QPainter *painter,const QWidget *widget=nullptr)const override {
        const auto rule=skin.component("inputs");
        if(!custom || rule.isEmpty() || skinRole(widget)!="inputs" || (control!=CC_ComboBox && control!=CC_SpinBox)){QProxyStyle::drawComplexControl(control,option,painter,widget);return;}
        QStyleOption panel;panel.rect=option->rect;panel.state=option->state;panel.palette=option->palette;drawPrimitive(PE_PanelLineEdit,&panel,painter,widget);
        if(control==CC_ComboBox){QStyleOption arrow=*option;arrow.rect=subControlRect(control,option,SC_ComboBoxArrow,widget).adjusted(4,4,-4,-4);QProxyStyle::drawPrimitive(PE_IndicatorArrowDown,&arrow,painter,widget);}
        else for(auto pair:QList<QPair<SubControl,PrimitiveElement>>{{SC_SpinBoxUp,PE_IndicatorArrowUp},{SC_SpinBoxDown,PE_IndicatorArrowDown}}){QStyleOption arrow=*option;arrow.rect=subControlRect(control,option,pair.first,widget).adjusted(3,2,-3,-2);QProxyStyle::drawPrimitive(pair.second,&arrow,painter,widget);}
    }
    void drawControl(ControlElement element,const QStyleOption *option,QPainter *painter,const QWidget *widget=nullptr)const override {
        if(!custom || element!=CE_PushButton){QProxyStyle::drawControl(element,option,painter,widget);return;}
        const auto *button=qstyleoption_cast<const QStyleOptionButton *>(option);if(!button)return;
        const bool enabled=button->state & State_Enabled;
        const bool active=button->state & (State_Sunken|State_On|State_MouseOver);
        const auto group=enabled?QPalette::Active:QPalette::Disabled;
        painter->save();painter->setRenderHint(QPainter::Antialiasing);
        const auto rule=skinRole(widget)=="buttons"?skin.component("buttons"):QJsonObject();const double width=rule.value("borderWidth").toDouble(button->state & State_HasFocus?2:1);
        painter->setPen(width>0?QPen(QColor(rule.value("border").toString(button->palette.color(group,button->state & State_HasFocus?QPalette::Highlight:QPalette::Mid).name())),width):QPen(Qt::NoPen));
        painter->setBrush(button->palette.color(group,active?QPalette::Highlight:QPalette::Button));
        const int r=rule.value("cornerRadius").toInt(radius);painter->drawRoundedRect(QRectF(button->rect).adjusted(1,1,-1,-1),r,r);painter->restore();
        auto label=*button;label.palette.setColor(QPalette::ButtonText,button->palette.color(group,active?QPalette::HighlightedText:QPalette::ButtonText));
        QProxyStyle::drawControl(CE_PushButtonLabel,&label,painter,widget);
    }
};
class SkinPreviewPanel final:public QWidget {
public:
    AppSkin skin;QString role;
    SkinPreviewPanel(const QString &value,QWidget *parent):QWidget(parent),role(value){}
protected:
    void paintEvent(QPaintEvent *)override {
        const auto rule=skin.component(role);const auto bg=rule.value("background").toString(role=="header"?skin.background:skin.surface);
        QPainter p(this);const auto fg=rule.value("text").toString(skin.text);const double width=rule.value("borderWidth").toDouble(role=="cards" || rule.contains("border")?1:0);const int radius=rule.value("cornerRadius").toInt(skin.cornerRadius);
        p.setRenderHint(QPainter::Antialiasing);QPainterPath path;path.addRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),radius,radius);p.fillPath(path,QColor(bg));p.save();p.setClipPath(path);const int banner=role=="header"?skinBannerHeight(skin,.5):0;
        drawSkinLayers(p,skin,role,QRectF(0,0,this->width(),height()-banner),bg,fg,.5);if(banner)drawSkinLayers(p,skin,role,QRectF(0,height()-banner,this->width(),banner),bg,fg,.5,true);p.restore();if(width>0){p.setPen(QPen(QColor(rule.value("border").toString(skin.text)),width));p.drawPath(path);}

    }
};
QPointer<NativeSkinStyle> nativeStyle;
void skinButton(QWidget *widget,bool custom) {
    auto *button=qobject_cast<QPushButton *>(widget);if(!button)return;
    if(custom){if(!button->property("ao3OriginalStyle").isValid())button->setProperty("ao3OriginalStyle",button->styleSheet());if(!button->styleSheet().isEmpty())button->setStyleSheet({});}
    else if(button->property("ao3OriginalStyle").isValid()){button->setStyleSheet(button->property("ao3OriginalStyle").toString());button->setProperty("ao3OriginalStyle",QVariant());}
}
void configureSkinBanner(QWidget *widget) {
    if(!nativeStyle || widget->property("skinRole").toString()!="header" || !widget->layout())return;
    if(!widget->property("ao3BaseMargins").isValid()){const auto m=widget->layout()->contentsMargins();widget->setProperty("ao3BaseMargins",QVariantList{m.left(),m.top(),m.right(),m.bottom()});}
    if(!widget->property("ao3BannerMinimum").isValid())widget->setProperty("ao3BannerMinimum",widget->minimumHeight());
    const auto margins=widget->property("ao3BaseMargins").toList();const auto rule=nativeStyle->skin.component("header");const int banner=skinBannerHeight(nativeStyle->skin,nativeStyle->scale);
    const int padding=rule.value("padding").toInt(-1);widget->layout()->setContentsMargins(padding<0?margins[0].toInt():padding,padding<0?margins[1].toInt():padding,padding<0?margins[2].toInt():padding,(padding<0?margins[3].toInt():padding)+banner);
    widget->setMinimumHeight(widget->property("ao3BannerMinimum").toInt()+banner);
}
void componentWidget(QWidget *widget) {
    if(!nativeStyle)return;
    for(auto *parent=widget;parent;parent=parent->parentWidget())if(parent->objectName()=="skinPreview")return;
    bool editor=false,reader=false;for(auto *parent=widget;parent;parent=parent->parentWidget()){editor|=parent->property("skinEditor").toBool();reader|=qobject_cast<ReaderWidget *>(parent)!=nullptr;}
    if(auto *scroll=qobject_cast<QScrollArea *>(widget);scroll && !editor && !reader){
        bool decorated=false;for(const auto &v:nativeStyle->skin.layers)if(v.toObject().value("target")=="window")decorated=true;
        auto *viewport=scroll->viewport();if(!viewport->property("ao3OriginalFill").isValid())viewport->setProperty("ao3OriginalFill",viewport->autoFillBackground());viewport->setAutoFillBackground(decorated?false:viewport->property("ao3OriginalFill").toBool());viewport->setProperty("skinLayerTarget",decorated?QVariant("window"):QVariant());
        if(auto *body=scroll->widget()){if(!body->property("ao3OriginalFill").isValid())body->setProperty("ao3OriginalFill",body->autoFillBackground());body->setAutoFillBackground(decorated?false:body->property("ao3OriginalFill").toBool());}
    }
    const auto rule=nativeStyle->skin.component(skinRole(widget));
    const bool overrideStyle=nativeStyle->skin.version>=3 && !rule.isEmpty();
    if(overrideStyle && !qobject_cast<QPushButton *>(widget)){if(!widget->property("ao3ComponentStyle").isValid())widget->setProperty("ao3ComponentStyle",widget->styleSheet());if(!widget->styleSheet().isEmpty())widget->setStyleSheet({});}
    else if(widget->property("ao3ComponentStyle").isValid()){widget->setStyleSheet(widget->property("ao3ComponentStyle").toString());widget->setProperty("ao3ComponentStyle",QVariant());}
    if(widget->property("ao3OriginalMinimumHeight").isValid()){
        widget->setMinimumHeight(widget->property("ao3OriginalMinimumHeight").toInt());widget->setMaximumHeight(widget->property("ao3OriginalMaximumHeight").toInt());
    }
    if(!overrideStyle){configureSkinBanner(widget);return;}
    const auto role=skinRole(widget);const auto &skin=nativeStyle->skin;
    const QColor bg(rule.value("background").toString(role=="buttons"?skin.accent:role=="header"?skin.background:skin.surface)),fg(rule.value("text").toString(role=="buttons"?skin.onAccent:skin.text));
    auto palette=widget->palette();
    for(auto group:{QPalette::Active,QPalette::Inactive,QPalette::Disabled}){
        for(auto r:{QPalette::Window,QPalette::Base,QPalette::Button})palette.setColor(group,r,bg);
        for(auto r:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText,QPalette::PlaceholderText})palette.setColor(group,r,fg);
        palette.setColor(group,QPalette::Highlight,fg);palette.setColor(group,QPalette::HighlightedText,bg);
        palette.setColor(group,QPalette::Link,fg);
    }
    widget->setPalette(palette);
    if(!widget->property("ao3ComponentBaseSize").isValid()){
        const double inherited=widget->testAttribute(Qt::WA_SetFont)?1:nativeStyle->skin.component(skinRole(widget->parentWidget())).value("textScale").toDouble(1);
        widget->setProperty("ao3ComponentBaseSize",widget->font().pointSizeF()/nativeStyle->scale/inherited);
    }
    auto font=widget->font();font.setPointSizeF(widget->property("ao3ComponentBaseSize").toDouble()*nativeStyle->scale*rule.value("textScale").toDouble(1));widget->setFont(font);
    if(widget->property("skinRole").isValid() && widget->layout()){
        if(!widget->property("ao3BaseMargins").isValid()){const auto m=widget->layout()->contentsMargins();widget->setProperty("ao3BaseMargins",QVariantList{m.left(),m.top(),m.right(),m.bottom()});}
        if(rule.contains("padding")){const int n=rule.value("padding").toInt();widget->layout()->setContentsMargins(n,n,n,n);}
    }
    if((role=="buttons" || role=="inputs") && (rule.contains("padding") || rule.contains("textScale"))){
        if(!widget->property("ao3OriginalMinimumHeight").isValid()){widget->setProperty("ao3OriginalMinimumHeight",widget->minimumHeight());widget->setProperty("ao3OriginalMaximumHeight",widget->maximumHeight());}
        const int height=widget->fontMetrics().height()+2*rule.value("padding").toInt(6)+8;
        widget->setMaximumHeight(qMax(widget->maximumHeight(),height));widget->setMinimumHeight(qMax(widget->minimumHeight(),height));
    }
    configureSkinBanner(widget);widget->updateGeometry();widget->update();
    if(widget->property("skinRole").toString()=="cards")for(auto *parent=widget->parentWidget();parent;parent=parent->parentWidget())if(auto *list=qobject_cast<QListWidget *>(parent)){for(int i=0;i<list->count();++i)if(list->itemWidget(list->item(i))==widget)list->item(i)->setSizeHint(widget->sizeHint());break;}
}
class SkinPolish final:public QObject {
public:using QObject::QObject;
    bool eventFilter(QObject *object,QEvent *event)override {
        auto *widget=qobject_cast<QWidget *>(object);if(!widget || !nativeStyle)return false;
        if(event->type()==QEvent::Polish && nativeStyle->custom)QTimer::singleShot(0,widget,[widget]{if(nativeStyle && nativeStyle->custom){skinButton(widget,true);componentWidget(widget);}});
        if(event->type()==QEvent::Paint && nativeStyle->skin.version>=3 && (widget->property("skinRole").isValid() || widget->property("skinLayerTarget").isValid())){
            const auto role=widget->property("skinLayerTarget").isValid()?widget->property("skinLayerTarget").toString():widget->property("skinRole").toString();const auto rule=nativeStyle->skin.component(role);
            bool decorated=false;for(const auto &v:nativeStyle->skin.layers)if(v.toObject().value("target")==role)decorated=true;
            if(rule.isEmpty() && !decorated)return false;
            const auto &skin=nativeStyle->skin;const auto bg=rule.value("background").toString(role=="window" || role=="header"?skin.background:skin.surface),fg=rule.value("text").toString(skin.text);
            QPainter p(widget);p.setRenderHint(QPainter::Antialiasing);const double width=rule.value("borderWidth").toDouble(role=="cards" || rule.contains("border")?1:0);const int radius=role=="window"?0:rule.value("cornerRadius").toInt(nativeStyle->radius);
            QPainterPath path;path.addRoundedRect(QRectF(widget->rect()).adjusted(width/2,width/2,-width/2,-width/2),radius,radius);p.fillPath(path,QColor(bg));p.save();p.setClipPath(path);const int banner=role=="header"?skinBannerHeight(skin,nativeStyle->scale):0;
            drawSkinLayers(p,skin,role,QRectF(0,0,widget->width(),widget->height()-banner),bg,fg,nativeStyle->scale);
            if(banner)drawSkinLayers(p,skin,role,QRectF(0,widget->height()-banner,widget->width(),banner),bg,fg,nativeStyle->scale,true);p.restore();if(width>0){p.setPen(QPen(QColor(rule.value("border").toString(skin.text)),width));p.drawPath(path);}return true;
        }
        return false;
    }
};}


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
    if(!nativeStyle && skin.version>=2){nativeStyle=new NativeSkinStyle;qApp->setStyle(nativeStyle);static bool filterInstalled=false;if(!filterInstalled){qApp->installEventFilter(new SkinPolish(qApp));filterInstalled=true;}}
    if(nativeStyle){nativeStyle->skin=skin;nativeStyle->custom=skin.version>=2;nativeStyle->scale=skin.uiScale;nativeStyle->radius=skin.cornerRadius;}
    static const double baseSize=QApplication::font().pointSizeF();
    static double previousScale=1;auto font=QApplication::font();font.setPointSizeF(baseSize*skin.uiScale);
    for(auto *widget:QApplication::allWidgets()){
        skinButton(widget,skin.version>=2);
        bool reader=false;for(auto *parent=widget;parent;parent=parent->parentWidget())if(qobject_cast<ReaderWidget *>(parent)){reader=true;break;}
        if(reader)continue;
        auto current=widget->font();if(!widget->property("ao3BaseFontSize").isValid())widget->setProperty("ao3BaseFontSize",current.pointSizeF()/previousScale);
        auto size=widget->property("ao3BaseFontSize").toDouble();if(!std::isfinite(size) || size<=0){size=current.pointSizeF();widget->setProperty("ao3BaseFontSize",size);}
        current.setPointSizeF(size*skin.uiScale);widget->setFont(current);widget->setProperty("ao3FontScale",skin.uiScale);
    }
    QApplication::setFont(font);previousScale=skin.uiScale;
    qApp->setPalette(skin.palette());
    auto widgets=QApplication::allWidgets();
    auto depth=[](const QWidget *w){int n=0;for(;w;w=w->parentWidget())++n;return n;};
    std::stable_sort(widgets.begin(),widgets.end(),[&](const QWidget *a,const QWidget *b){return depth(a)<depth(b);});
    for(auto *w:widgets) {
        w->setPalette(skin.palette());
        if(w->property("ao3ComponentBaseSize").isValid()){auto f=w->font();f.setPointSizeF(w->property("ao3ComponentBaseSize").toDouble()*skin.uiScale);w->setFont(f);}
        if(w->layout() && w->property("ao3BaseMargins").isValid()){const auto m=w->property("ao3BaseMargins").toList();w->layout()->setContentsMargins(m[0].toInt(),m[1].toInt(),m[2].toInt(),m[3].toInt());}
        w->update();
    }
    qApp->setStyleSheet(qApp->styleSheet());
    for(auto *w:widgets)if(!w->styleSheet().isEmpty())w->setStyleSheet(w->styleSheet());
    for(auto *w:widgets)componentWidget(w);
    for(auto *window:QApplication::topLevelWidgets())for(auto *reader:window->findChildren<ReaderWidget *>())reader->refreshSkin();
}
void editAppSkin(QWidget *parent) {
    QDialog dialog(parent);dialog.setProperty("skinEditor",true);dialog.setWindowTitle("App skin");dialog.resize(600,qMin(820,qMax(560,dialog.screen()->availableGeometry().height()-40)));
    auto *layout=new QVBoxLayout(&dialog);
    auto *presets=new QComboBox(&dialog);presets->addItems({"Custom","Light","Dark","Sepia","Neos Tumblr Blue","Neos Charcoal Rose","Neos Nebula"});layout->addWidget(presets);
    layout->addWidget(new QLabel("Neos adaptations · ZerafinaCSS · MIT",&dialog));
    auto *follow=new QCheckBox("Follow system theme",&dialog);follow->setObjectName("followSystemTheme");follow->setChecked(QSettings().value("appearance/followSystem",false).toBool());layout->addWidget(follow);
    auto *tabs=new QTabWidget(&dialog);layout->addWidget(tabs,1);
    auto addTab=[&](const QString &name){auto *scroll=new QScrollArea(tabs);scroll->setWidgetResizable(true);auto *body=new QWidget(scroll);scroll->setWidget(body);tabs->addTab(scroll,name);auto *form=new QFormLayout(body);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);form->setLabelAlignment(Qt::AlignLeft|Qt::AlignVCenter);form->setFormAlignment(Qt::AlignTop);form->setSizeConstraint(QLayout::SetMinAndMaxSize);form->setVerticalSpacing(8);form->setContentsMargins(16,16,16,16);return form;};
    auto *form=addTab("Theme");QList<QLineEdit *> fields;
    for(const auto &label:{"Name","Background","Surface","Text","Accent","Text on accent"}){auto *field=new QLineEdit(&dialog);field->setMaxLength(fields.isEmpty()?60:7);field->setMinimumHeight(32);form->addRow(label,field);fields.append(field);}
    auto *scale=new QDoubleSpinBox(&dialog);scale->setRange(1,1.5);scale->setSingleStep(.05);scale->setDecimals(2);form->addRow("Interface scale",scale);
    auto *radius=new QSpinBox(&dialog);radius->setRange(0,24);form->addRow("Default corners",radius);
    QMap<QString,QMap<QString,QLineEdit *>> componentFields;
    QJsonObject imageData;QJsonArray layerData;std::function<void()> refreshImages;
    for(const auto &role:QStringList{"buttons","inputs","cards","header"}){
        auto *componentForm=addTab(role.left(1).toUpper()+role.mid(1));
        auto *hint=new QLabel("Leave a field empty to inherit the theme.",&dialog);hint->setWordWrap(true);componentForm->addRow(hint);
        for(const auto &pair:QList<QPair<QString,QString>>{{"background","Background"},{"text","Text"},{"border","Border"},{"cornerRadius","Corners (0–24)"},{"padding","Spacing (0–24)"},{"textScale","Text scale (0.85–1.5)"},{"borderWidth","Border width (0–3)"}}){
            auto *field=new QLineEdit(&dialog);field->setMaxLength(7);field->setMinimumHeight(32);field->setPlaceholderText("Inherit");field->setObjectName("skin_"+role+"_"+pair.first);componentFields[role][pair.first]=field;componentForm->addRow(pair.second,field);
        }
    }
    auto *imageForm=addTab("Images");
    auto *preview=new QWidget(&dialog);preview->setObjectName("skinPreview");preview->setAutoFillBackground(true);auto *pv=new QVBoxLayout(preview);
    auto *header=new SkinPreviewPanel("header",preview);auto *headerLayout=new QVBoxLayout(header);headerLayout->addWidget(new QLabel("Archive of Our Own",header));pv->addWidget(header);
    auto *card=new SkinPreviewPanel("cards",preview);auto *cardLayout=new QVBoxLayout(card);auto *cardText=new QLabel("Chapter title\nA preview of your reading text.",card);cardText->setWordWrap(true);cardLayout->addWidget(cardText);pv->addWidget(card);
    auto *input=new QLineEdit("Search preview",preview);input->setReadOnly(true);pv->addWidget(input);
    auto *button=new QPushButton("Button",preview);pv->addWidget(button);layout->addWidget(preview);
    auto *previewStyle=new NativeSkinStyle;previewStyle->setParent(preview);previewStyle->custom=true;button->setStyle(previewStyle);input->setStyle(previewStyle);
    auto *errorLabel=new QLabel(&dialog);errorLabel->setWordWrap(true);layout->addWidget(errorLabel);
    auto *files=new QHBoxLayout;auto *importButton=new QPushButton("Import",&dialog);auto *exportButton=new QPushButton("Export",&dialog);auto *reset=new QPushButton("Reset",&dialog);files->addWidget(importButton);files->addWidget(exportButton);files->addWidget(reset);layout->addLayout(files);
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,&dialog);buttons->button(QDialogButtonBox::Save)->setText("Apply skin");layout->addWidget(buttons);
    bool filling=false;
    auto read=[&](AppSkin &skin,QString &error){
        AppSkin draft{fields[0]->text(),fields[1]->text(),fields[2]->text(),fields[3]->text(),fields[4]->text(),fields[5]->text()};draft.version=imageData.isEmpty() && layerData.isEmpty()?3:4;draft.images=imageData;draft.layers=layerData;draft.uiScale=scale->value();draft.cornerRadius=radius->value();
        for(auto role=componentFields.begin();role!=componentFields.end();++role){QJsonObject rule;for(auto field=role.value().begin();field!=role.value().end();++field){const auto value=field.value()->text().trimmed();if(value.isEmpty())continue;if(QStringList{"background","text","border"}.contains(field.key()))rule[field.key()]=value;else {bool ok=false;const double n=value.toDouble(&ok);if(!ok){error="Enter a number for "+role.key()+" "+field.key()+".";return false;}rule[field.key()]=n;}}if(!rule.isEmpty())draft.components[role.key()]=rule;}
        return AppSkin::parse(draft.json(),skin,error);
    };
    auto update=[&]{if(filling)return;AppSkin skin;QString error;const bool ok=read(skin,error);errorLabel->setText(error);buttons->button(QDialogButtonBox::Save)->setEnabled(ok);exportButton->setEnabled(ok);if(!ok)return;
        preview->setPalette(skin.palette());header->skin=skin;card->skin=skin;for(auto *panel:{header,card}){const int padding=skin.component(panel->role).value("padding").toInt(8);panel->layout()->setContentsMargins(padding,padding,padding,padding+(panel->role=="header"?skinBannerHeight(skin,.5):0));}previewStyle->skin=skin;previewStyle->scale=skin.uiScale;previewStyle->radius=skin.cornerRadius;
        for(auto pair:QList<QPair<QWidget *,QString>>{{header,"header"},{card,"cards"},{input,"inputs"},{button,"buttons"}}){auto *w=pair.first;const auto r=skin.component(pair.second);auto p=skin.palette();const auto bg=r.value("background").toString(pair.second=="buttons"?skin.accent:pair.second=="header"?skin.background:skin.surface),fg=r.value("text").toString(pair.second=="buttons"?skin.onAccent:skin.text);for(auto role:{QPalette::Window,QPalette::Base,QPalette::Button})p.setColor(role,QColor(bg));for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText})p.setColor(role,QColor(fg));w->setPalette(p);w->setAutoFillBackground(w!=header && w!=card);auto f=dialog.font();f.setPointSizeF(f.pointSizeF()*skin.uiScale*r.value("textScale").toDouble(1));w->setFont(f);w->update();}
    };
    refreshImages=addSkinImageEditor(&dialog,imageForm,imageData,layerData,update);
    auto fill=[&](const AppSkin &skin){filling=true;imageData=skin.images;layerData=skin.layers;if(refreshImages)refreshImages();const QStringList values={skin.name,skin.background,skin.surface,skin.text,skin.accent,skin.onAccent};for(int i=0;i<fields.size();++i)fields[i]->setText(values[i]);scale->setValue(skin.uiScale);radius->setValue(skin.cornerRadius);for(auto role=componentFields.begin();role!=componentFields.end();++role){const auto rule=skin.component(role.key());for(auto f=role.value().begin();f!=role.value().end();++f)f.value()->setText(rule.contains(f.key())?(rule.value(f.key()).isString()?rule.value(f.key()).toString():QString::number(rule.value(f.key()).toDouble())):QString());}filling=false;update();};
    for(auto *field:fields)QObject::connect(field,&QLineEdit::textChanged,&dialog,update);
    for(const auto &role:componentFields)for(auto *field:role)QObject::connect(field,&QLineEdit::textChanged,&dialog,update);
    QObject::connect(scale,&QDoubleSpinBox::valueChanged,&dialog,update);QObject::connect(radius,&QSpinBox::valueChanged,&dialog,update);
    AppSkin manual;customAppSkin(manual);fill(resolvedAppSkin(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark));tabs->setEnabled(!follow->isChecked());presets->setEnabled(!follow->isChecked());
    QObject::connect(follow,&QCheckBox::toggled,&dialog,[&](bool checked){if(checked){QString error;read(manual,error);fill(AppSkin::preset(QGuiApplication::styleHints()->colorScheme()==Qt::ColorScheme::Dark?1:0));}else fill(manual);tabs->setEnabled(!checked);presets->setEnabled(!checked);});
    QObject::connect(presets,&QComboBox::activated,&dialog,[&](int i){if(i>=1 && i<=3)fill(AppSkin::preset(i-1));else if(i>=4){QFile file(":/skins/"+QStringList{"neos/tumblr-blue","neos/charcoal-rose","illustrated/nebula"}.value(i-4)+".json");AppSkin skin;QString error;if(file.open(QIODevice::ReadOnly) && AppSkin::parse(file.readAll(),skin,error))fill(skin);else errorLabel->setText(error.isEmpty()?"Cannot load bundled skin.":error);}});
    QObject::connect(reset,&QPushButton::clicked,&dialog,[&]{follow->setChecked(false);fill(AppSkin::preset(0));});
    QObject::connect(importButton,&QPushButton::clicked,&dialog,[&]{const auto path=QFileDialog::getOpenFileName(&dialog,"Import app skin",{},"Skin files (*.json)");if(path.isEmpty())return;QFile file(path);AppSkin skin;QString error;if(!file.open(QIODevice::ReadOnly)||!AppSkin::parse(file.read(skinMaxFileBytes+1),skin,error)){QMessageBox::warning(&dialog,"Cannot import skin",error.isEmpty()?"Cannot read the file.":error);return;}follow->setChecked(false);presets->setCurrentIndex(0);fill(skin);});
    QObject::connect(exportButton,&QPushButton::clicked,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;const auto path=QFileDialog::getSaveFileName(&dialog,"Export app skin","ao3-skin.json","Skin files (*.json)");if(path.isEmpty())return;QSaveFile file(path);const auto bytes=skin.json();if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())QMessageBox::warning(&dialog,"Export failed","Could not save the skin file.");});
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{AppSkin skin;QString error;if(!read(skin,error))return;QSettings settings;settings.setValue("appearance/followSystem",follow->isChecked());if(!follow->isChecked())settings.setValue("appearance/skin",skin.json());applyAppSkin();dialog.accept();});
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);dialog.exec();
}
