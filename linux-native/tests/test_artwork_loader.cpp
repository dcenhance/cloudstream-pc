#include "../media/ArtworkLoader.h"

#include <QtTest>

#include <QBuffer>
#include <QHostAddress>
#include <QImage>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

class ArtworkHttpFixture final : public QObject {
    Q_OBJECT
public:
    explicit ArtworkHttpFixture(QObject *parent = nullptr) : QObject(parent) {
        QImage source(1200, 1800, QImage::Format_RGB32);
        source.fill(QColor("#cc3344"));
        QBuffer buffer(&payload);
        buffer.open(QIODevice::WriteOnly);
        source.save(&buffer, "JPEG", 90);
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (auto *socket = server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    if (!socket->canReadLine()) return;
                    socket->readAll();
                    ++requestCount;
                    const auto contentLength = declaredLength >= 0
                        ? declaredLength : payload.size();
                    const auto response = QByteArray(
                        "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\n"
                        "Cache-Control: max-age=3600\r\nContent-Length: ") +
                        QByteArray::number(contentLength) +
                        "\r\nConnection: close\r\n\r\n" + payload;
                    socket->write(response);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    bool listen() { return server.listen(QHostAddress::LocalHost); }
    QUrl url() const {
        return QUrl(QString("http://127.0.0.1:%1/poster.jpg").arg(server.serverPort()));
    }

    QTcpServer server;
    QByteArray payload;
    int requestCount = 0;
    qint64 declaredLength = -1;
};

class ArtworkLoaderTest final : public QObject {
    Q_OBJECT

private:
    static CloudStream::ArtworkLoader *localLoader(const QString &cachePath,
                                                    QObject *parent = nullptr) {
        return new CloudStream::ArtworkLoader(
            cachePath, parent, CloudStream::ArtworkLoader::AllowPrivateNetwork);
    }

private slots:
    void coalescesConcurrentLoadsAndReusesMemoryResult() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        QVERIFY(fixture.listen());
        QScopedPointer<CloudStream::ArtworkLoader> loader(localLoader(cache.path()));
        QObject context;
        QList<QImage> delivered;

        loader->load(fixture.url(), QSize(150, 225), &context,
                     [&delivered](const QImage &image) { delivered.append(image); });
        loader->load(fixture.url(), QSize(150, 225), &context,
                     [&delivered](const QImage &image) { delivered.append(image); });

        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 2, 3000);
        QCOMPARE(fixture.requestCount, 1);
        QCOMPARE(delivered[0].size(), QSize(150, 225));
        QCOMPARE(delivered[1].size(), QSize(150, 225));

        loader->load(fixture.url(), QSize(150, 225), &context,
                     [&delivered](const QImage &image) { delivered.append(image); });
        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 3, 250);
        QCOMPARE(fixture.requestCount, 1);
    }

    void dropsDeadConsumersAndReleasesTheirPipelineSlot() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        QVERIFY(fixture.listen());
        QScopedPointer<CloudStream::ArtworkLoader> loader(localLoader(cache.path()));
        int staleDeliveries = 0;
        auto *staleContext = new QObject;

        loader->load(fixture.url(), QSize(150, 225), staleContext,
                     [&staleDeliveries](const QImage &) { ++staleDeliveries; });
        delete staleContext;
        QTest::qWait(100);
        QCOMPARE(staleDeliveries, 0);

        QObject liveContext;
        QImage liveImage;
        loader->load(fixture.url(), QSize(150, 225), &liveContext,
                     [&liveImage](const QImage &image) { liveImage = image; });
        QTRY_VERIFY_WITH_TIMEOUT(!liveImage.isNull(), 3000);
        QCOMPARE(staleDeliveries, 0);
        QVERIFY(fixture.requestCount >= 1);
        QVERIFY(fixture.requestCount <= 2);
    }

    void fitModePreservesTheSourceAspectRatio() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        QVERIFY(fixture.listen());
        QScopedPointer<CloudStream::ArtworkLoader> loader(localLoader(cache.path()));
        QObject context;
        QImage delivered;

        loader->load(fixture.url(), QSize(64, 64), &context,
                     [&delivered](const QImage &image) { delivered = image; },
                     CloudStream::ArtworkLoader::HighPriority,
                     CloudStream::ArtworkLoader::FitInside);

        QTRY_VERIFY_WITH_TIMEOUT(!delivered.isNull(), 3000);
        QCOMPARE(delivered.size(), QSize(43, 64));
    }

    void rejectsUnsafeSchemesAndPrivateTargetsByDefault() {
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(QUrl("file:///etc/passwd")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://127.0.0.1/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://192.168.1.1/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:192.168.1.1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:127.0.0.1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:169.254.1.1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:100.64.0.1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:192.0.2.1]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://[::ffff:240.0.0.1]/image.jpg")));
        QVERIFY(CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("https://[::ffff:8.8.8.8]/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://localhost/image.jpg")));
        QVERIFY(!CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://localhost./image.jpg")));
        QVERIFY(CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("https://image.tmdb.org/poster.jpg")));
        QVERIFY(CloudStream::ArtworkLoader::isUrlAllowed(
            QUrl("http://127.0.0.1/image.jpg"),
            CloudStream::ArtworkLoader::AllowPrivateNetwork));
    }

    void rejectsHostnamesThatResolveToPrivateAddresses() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        QVERIFY(fixture.listen());
        CloudStream::ArtworkLoader loader(cache.path());
        QObject context;
        int delivered = 0;
        QUrl aliasUrl = fixture.url();
        aliasUrl.setHost("ip6-localhost");

        loader.load(aliasUrl, QSize(150, 225), &context,
                    [&delivered](const QImage &) { ++delivered; });

        QTest::qWait(500);
        QCOMPARE(fixture.requestCount, 0);
        QCOMPARE(delivered, 0);
    }

    void rejectsExtremeAspectDecodeDimensionsBeforeIntegerConversion() {
        QCOMPARE(CloudStream::ArtworkLoader::decodeSizeFor(
                     QSize(2000, 3000), QSize(150, 225),
                     CloudStream::ArtworkLoader::CenterCrop),
                 QSize(150, 225));
        QVERIFY(CloudStream::ArtworkLoader::decodeSizeFor(
                    QSize(1, 10000), QSize(150, 225),
                    CloudStream::ArtworkLoader::CenterCrop).isEmpty());
        QVERIFY(CloudStream::ArtworkLoader::decodeSizeFor(
                    QSize(10000, 1), QSize(150, 225),
                    CloudStream::ArtworkLoader::CenterCrop).isEmpty());
    }

    void consumerJoiningDuringDecodeReusesTheInflightPayload() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        QVERIFY(fixture.listen());
        QScopedPointer<CloudStream::ArtworkLoader> loader(localLoader(cache.path()));
        QObject context;
        QList<QImage> delivered;
        bool joined = false;
        connect(loader.data(), &CloudStream::ArtworkLoader::decodeStarted, loader.data(),
                [&](const QUrl &) {
            if (joined) return;
            joined = true;
            loader->load(fixture.url(), QSize(150, 225), &context,
                         [&delivered](const QImage &image) { delivered.append(image); });
        });

        loader->load(fixture.url(), QSize(150, 225), &context,
                     [&delivered](const QImage &image) { delivered.append(image); });

        QTRY_COMPARE_WITH_TIMEOUT(delivered.size(), 2, 3000);
        QCOMPARE(fixture.requestCount, 1);
    }

    void rejectsResponsesWhoseDeclaredSizeExceedsTheLimit() {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        ArtworkHttpFixture fixture;
        fixture.declaredLength = CloudStream::ArtworkLoader::maximumDownloadBytes() + 1;
        QVERIFY(fixture.listen());
        QScopedPointer<CloudStream::ArtworkLoader> loader(localLoader(cache.path()));
        QObject context;
        int delivered = 0;

        loader->load(fixture.url(), QSize(150, 225), &context,
                     [&delivered](const QImage &) { ++delivered; });

        QTRY_COMPARE_WITH_TIMEOUT(fixture.requestCount, 1, 1000);
        QTest::qWait(200);
        QCOMPARE(delivered, 0);
    }
};

QTEST_MAIN(ArtworkLoaderTest)
#include "test_artwork_loader.moc"
