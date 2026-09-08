#pragma once
// Test-only asynchronous transport: production URL policy is still exercised.
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
struct MockResponse { QByteArray bytes; int status=200; QUrl redirect; bool hang=false; };
class MockReply : public QNetworkReply {
 QByteArray bytes_; qint64 offset_=0;
public:
 MockReply(const QNetworkRequest &req, MockResponse response, QObject *parent):QNetworkReply(parent),bytes_(response.bytes) {
  setRequest(req); setUrl(req.url()); setOperation(QNetworkAccessManager::GetOperation);
  setAttribute(QNetworkRequest::HttpStatusCodeAttribute,response.status);
  if(!response.redirect.isEmpty()) setAttribute(QNetworkRequest::RedirectionTargetAttribute,response.redirect);
  open(QIODevice::ReadOnly);
  if(!response.hang) QTimer::singleShot(0,this,[this]{ emit readyRead(); if(isFinished()) return; setFinished(true); emit finished(); });
 }
 void abort() override { if(isFinished()) return; setError(OperationCanceledError,"cancelled"); setFinished(true); emit finished(); }
 qint64 bytesAvailable() const override { return bytes_.size()-offset_+QNetworkReply::bytesAvailable(); }
protected:
 qint64 readData(char *data,qint64 max) override { auto n=qMin(max,bytes_.size()-offset_); if(n<=0) return -1; memcpy(data,bytes_.constData()+offset_,n); offset_+=n; return n; }
};
class MockNetwork : public QNetworkAccessManager {
public:
 QList<MockResponse> responses; QList<QNetworkRequest> requests;
protected:
 QNetworkReply *createRequest(Operation,const QNetworkRequest &r,QIODevice *) override {
  requests.append(r); return new MockReply(r,responses.isEmpty()?MockResponse{{},500}:responses.takeFirst(),this);
 }
};
