#include <QtTest>

#include "../network/CloudStreamRequest.h"

#include <QNetworkRequest>

class CloudStreamRequestTest final : public QObject {
    Q_OBJECT
private slots:
    void metadataRequestsAvoidHttp2FanoutFailures() {
        const auto request = CloudStream::CloudStreamRequest::metadata(
            QUrl("https://raw.githubusercontent.com/example/repo.json"));
        QCOMPARE(request.rawHeader("User-Agent"),
                 QByteArray("CloudStream-Linux/0.1 (+https://github.com/recloudstream/cloudstream)"));
        QCOMPARE(request.attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(),
                 static_cast<int>(QNetworkRequest::NoLessSafeRedirectPolicy));
        QCOMPARE(request.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
        QVERIFY(request.transferTimeoutAsDuration().count() > 0);
    }

    void artworkRequestsUseHttp2AndThePersistentCache() {
        const auto request = CloudStream::CloudStreamRequest::artwork(
            QUrl("https://image.cdn.example/poster.jpg"));
        QCOMPARE(request.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), true);
        QCOMPARE(request.attribute(QNetworkRequest::CacheLoadControlAttribute).toInt(),
                 static_cast<int>(QNetworkRequest::PreferCache));
    }
};

QTEST_APPLESS_MAIN(CloudStreamRequestTest)
#include "test_network_request_policy.moc"
