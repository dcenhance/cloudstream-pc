#pragma once
#include "ReleasePolicy.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
namespace CloudStream::Updates {
struct Release { QString tag, version, notes, name; QUrl url; QByteArray sha256; qint64 size=0; };
std::optional<Release> parseRelease(const QByteArray &json, Package package, QString *error);
class ReleaseUpdater final : public QObject {
 Q_OBJECT
public:
 explicit ReleaseUpdater(QString current, Package package, QObject *parent=nullptr, QNetworkAccessManager *network=nullptr);
 ~ReleaseUpdater() override;
 QString currentVersion() const { return current_; }
 QString status() const { return status_; }
 Release release() const { return release_; }
 QString downloadedPath() const { return downloaded_; }
 bool busy() const { return busy_; }
 bool available() const { return available_; }
 Package package() const { return package_; }
 void check();
 void download();
 void cancel();
 void retainDownload() { if(directory_ && !downloaded_.isEmpty()) directory_->setAutoRemove(false); }
signals:
 void changed();
 void progress(qint64 received, qint64 total);
private:
 void request(const QUrl &url, int redirects=0);
 void finish(QString status);
 QString current_, status_="Not checked", downloaded_;
 Package package_;
 QNetworkAccessManager *network_;
 QPointer<QNetworkReply> reply_;
 QTimer deadline_;
 bool busy_=false, downloading_=false, available_=false;
 QByteArray metadata_;
 qint64 received_=0;
 Release release_;
 std::unique_ptr<QTemporaryDir> directory_;
 std::unique_ptr<QSaveFile> file_;
 QCryptographicHash hash_{QCryptographicHash::Sha256};
};
}
