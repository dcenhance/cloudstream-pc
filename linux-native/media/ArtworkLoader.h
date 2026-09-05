#pragma once

#include <QCache>
#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QThreadPool>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace CloudStream {

class ArtworkLoader final : public QObject {
    Q_OBJECT
public:
    enum Priority {
        LowPriority,
        NormalPriority,
        HighPriority,
    };

    enum Transform {
        CenterCrop,
        FitInside,
    };

    enum NetworkPolicy {
        PublicNetworkOnly,
        AllowPrivateNetwork,
    };

    using Callback = std::function<void(const QImage &)>;

    explicit ArtworkLoader(const QString &cacheDirectory, QObject *parent = nullptr,
                           NetworkPolicy networkPolicy = PublicNetworkOnly);
    void load(const QUrl &url, const QSize &targetSize, QObject *context,
              Callback callback, Priority priority = NormalPriority,
              Transform transform = CenterCrop);
    static bool isUrlAllowed(const QUrl &url,
                             NetworkPolicy networkPolicy = PublicNetworkOnly);
    static qint64 maximumDownloadBytes();
    static QSize decodeSizeFor(const QSize &sourceSize, const QSize &targetSize,
                               Transform transform);

signals:
    void decodeStarted(const QUrl &url);

private:
    struct Consumer {
        QPointer<QObject> context;
        QSize targetSize;
        Transform transform = CenterCrop;
        QString cacheKey;
        Callback callback;
    };

    struct PendingRequest {
        QUrl url;
        QList<Consumer> consumers;
        QList<Consumer> decodingConsumers;
        Priority priority = NormalPriority;
        QPointer<QNetworkReply> reply;
        QByteArray bytes;
        bool active = false;
        bool decoding = false;
        bool resolvingHost = false;
        bool tooLarge = false;
        int redirectCount = 0;
        int hostLookupId = -1;
    };

    struct HostLookup {
        int id = -1;
        QStringList requestKeys;
    };

    static QString urlKey(const QUrl &url);
    static QString imageKey(const QUrl &url, const QSize &targetSize, Transform transform);
    static QString transformKey(const QSize &targetSize, Transform transform);
    static QImage decodeAtDisplaySize(const QByteArray &bytes, const QSize &targetSize,
                                      Transform transform);
    void pump();
    void start(const QString &key);
    void issueNetworkRequest(const QString &key);
    void sendNetworkRequest(const QString &key);
    void finish(const QString &key, QNetworkReply *reply);
    void startDecode(const QString &key);
    void finishDecode(const QString &key, const QHash<QString, QImage> &images);
    void prune(const QString &key);
    void complete(const QString &key);
    void removeHostLookupWaiter(const QString &key, int lookupId);
    void deliver(const QList<Consumer> &consumers,
                 const QHash<QString, QImage> &images);

    QNetworkAccessManager *network_{};
    QCache<QString, QImage> memoryCache_;
    QHash<QString, PendingRequest> pending_;
    QHash<QString, HostLookup> hostLookups_;
    QHash<QString, qint64> approvedHostsUntil_;
    QHash<QString, qint64> rejectedHostsUntil_;
    QStringList queue_;
    QThreadPool decodePool_;
    QElapsedTimer hostSafetyClock_;
    NetworkPolicy networkPolicy_ = PublicNetworkOnly;
    int activeRequests_ = 0;
    int maximumActiveRequests_ = 8;
};

} // namespace CloudStream
