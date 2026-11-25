#pragma once
#include "flowlayout.h"
#include "commentpage.h"
#include <QWidget>
#include <QTextEdit>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QUrl>
#include <QKeySequence>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextList>
#include <QStackedWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <functional>

namespace Authoring {
inline bool field(const QString &name){return name.endsWith("[content]") || name.endsWith("[summary]") || name.endsWith("[notes]") || name.endsWith("[endnotes]");}
inline bool supported(const QString &html){
    const QStringList tags={"p","br","div","span","b","strong","i","em","u","s","strike","del","h1","h2","h3","h4","h5","h6","ul","ol","li","a"};
    const auto nodes=CommentHtml::nodes(html);for(const auto &n:nodes){if(n.tag=="ul" || n.tag=="ol")for(int p=n.parent;p>=0;p=nodes[p].parent)if(nodes[p].tag=="ul" || nodes[p].tag=="ol")return false;if(!tags.contains(n.tag))return false;for(auto i=n.attrs.cbegin();i!=n.attrs.cend();++i)if(!(n.tag=="a" && i.key()=="href"))return false;}return true;
}
inline QString html(const QTextDocument *document){
    QString out;QTextList *list=nullptr;QString listTag;
    for(auto b=document->begin();b.isValid();b=b.next()){
        auto *next=b.textList();if(next!=list){if(list)out+="</"+listTag+">";list=next;if(list){listTag=list->format().style()==QTextListFormat::ListDecimal?"ol":"ul";out+="<"+listTag+">";}}
        const int heading=b.blockFormat().headingLevel();const QString tag=list?"li":heading?"h"+QString::number(heading):"p";out+="<"+tag+">";
        for(auto it=b.begin();!it.atEnd();++it){const auto f=it.fragment();if(!f.isValid())continue;auto text=f.text().toHtmlEscaped().replace(QChar::LineSeparator,"<br>");const auto c=f.charFormat();
            if(c.fontStrikeOut())text="<s>"+text+"</s>";if(c.fontUnderline())text="<u>"+text+"</u>";if(c.fontItalic())text="<em>"+text+"</em>";if(c.fontWeight()>=QFont::Bold)text="<strong>"+text+"</strong>";
            if(c.isAnchor() && !c.anchorHref().isEmpty())text="<a href=\""+c.anchorHref().toHtmlEscaped()+"\">"+text+"</a>";out+=text;
        }
        if(b.text().isEmpty())out+="<br>";out+="</"+tag+">";
    }
    if(list)out+="</"+listTag+">";return out;
}
}
class AuthoringEditor final:public QWidget {
public:
    QTextEdit *visual,*source;QComboBox *mode;std::function<void(const QString &)> changed;
    AuthoringEditor(const QString &value,const QString &name,QWidget *parent=nullptr):QWidget(parent){
        auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);mode=new QComboBox(this);mode->addItems({"Visual","HTML"});mode->setAccessibleName("Editing mode");layout->addWidget(mode);
        auto *toolbar=new QWidget(this);auto *buttons=new FlowLayout(toolbar);buttons->setContentsMargins(0,0,0,0);buttons->setSpacing(4);layout->addWidget(toolbar);auto *stack=new QStackedWidget(this);visual=new QTextEdit(stack);source=new QTextEdit(stack);source->setAcceptRichText(false);source->setPlainText(value);source->setObjectName("tool_"+name);source->setAccessibleName("HTML source");visual->setObjectName("visual_"+name);visual->setAccessibleName("Visual editor");visual->setAcceptRichText(false);visual->setHtml(Authoring::supported(value)?value:QString());stack->addWidget(visual);stack->addWidget(source);stack->setMinimumHeight(180);stack->setMaximumHeight(360);layout->addWidget(stack);
        auto add=[&](const QString &label,auto action){auto *b=new QPushButton(label,toolbar);buttons->addWidget(b);b->setFocusPolicy(Qt::StrongFocus);if(label=="Bold")b->setShortcut(QKeySequence::Bold);if(label=="Italic")b->setShortcut(QKeySequence::Italic);if(label=="Underline")b->setShortcut(QKeySequence::Underline);if(label=="Undo")b->setShortcut(QKeySequence::Undo);if(label=="Redo")b->setShortcut(QKeySequence::Redo);connect(b,&QPushButton::clicked,this,action);};
        auto format=[this](int kind){auto c=visual->textCursor();if(!c.hasSelection())return;QTextCharFormat f;if(kind==0)f.setFontWeight(c.charFormat().fontWeight()>=QFont::Bold?QFont::Normal:QFont::Bold);if(kind==1)f.setFontItalic(!c.charFormat().fontItalic());if(kind==2)f.setFontUnderline(!c.charFormat().fontUnderline());if(kind==3)f.setFontStrikeOut(!c.charFormat().fontStrikeOut());c.mergeCharFormat(f);visual->setTextCursor(c);};
        add("Bold",[=]{format(0);});add("Italic",[=]{format(1);});add("Underline",[=]{format(2);});add("Strike",[=]{format(3);});
        add("Heading",[this]{auto c=visual->textCursor();auto f=c.blockFormat();f.setHeadingLevel(f.headingLevel()?0:2);c.setBlockFormat(f);auto selected=c;if(!selected.hasSelection())selected.select(QTextCursor::BlockUnderCursor);QTextCharFormat size;size.setFontPointSize(visual->font().pointSizeF()*(f.headingLevel()?1.4:1.0));selected.mergeCharFormat(size);});
        auto list=[this](bool ordered){auto c=visual->textCursor();const auto style=ordered?QTextListFormat::ListDecimal:QTextListFormat::ListDisc;if(c.currentList() && c.currentList()->format().style()==style){const int end=c.selectionEnd();auto b=c.document()->findBlock(c.selectionStart());while(b.isValid() && b.position()<=end){auto next=b.next();if(auto *l=b.textList())l->remove(b);QTextCursor at(b);auto f=at.blockFormat();f.setIndent(0);at.setBlockFormat(f);if(!c.hasSelection() || next.position()>=end)break;b=next;}}else{QTextListFormat f;f.setStyle(style);c.createList(f);}};
        add("Bullets",[=]{list(false);});add("Numbered",[=]{list(true);});
        add("Link",[this]{auto c=visual->textCursor();if(!c.hasSelection())return;bool ok=false;const auto url=QInputDialog::getText(this,"Link","HTTPS URL",QLineEdit::Normal,c.charFormat().anchorHref(),&ok);if(!ok)return;QUrl u(url);if(!url.isEmpty() && (u.scheme()!="https" || u.host().isEmpty() || !u.userInfo().isEmpty())){QMessageBox::information(this,"Link","Enter a complete HTTPS link.");return;}QTextCharFormat f;f.setAnchor(!url.isEmpty());f.setAnchorHref(url);c.mergeCharFormat(f);});
        add("Undo",[this]{visual->undo();});add("Redo",[this]{visual->redo();});
        const bool basic=Authoring::supported(value);mode->setCurrentIndex(basic?0:1);stack->setCurrentIndex(mode->currentIndex());toolbar->setVisible(basic);
        connect(mode,&QComboBox::currentIndexChanged,this,[=](int index){if(index==0){if(!Authoring::supported(source->toPlainText())){QMessageBox::information(this,"HTML formatting","This content uses formatting the visual editor cannot preserve. Keep editing it as HTML.");mode->setCurrentIndex(1);return;}QSignalBlocker guard(visual);visual->setHtml(source->toPlainText());}else{QSignalBlocker guard(source);source->setPlainText(current);}stack->setCurrentIndex(index);toolbar->setVisible(index==0);});
        current=value;connect(source,&QTextEdit::textChanged,this,[this]{if(mode->currentIndex()!=1)return;current=source->toPlainText();if(changed)changed(current);});connect(visual,&QTextEdit::textChanged,this,[this]{if(mode->currentIndex()!=0)return;current=Authoring::html(visual->document());if(changed)changed(current);});
    }
private:QString current;
};
