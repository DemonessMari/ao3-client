#include "nativetools.h"
#include "formattedwork.h"
#include <QtTest>
#include <QFile>
class NativeToolsTest : public QObject {
 Q_OBJECT
 QString fixture(const QString &name){QFile f(QString(AO3_FIXTURE_DIR)+"/native-tools-"+name+".html");if(!f.open(QIODevice::ReadOnly))return {};return QString::fromUtf8(f.readAll());}
private slots:
 void publishingKeepsActionsAndTextSeparate(){
  const auto html=fixture("work");const auto page=NativeTools::parse(html,"/works/new","Writer");QVERIFY(page.error.isEmpty());QCOMPARE(page.forms.size(),1);const auto f=page.forms.first();QCOMPARE(f.submits.size(),3);QCOMPARE(f.fields.size(),5);QVERIFY(f.fields[1].values.first().contains("<form action='/works/666'"));QCOMPARE(page.links.size(),1);QVERIFY(page.links.first().second.endsWith("/drafts"));
  auto fresh=NativeTools::parse(QString(html).replace("work+token","fresh+token"),"/works/new","Writer");QString error;const auto fields=NativeTools::prepare(f,fresh,{{"work[title]",{"New title"}},{"work[archive_warning_ids][]",{"14","17"}}},f.submits[0],error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(fields.value("authenticity_token"),QStringList{"fresh+token"});QVERIFY(fields.contains("save_button"));QVERIFY(!fields.contains("post_button"));QCOMPARE(fields.value("work[archive_warning_ids][]").size(),2);
  fresh=NativeTools::parse(QString(html).replace("Original","Changed elsewhere"),"/works/new","Writer");NativeTools::prepare(f,fresh,{},f.submits[0],error);QVERIFY(!error.isEmpty());
 }
 void previewUsesFreshAuthorityWithoutDroppingUnsavedContent(){
  const auto edit=QString(fixture("work")).replace("action=\"/works\"","action=\"/works/42\"").replace("<fieldset>","<input name='_method' type='hidden' value='patch'><fieldset>");
  const auto original=NativeTools::parse(edit,"/works/42/edit","Writer").forms.first();auto preview=NativeTools::continued(NativeTools::parse(fixture("preview"),"/works/42","Writer"),original,"/works/42");QCOMPARE(preview.forms.size(),1);QVERIFY(preview.text.contains("Preview text"));QVERIFY(preview.previewHtml.contains("Preview text"));QVERIFY(!preview.previewHtml.contains("<form"));QCOMPARE(preview.forms.first().source,QString("/works/42/edit"));QString error;
  const auto fields=NativeTools::prepare(preview.forms.first(),NativeTools::parse(QString(edit).replace("work+token","fresh"),"/works/42/edit","Writer"),{},preview.forms.first().submits.last(),error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(fields.value("work[title]"),QStringList{"New title"});QCOMPARE(fields.value("authenticity_token"),QStringList{"fresh"});QVERIFY(fields.value("work[chapter_attributes][content]").first().contains("Changed story"));
 }
 void bulkTransitionsRecheckTheOriginalSelectionAndGlobalToken(){
  const auto html=fixture("bulk");const auto initial=NativeTools::parse(html,"/users/Writer/works/show_multiple","Writer").forms.first();auto next=html;next.replace("/works/edit_multiple","/works/update_multiple");next.replace("name=\"work_ids[]\" type=\"checkbox\"","name=\"work_ids[]\" type=\"hidden\"");next.replace("value=\"Edit\"","value=\"Update\"");
  const auto page=NativeTools::continued(NativeTools::parse(next,initial.action,"Writer"),initial,initial.action);QCOMPARE(page.forms.first().source,initial.source);QString error;const auto fields=NativeTools::prepare(page.forms.first(),NativeTools::parse(QString(html).replace("global+token","fresh+global"),initial.source,"Writer"),{},page.forms.first().submits.first(),error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(fields.value("authenticity_token"),QStringList{"fresh+global"});QCOMPARE(fields.value("work_ids[]"),QStringList({"42","43"}));
 }
 void accountFilesAndDestructiveActionsStayBounded(){
  const auto p=NativeTools::parse(fixture("account"),"/users/Writer/change_password","Writer");QVERIFY(p.error.isEmpty());QCOMPARE(p.forms.size(),3);QCOMPARE(p.forms[0].fields.first().type,QString("password"));QVERIFY(p.forms[0].fields.first().values.first().isEmpty());QVERIFY(!p.forms[0].checkpoint.contains("password_check"));QCOMPARE(p.forms[2].fixed.value("_method"),QStringList{"delete"});QVERIFY(p.forms[2].action.endsWith("/Other"));
  QString error;auto file=NativeTools::upload("pseud[icon]","bad\r\nname.png",QByteArray::fromHex("89504e470d0a1a0a"),error);QVERIFY(error.isEmpty());QVERIFY(!file.filename.contains('\r'));NativeTools::upload("pseud[icon]","file.exe","MZbad",error);QVERIFY(!error.isEmpty());
  QVERIFY(NativeTools::route("https://archiveofourown.org@evil.example/works/new","Writer").isEmpty());QVERIFY(NativeTools::route("/users/Other/change_password","Writer").isEmpty());QVERIFY(NativeTools::parse(fixture("account"),"/users/Writer/change_password","Other").forms.isEmpty());QVERIFY(!NativeTools::fieldAllowed("user[admin]"));
 }
 void feedsAndOptInMediaRemainPassive(){
  QFile file(QString(AO3_FIXTURE_DIR)+"/native-tools-feed.atom");QVERIFY(file.open(QIODevice::ReadOnly));const auto feed=NativeTools::parse(QString::fromUtf8(file.readAll()),"/tags/Friends/feed.atom","Writer");QVERIFY(feed.error.isEmpty());QVERIFY(feed.feed);QCOMPARE(feed.links.size(),1);QCOMPARE(feed.links.first().second,QString("/works/42"));QVERIFY(feed.text.contains("Summary & notes"));
  const auto html=staticReadingHtml("<p>img-src data:;</p><audio src='https://media.example/a.mp3' autoplay></audio><video><source src='https://media.example/v.mp4'></video><iframe src='https://player.example/embed'></iframe><script>bad()</script>");QVERIFY(html.contains("<audio"));QVERIFY(!html.contains("autoplay"));QVERIFY(!html.contains("<iframe"));QVERIFY(html.contains("Open embedded player"));QVERIFY(readingEmbedTargets(html).contains(QUrl("https://player.example/embed")));QVERIFY(readingEmbedTargets("<a data-ao3-embed='1' href='https://archiveofourown.org/users/Writer'>Not a player</a>").isEmpty());
  const auto offline=formattedDocument(html,{},"#ffffff","#000000");QVERIFY(!offline.contains("<audio"));QVERIFY(offline.contains("Audio/video requires external content."));
  const auto online=formattedDocument(html,".a{background:url(/images/a.png)}","#ffffff","#000000",18,1.6,24,true,{},"serif",true);QVERIFY(online.contains("media-src data: https:"));QVERIFY(online.contains("frame-src 'none'"));QVERIFY(!online.contains("<script"));QVERIFY(online.contains("<p>img-src data:;</p>"));QVERIFY(online.contains("https://archiveofourown.org/images/a.png"));
 }
 void nestedSectionDataDoesNotRunScripts(){
  const auto html=fixture("sections");const auto original=NativeTools::parse(html,"/skins/7/edit","Writer").forms.first();QCOMPARE(original.sections.size(),1);QString error;const auto added=NativeTools::addSection(original,original.sections.first().key,"Writer",error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(added.fields.size(),3);QCOMPARE(added.addedSections,1);QCOMPARE(added.checkpoint,original.checkpoint);QString child,remove;for(const auto &field:added.fields){if(field.name.endsWith("[parent_skin_title]"))child=field.name;if(field.name.endsWith("[_destroy]")){remove=field.name;QCOMPARE(field.type,QString("check"));}}
  const auto values=NativeTools::prepare(added,NativeTools::parse(QString(html).replace("skin+token","fresh+token"),"/skins/7/edit","Writer"),{{child,{"Parent"}},{remove,{"1"}}},added.submits.first(),error);QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(values.value(child),QStringList{"Parent"});QCOMPARE(values.value(remove),QStringList{"1"});
  auto hostile=html;hostile.replace("; return false;","; execute_unknown(); return false;");QVERIFY(NativeTools::parse(hostile,"/skins/7/edit","Writer").forms.first().sections.isEmpty());
 }
 void challengeNoScriptControlsAndGuestBrowsing(){
  const auto p=NativeTools::parse(fixture("challenge"),"/collections/Event/signups/new","Writer");QCOMPARE(p.forms.size(),2);QCOMPARE(p.forms.first().method,QString("get"));QVERIFY(p.forms.last().fields[1].name.contains("[requests_attributes][0]"));QCOMPARE(p.forms.last().fields.last().type,QString("choice"));QCOMPARE(NativeTools::parse(fixture("challenge"),"/collections/Event/signups/new",{}).forms.size(),1);
 }
};
QTEST_MAIN(NativeToolsTest)
#include "native_tools_test.moc"
