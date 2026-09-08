#include <QtTest>
#include "../updates/ReleasePolicy.h"
using namespace CloudStream::Updates;
#include "../updates/ReleaseUpdater.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
QByteArray fixture(Package p, QString version="0.1.0-preview.10", QByteArray payload="fixture package") {
 const auto name=assetName(version,p);
 return QJsonDocument(QJsonObject{{"tag_name","v"+version},{"draft",false},{"prerelease",false},{"body","Test-only release notes"},
 {"assets",QJsonArray{QJsonObject{{"name",name},{"size",payload.size()},{"digest","sha256:"+QString(QCryptographicHash::hash(payload,QCryptographicHash::Sha256).toHex())},
 {"browser_download_url","https://github.com/dcenhance/cloudstream-pc/releases/download/v"+version+"/"+name}}}}}).toJson();
}
#include "MockUpdateNetwork.h"
#include "../updates/UpdateInstaller.h"
#include "../updates/BuildInfo.h"
#include "../updates/UpdatePane.h"
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
class UpdaterTest : public QObject {
 Q_OBJECT
private slots:
 void asynchronousCheckAndVerifiedDownload() {
  MockNetwork net; net.responses={{fixture(Package::AppImage)},{"fixture package"}};
  ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net);
  u.check(); QVERIFY(u.busy()); QTRY_VERIFY(!u.busy()); QVERIFY(u.available());
  QCOMPARE(net.requests.first().url(),QUrl("https://api.github.com/repos/dcenhance/cloudstream-pc/releases/latest"));
  QVERIFY(net.requests.first().rawHeader("User-Agent").startsWith("CloudStream-PC/"));
  u.download(); QTRY_VERIFY(!u.busy()); QVERIFY2(!u.downloadedPath().isEmpty(),qPrintable(u.status()));
  QFile f(u.downloadedPath()); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(),QByteArray("fixture package"));
 }
 void failuresAndCancellation() {
  for(auto version:{"0.1.0-preview.4","0.1.0-preview.3"}) {
   MockNetwork net; net.responses={{fixture(Package::AppImage,version)}}; ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net);
   u.check(); QTRY_VERIFY(!u.busy()); QVERIFY(!u.available()); u.download(); QCOMPARE(net.requests.size(),1);
  }
  for(auto response:QList<MockResponse>{{"bad bad package"},{"",500},{"",302,QUrl("https://evil.test/payload")},{"",302,QUrl("http://github.com/x")}}) {
   MockNetwork net; net.responses={{fixture(Package::AppImage)},response}; ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net);
   u.check(); QTRY_VERIFY(!u.busy()); u.download(); QTRY_VERIFY(!u.busy()); QVERIFY(u.downloadedPath().isEmpty()); QCOMPARE(net.requests.size(),2);
  }
  MockNetwork net; net.responses={{fixture(Package::AppImage)},{"",200,{},true},{fixture(Package::AppImage)}};
  ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net); u.check(); QTRY_VERIFY(!u.busy()); u.download(); QVERIFY(u.busy()); u.cancel(); QVERIFY(!u.busy()); QVERIFY(u.downloadedPath().isEmpty());
  u.check(); QTRY_VERIFY(!u.busy()); QVERIFY(u.available());
  MockNetwork rate; rate.responses={{"",403}}; ReleaseUpdater r("0.1.0",Package::Deb,nullptr,&rate); r.check(); QTRY_VERIFY(!r.busy()); QVERIFY(r.status().contains("rate limit"));
 }
 void atomicAppImageReplacement() {
  QTemporaryDir d; const auto target=d.filePath("CloudStream.AppImage"), source=d.filePath("new.AppImage");
  QByteArray old="old fixture", payload=QByteArray::fromHex("7f454c460201010041490200")+"new fixture";
  {QFile f(target); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(old); f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);}
  {QFile f(source); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(payload);}
  auto hash=QCryptographicHash::hash(payload,QCryptographicHash::Sha256);
  QVERIFY(!replaceAppImage(source,target,QByteArray(32,'x'),payload.size()).isEmpty());
  {QFile f(target); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(),old);}
  const auto result=replaceAppImage(source,target,hash,payload.size()); QVERIFY2(result.isEmpty(),qPrintable(result));
  {QFile f(target); f.open(QIODevice::ReadOnly); QCOMPARE(f.readAll(),payload); QVERIFY(f.permissions()&QFile::ExeOwner);}
  {QFile f(target+".backup"); QVERIFY(f.open(QIODevice::ReadOnly)); QCOMPARE(f.readAll(),old);}
  QVERIFY(!replaceAppImage(source,target,hash,payload.size()).isEmpty()); // Preserve prior rollback, never clobber it.
  const auto link=d.filePath("link.AppImage"); QVERIFY(QFile::link(target,link)); QVERIFY(!replaceAppImage(source,link,hash,payload.size()).isEmpty());
 }
 void buildIdentity() {
#ifdef Q_OS_LINUX
  QCOMPARE(readPackageIdentity(buildVersion(),"deb","x86_64"),Package::Deb);
  QCOMPARE(readPackageIdentity(buildVersion(),"windows-setup","x86_64"),Package::Source);
#endif
  QCOMPARE(readPackageIdentity("999.0.0","deb","x86_64"),Package::Source);
  QCOMPARE(readPackageIdentity(buildVersion(),"deb","arm64"),Package::Source);
  QCOMPARE(readPackageIdentity(buildVersion(),"evil","x86_64"),Package::Source);
 }
 void destroyedUpdaterAbortsAndDisposesReply() {
  MockNetwork net; net.responses={{"",200,{},true}};
  auto *u=new ReleaseUpdater("0.1.0",Package::AppImage,nullptr,&net); u->check();
  QPointer<QNetworkReply> reply=net.findChild<QNetworkReply *>(); QVERIFY(reply); delete u;
  QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
  QVERIFY(reply.isNull());
 }
 void explicitConsentAndNoAutomaticInstall() {
  MockNetwork net; net.responses={{fixture(Package::AppImage)},{"fixture package"}};
  ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net);
  QTemporaryDir dir; QSettings settings(dir.filePath("test.ini"),QSettings::IniFormat);
  UpdatePane pane(&u,&settings); pane.show(); u.check(); QTRY_VERIFY(!u.busy());
  auto *download=pane.findChild<QPushButton *>("downloadAppUpdate"); QVERIFY(download); QVERIFY(download->isEnabled());
  auto respond=[](QMessageBox::StandardButton answer){QTimer::singleShot(0,[answer]{ auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); if(box && box->button(answer)) box->button(answer)->click(); });};
  respond(QMessageBox::No); download->click(); QCOMPARE(net.requests.size(),1); QVERIFY(u.downloadedPath().isEmpty());
  respond(QMessageBox::Yes); download->click(); QTRY_VERIFY(!u.busy()); QCOMPARE(net.requests.size(),2); QVERIFY(!u.downloadedPath().isEmpty());
  auto *reveal=pane.findChild<QPushButton *>("showAppUpdatePackage"); QVERIFY(reveal); QVERIFY(reveal->isEnabled());
  auto *apply=pane.findChild<QPushButton *>("applyAppUpdate"); QVERIFY(apply); QVERIFY(apply->isEnabled());
  respond(QMessageBox::No); apply->click(); QVERIFY(QFileInfo::exists(u.downloadedPath())); QCOMPARE(net.requests.size(),2);
 }
 void redirectsAndBounds() {
  const auto cdn=QUrl("https://release-assets.githubusercontent.com/github-production-release-asset/123/file?token=test");
  MockNetwork net; net.responses={{fixture(Package::AppImage)},{"",302,cdn},{"fixture package"}};
  ReleaseUpdater u("0.1.0-preview.4",Package::AppImage,nullptr,&net); u.check(); QTRY_VERIFY(!u.busy()); u.download(); QTRY_VERIFY(!u.busy()); QVERIFY(!u.downloadedPath().isEmpty()); QCOMPARE(net.requests.size(),3);
  MockNetwork loop; loop.responses={{fixture(Package::AppImage)}}; for(int i=0;i<8;++i) loop.responses.append({"",302,cdn});
  ReleaseUpdater l("0.1.0-preview.4",Package::AppImage,nullptr,&loop); l.check(); QTRY_VERIFY(!l.busy()); l.download(); QTRY_VERIFY(!l.busy()); QVERIFY(l.downloadedPath().isEmpty()); QVERIFY(l.status().contains("redirect"));
  MockNetwork large; large.responses={{QByteArray(2*1024*1024+1,'x')}};
  ReleaseUpdater b("0.1.0",Package::Deb,nullptr,&large); b.check(); QTRY_VERIFY(!b.busy()); QVERIFY(!b.available()); QVERIFY(b.status().contains("limit"));
 }
 void releaseMetadata() {
  QString error;
  for(auto p:{Package::WindowsSetup,Package::WindowsZip,Package::AppImage,Package::Deb,Package::Rpm}) {
   auto r=parseRelease(fixture(p),p,&error); QVERIFY2(r.has_value(),qPrintable(error)); QCOMPARE(r->name,assetName("0.1.0-preview.10",p));
  }
  QVERIFY(!parseRelease(fixture(Package::AppImage),Package::WindowsSetup,&error));
  auto obj=QJsonDocument::fromJson(fixture(Package::WindowsSetup)).object();
  obj["tag_name"]="../../bad"; QVERIFY(!parseRelease(QJsonDocument(obj).toJson(),Package::WindowsSetup,&error));
  obj=QJsonDocument::fromJson(fixture(Package::WindowsSetup)).object(); auto assets=obj["assets"].toArray(); auto asset=assets[0].toObject();
  asset.remove("digest"); assets[0]=asset; obj["assets"]=assets; QVERIFY(!parseRelease(QJsonDocument(obj).toJson(),Package::WindowsSetup,&error));
  QVERIFY(!parseRelease("{broken",Package::WindowsSetup,&error));
 }
 void assetsAndHosts() {
  const QString v="0.1.0-preview.4";
  QCOMPARE(assetName(v,Package::WindowsSetup), "CloudStream-PC-0.1.0-preview.4-Windows-x64-Setup.exe");
  QCOMPARE(assetName(v,Package::WindowsZip), "CloudStream-PC-0.1.0-preview.4-Windows-x64.zip");
  QCOMPARE(assetName(v,Package::AppImage), "CloudStream-PC-0.1.0-preview.4-x86_64-system-runtime.AppImage");
  QCOMPARE(assetName(v,Package::Deb), "cloudstream-pc_0.1.0.preview.4_amd64.deb");
  QCOMPARE(assetName(v,Package::Rpm), "cloudstream-pc-0.1.0-0.preview.4.x86_64.rpm");
  QVERIFY(assetName(v,Package::Source).isEmpty());
  QVERIFY(trustedUrl(QUrl("https://github.com/dcenhance/cloudstream-pc/releases/download/v0.1.0-preview.4/file")));
  QVERIFY(trustedUrl(QUrl("https://release-assets.githubusercontent.com/github-production-release-asset/123/file?token=x"),true));
  for(auto bad:{"http://github.com/dcenhance/cloudstream-pc/releases/download/a/b", "https://github.com.evil.test/x", "https://github.com/other/repo/releases/download/v1/file", "https://user@github.com/dcenhance/cloudstream-pc/releases/download/a/b", "https://127.0.0.1/x", "https://release-assets.githubusercontent.com:444/x"}) QVERIFY(!trustedUrl(QUrl(bad),true));
 }
 void versions() {
  QCOMPARE(compareVersions("v0.1.0-preview.10", "0.1.0-preview.4"), std::optional<int>(1));
  QCOMPARE(compareVersions("0.1.0", "0.1.0-preview.10"), std::optional<int>(1));
  QCOMPARE(compareVersions("0.1.0-preview.4", "v0.1.0-preview.4"), std::optional<int>(0));
  QCOMPARE(compareVersions("0.1.0-preview.3", "0.1.0-preview.4"), std::optional<int>(-1));
  for (auto bad : {"Preview4", "1.0", "v01.0.0", "1.0.0-preview.04", "1.0.0/evil", "1.0.0\n", "999999999999999999999.0.0", "1.0.0-"})
   QVERIFY2(!compareVersions(bad, "0.1.0"), bad);
 }
};
QTEST_MAIN(UpdaterTest)
#include "test_updater.moc"
