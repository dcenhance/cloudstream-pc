#include "ReleaseUpdater.h"
#include "../network/CloudStreamRequest.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QDir>
#include <cmath>
namespace CloudStream::Updates {
ReleaseUpdater::ReleaseUpdater(QString current, Package package, QObject *parent, QNetworkAccessManager *network)
 : QObject(parent), current_(current), package_(package), network_(network?network:new QNetworkAccessManager(this)) {
 deadline_.setSingleShot(true);
 connect(&deadline_,&QTimer::timeout,this,[this]{finish("Update request timed out. Please try again.");});
}
ReleaseUpdater::~ReleaseUpdater() {
 if(reply_) { disconnect(reply_,nullptr,this,nullptr); reply_->abort(); reply_->deleteLater(); }
}
void ReleaseUpdater::check() {
 if(busy_) return;
 available_=false; release_={}; downloaded_.clear(); file_.reset(); directory_.reset(); metadata_.clear();
 downloading_=false; busy_=true; status_="Checking GitHub Releases…"; deadline_.start(60000); emit changed();
 request(QUrl("https://api.github.com/repos/dcenhance/cloudstream-pc/releases/latest"));
}
void ReleaseUpdater::download() {
 if(busy_ || !available_) return;
 downloaded_.clear(); directory_=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/cloudstream-update-XXXXXX");
 if(!directory_->isValid()) { finish("Cannot create a private download directory."); return; }
 file_=std::make_unique<QSaveFile>(directory_->filePath(release_.name));
 file_->setDirectWriteFallback(false);
 if(!file_->open(QIODevice::WriteOnly)) { finish("Cannot write update package."); return; }
 file_->setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
 hash_.reset(); received_=0; downloading_=true; busy_=true; status_="Downloading and verifying SHA-256…";
 deadline_.start(30*60*1000); emit changed(); request(release_.url);
}
void ReleaseUpdater::cancel() { if(busy_) finish("Update cancelled. No installation was changed."); }
void ReleaseUpdater::finish(QString status) {
 deadline_.stop();
 if(reply_) { auto *r=reply_.data(); reply_=nullptr; disconnect(r,nullptr,this,nullptr); if(!r->isFinished()) r->abort(); r->deleteLater(); }
 if(file_) { file_->cancelWriting(); file_.reset(); }
 busy_=false; status_=status; emit changed();
}
void ReleaseUpdater::request(const QUrl &url,int redirects) {
 if(!trustedUrl(url,redirects>0) || redirects>5) { finish("Blocked an untrusted update redirect."); return; }
 auto req=CloudStreamRequest::metadata(url);
 req.setRawHeader("User-Agent",("CloudStream-PC/"+current_+" (+https://github.com/dcenhance/cloudstream-pc)").toUtf8());
 req.setRawHeader("Accept",downloading_?"application/octet-stream":"application/vnd.github+json");
 req.setRawHeader("X-GitHub-Api-Version","2022-11-28");
 req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
 auto *r=network_->get(req); reply_=r;
 r->setReadBufferSize(128*1024);
 connect(r,&QNetworkReply::readyRead,this,[this,r]{
  if(reply_!=r) return;
  auto code=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  // Never consume a redirect/error body as a package.
  if(code!=200) { r->read(65536); return; }
  while(r->bytesAvailable()>0) {
   auto bytes=r->read(65536); if(bytes.isEmpty()) break;
   if(downloading_) {
    received_+=bytes.size();
    if(received_>release_.size) {finish("Package exceeds its declared size.");return;}
    if(!file_ || file_->write(bytes)!=bytes.size()) {finish("Cannot write package (disk full or permission denied).");return;}
    hash_.addData(bytes); emit progress(received_,release_.size);
   } else {
    metadata_.append(bytes);
    if(metadata_.size()>2*1024*1024) {finish("Release metadata exceeds safety limit.");return;}
   }
  }
 });
 connect(r,&QNetworkReply::finished,this,[this,r,url,redirects]{
  if(reply_!=r) return;
  const int code=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if(code>=300 && code<=399) {
   const auto target=r->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
   reply_=nullptr; r->deleteLater();
   if(target.isEmpty() || (!downloading_ && url.resolved(target).host()!="api.github.com")) {finish("Blocked an invalid update redirect.");return;}
   request(url.resolved(target),redirects+1); return;
  }
  if(code==403 || code==429) { finish("GitHub rate limit or access restriction. Try again later (no token required).");return; }
  if(code!=200 || r->error()!=QNetworkReply::NoError) { finish(QString("Update request failed (HTTP %1): %2").arg(code).arg(r->errorString()));return; }
  if(downloading_) {
   if(received_!=release_.size || hash_.result()!=release_.sha256) {finish("SHA-256 or size mismatch. Package discarded; installation untouched.");return;}
   if(!file_->commit()) {finish("Could not finalize the verified package.");return;}
   file_.reset(); downloaded_=directory_->filePath(release_.name);
   finish("Package verified with GitHub SHA-256. Ready for your confirmation.");
  } else {
   QString error; auto parsed=parseRelease(metadata_,package_,&error);
   if(!parsed) {finish(error);return;} release_=*parsed;
   auto comparison=compareVersions(release_.version,current_);
   if(!comparison) {finish("Current build version is unknown; automatic update is disabled.");return;}
   available_=*comparison>0;
   finish(available_?"A newer compatible release is available.":(*comparison==0?"You are up to date.":"This build is newer than the latest release. No downgrade offered."));
  }
 });
}

namespace { constexpr qint64 metadataLimit=2*1024*1024; constexpr qint64 packageLimit=1024LL*1024*1024; }
std::optional<Release> parseRelease(const QByteArray &json, Package package, QString *error) {
 auto fail=[error](QString why)->std::optional<Release> { if(error) *error=why; return {}; };
 if(json.size()>metadataLimit) return fail("Release metadata exceeds safety limit.");
 QJsonParseError e; auto doc=QJsonDocument::fromJson(json,&e);
 if(e.error!=QJsonParseError::NoError || !doc.isObject()) return fail("Invalid GitHub release metadata.");
 auto o=doc.object(); auto tag=o["tag_name"].toString(); auto v=parseVersion(tag);
 if(!v || !o["draft"].isBool() || o["draft"].toBool() || !o["assets"].isArray()) return fail("Invalid release tag or draft release.");
 // Build metadata has no precedence and is not part of the package naming contract.
 if(v->text.contains('+')) return fail("Unsupported release tag metadata.");
 const auto expected=assetName(v->text,package);
 if(expected.isEmpty()) return fail("Choose a supported package format to download.");
 auto assets=o["assets"].toArray(); if(assets.size()>100) return fail("Too many release assets.");
 Release r; r.tag=tag; r.version=v->text; r.notes=o["body"].toString().left(65536);
 int matches=0;
 for(auto item:assets) {
  auto a=item.toObject(); if(a["name"].toString()!=expected) continue;
  ++matches; r.name=expected; r.url=QUrl(a["browser_download_url"].toString(),QUrl::StrictMode);
  const QString path="/dcenhance/cloudstream-pc/releases/download/"+tag+"/"+expected;
  if(!trustedUrl(r.url) || r.url.host()!="github.com" || r.url.path()!=path) return fail("Invalid release asset URL.");
  double size=a["size"].toDouble(-1); if(size<=0 || size>packageLimit || std::floor(size)!=size) return fail("Invalid or oversized release package.");
  r.size=qint64(size);
  auto digest=a["digest"].toString();
  static const QRegularExpression sha("\\Asha256:([0-9a-fA-F]{64})\\z"); auto match=sha.match(digest);
  if(!match.hasMatch()) return fail("This package has no GitHub SHA-256 digest. Download is blocked; ask the publisher to upload an asset with a digest.");
  r.sha256=QByteArray::fromHex(match.captured(1).toLatin1());
 }
 if(matches!=1) return fail(matches?"Ambiguous release packages.":"Latest release has no compatible package for this installation. No update offered.");
 return r;
}
}
