#include "ArtworkLoader.h"

#include "ArtworkSizing.h"
#include "../network/CloudStreamRequest.h"

#include <QBuffer>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QHostInfo>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>

namespace CloudStream {
namespace {
constexpr int memoryCacheKilobytes = 96 * 1024;
constexpr qint64 maximumArtworkBytes = 20LL * 1024LL * 1024LL;
constexpr qint64 maximumSourcePixels = 64LL * 1024LL * 1024LL;
constexpr qint64 maximumDecodedPixels = 16LL * 1024LL * 1024LL;
constexpr int maximumDecodedDimension = 16384;
constexpr int maximumRedirects = 5;
constexpr qint64 hostSafetyCacheMs = 30000;

QString sizeKey(const QSize &size) {
    return QString::number(size.width()) + "x" + QString::number(size.height());
}

bool isNonPublicIpv4(quint32 address) {
    struct Range {
        quint32 network;
        quint32 mask;
    };
    static constexpr Range blocked[] = {
        {0x00000000U, 0xff000000U}, // Current network
        {0x0a000000U, 0xff000000U}, // RFC1918
        {0x64400000U, 0xffc00000U}, // Carrier-grade NAT
        {0x7f000000U, 0xff000000U}, // Loopback
        {0xa9fe0000U, 0xffff0000U}, // Link-local
        {0xac100000U, 0xfff00000U}, // RFC1918
        {0xc0000000U, 0xffffff00U}, // IETF protocol assignments
        {0xc0000200U, 0xffffff00U}, // TEST-NET-1
        {0xc0586300U, 0xffffff00U}, // Deprecated 6to4 relay anycast
        {0xc0a80000U, 0xffff0000U}, // RFC1918
        {0xc6120000U, 0xfffe0000U}, // Benchmarking
        {0xc6336400U, 0xffffff00U}, // TEST-NET-2
        {0xcb007100U, 0xffffff00U}, // TEST-NET-3
        {0xe0000000U, 0xf0000000U}, // Multicast
        {0xf0000000U, 0xf0000000U}, // Reserved and broadcast
    };
    return std::any_of(std::begin(blocked), std::end(blocked),
                       [address](const Range &range) {
        return (address & range.mask) == range.network;
    });
}

bool isPrivateAddress(const QHostAddress &address) {
    bool hasEmbeddedIpv4 = false;
    const auto embeddedIpv4 = address.toIPv4Address(&hasEmbeddedIpv4);
    if (hasEmbeddedIpv4) {
        return isNonPublicIpv4(embeddedIpv4);
    }
    return address.isNull() || !address.isGlobal() || address.isMulticast() ||
        address.isBroadcast();
}
}

ArtworkLoader::ArtworkLoader(const QString &cacheDirectory, QObject *parent,
                             NetworkPolicy networkPolicy)
    : QObject(parent), network_(new QNetworkAccessManager(this)),
      memoryCache_(memoryCacheKilobytes), networkPolicy_(networkPolicy) {
    auto *diskCache = new QNetworkDiskCache(network_);
    diskCache->setCacheDirectory(cacheDirectory);
    diskCache->setMaximumCacheSize(256LL * 1024LL * 1024LL);
    network_->setCache(diskCache);
    decodePool_.setMaxThreadCount(3);
    decodePool_.setExpiryTimeout(30000);
    hostSafetyClock_.start();
}

qint64 ArtworkLoader::maximumDownloadBytes() {
    return maximumArtworkBytes;
}

bool ArtworkLoader::isUrlAllowed(const QUrl &url, NetworkPolicy networkPolicy) {
    if (!url.isValid()) return false;
    const auto scheme = url.scheme().toLower();
    if ((scheme != "http" && scheme != "https") || url.host().isEmpty()) return false;
    if (networkPolicy == AllowPrivateNetwork) return true;

    auto host = url.host().toLower();
    while (host.endsWith('.')) host.chop(1);
    if (host == "localhost" || host.endsWith(".localhost") || host.endsWith(".local") ||
        host.endsWith(".internal") || host == "home.arpa" || host.endsWith(".home.arpa")) {
        return false;
    }
    QHostAddress address;
    return !address.setAddress(host) || !isPrivateAddress(address);
}

QSize ArtworkLoader::decodeSizeFor(const QSize &sourceSize, const QSize &targetSize,
                                   Transform transform) {
    if (!sourceSize.isValid() || sourceSize.isEmpty() ||
        !targetSize.isValid() || targetSize.isEmpty()) {
        return {};
    }
    const long double horizontalScale =
        static_cast<long double>(targetSize.width()) / sourceSize.width();
    const long double verticalScale =
        static_cast<long double>(targetSize.height()) / sourceSize.height();
    const long double scale = transform == FitInside
        ? std::min(horizontalScale, verticalScale)
        : std::max(horizontalScale, verticalScale);
    const auto stableCeil = [](long double value) {
        const auto tolerance = std::max(1.0L, std::abs(value)) * 1.0e-12L;
        return std::ceil(value - tolerance);
    };
    const long double scaledWidth = stableCeil(sourceSize.width() * scale);
    const long double scaledHeight = stableCeil(sourceSize.height() * scale);
    if (!std::isfinite(static_cast<double>(scaledWidth)) ||
        !std::isfinite(static_cast<double>(scaledHeight)) ||
        scaledWidth < 1.0L || scaledHeight < 1.0L ||
        scaledWidth > maximumDecodedDimension ||
        scaledHeight > maximumDecodedDimension ||
        scaledWidth * scaledHeight > maximumDecodedPixels) {
        return {};
    }
    return QSize(int(scaledWidth), int(scaledHeight));
}

QString ArtworkLoader::urlKey(const QUrl &url) {
    return QString::fromUtf8(url.toEncoded(QUrl::FullyEncoded));
}

QString ArtworkLoader::transformKey(const QSize &targetSize, Transform transform) {
    return sizeKey(targetSize) + (transform == FitInside ? "#fit" : "#crop");
}

QString ArtworkLoader::imageKey(const QUrl &url, const QSize &targetSize,
                                Transform transform) {
    return urlKey(url) + "#" + transformKey(targetSize, transform);
}

void ArtworkLoader::load(const QUrl &url, const QSize &targetSize, QObject *context,
                         Callback callback, Priority priority, Transform transform) {
    if (!isUrlAllowed(url, networkPolicy_) || !targetSize.isValid() ||
        targetSize.isEmpty() || !context || !callback) {
        return;
    }

    const auto transformedKey = imageKey(url, targetSize, transform);
    if (const auto *cached = memoryCache_.object(transformedKey)) {
        const QPointer<QObject> safeContext(context);
        const QImage image = *cached;
        QTimer::singleShot(0, this, [safeContext, image, callback = std::move(callback)] {
            if (safeContext) callback(image);
        });
        return;
    }

    const auto key = urlKey(url);
    connect(context, &QObject::destroyed, this, [this, key] { prune(key); });
    Consumer consumer{context, targetSize, transform, transformedKey, std::move(callback)};
    auto existing = pending_.find(key);
    if (existing != pending_.end()) {
        existing->consumers.append(std::move(consumer));
        if (priority > existing->priority) existing->priority = priority;
        return;
    }

    PendingRequest request;
    request.url = url;
    request.priority = priority;
    request.consumers.append(std::move(consumer));
    pending_.insert(key, std::move(request));
    queue_.append(key);
    pump();
}

void ArtworkLoader::pump() {
    const auto queuedKeys = queue_;
    for (const auto &key : queuedKeys) prune(key);
    while (activeRequests_ < maximumActiveRequests_ && !queue_.isEmpty()) {
        auto selected = queue_.begin();
        for (auto it = queue_.begin(); it != queue_.end(); ++it) {
            if (pending_.value(*it).priority > pending_.value(*selected).priority) selected = it;
        }
        const auto key = *selected;
        queue_.erase(selected);
        start(key);
    }
}

void ArtworkLoader::start(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end() || pending->active) return;
    pending->active = true;
    ++activeRequests_;

    issueNetworkRequest(key);
}

void ArtworkLoader::issueNetworkRequest(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end()) return;
    if (!isUrlAllowed(pending->url, networkPolicy_)) {
        complete(key);
        return;
    }
    if (networkPolicy_ == AllowPrivateNetwork) {
        sendNetworkRequest(key);
        return;
    }

    QHostAddress literalAddress;
    if (literalAddress.setAddress(pending->url.host())) {
        if (isPrivateAddress(literalAddress)) complete(key);
        else sendNetworkRequest(key);
        return;
    }
    auto host = pending->url.host().toLower();
    while (host.endsWith('.')) host.chop(1);
    const auto now = hostSafetyClock_.elapsed();
    if (approvedHostsUntil_.value(host, -1) > now) {
        sendNetworkRequest(key);
        return;
    }
    if (rejectedHostsUntil_.value(host, -1) > now) {
        complete(key);
        return;
    }
    if (auto existingLookup = hostLookups_.find(host);
        existingLookup != hostLookups_.end()) {
        if (!existingLookup->requestKeys.contains(key)) {
            existingLookup->requestKeys.append(key);
        }
        pending->resolvingHost = true;
        pending->hostLookupId = existingLookup->id;
        return;
    }
    pending->resolvingHost = true;
    const auto expectedUrl = pending->url;
    const auto lookupId = QHostInfo::lookupHost(
        expectedUrl.host(), this, [this, host](const QHostInfo &hostInfo) {
        const auto lookup = hostLookups_.take(host);
        const bool approved = hostInfo.error() == QHostInfo::NoError &&
            !hostInfo.addresses().isEmpty() &&
            std::none_of(hostInfo.addresses().cbegin(), hostInfo.addresses().cend(),
                         [](const QHostAddress &address) {
                return isPrivateAddress(address);
            });
        const auto expires = hostSafetyClock_.elapsed() + hostSafetyCacheMs;
        if (approved) approvedHostsUntil_.insert(host, expires);
        else rejectedHostsUntil_.insert(host, expires);
        for (const auto &requestKey : lookup.requestKeys) {
            auto pending = pending_.find(requestKey);
            if (pending == pending_.end() || pending->hostLookupId != lookup.id) continue;
            pending->resolvingHost = false;
            pending->hostLookupId = -1;
            auto currentHost = pending->url.host().toLower();
            while (currentHost.endsWith('.')) currentHost.chop(1);
            if (currentHost != host || !approved) complete(requestKey);
            else sendNetworkRequest(requestKey);
        }
    });
    pending->hostLookupId = lookupId;
    hostLookups_.insert(host, HostLookup{lookupId, {key}});
}

void ArtworkLoader::sendNetworkRequest(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end()) return;

    auto request = CloudStreamRequest::artwork(pending->url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    if (pending->priority == HighPriority) request.setPriority(QNetworkRequest::HighPriority);
    else if (pending->priority == LowPriority) request.setPriority(QNetworkRequest::LowPriority);
    auto *reply = network_->get(request);
    reply->setReadBufferSize(1024 * 1024);
    pending->reply = reply;

    connect(reply, &QIODevice::readyRead, this, [this, key, reply] {
        auto pending = pending_.find(key);
        if (pending == pending_.end() || pending->reply != reply) return;
        const auto chunk = reply->readAll();
        if (pending->bytes.size() + qint64(chunk.size()) > maximumArtworkBytes) {
            pending->tooLarge = true;
            reply->abort();
            return;
        }
        pending->bytes.append(chunk);
    });
    connect(reply, &QNetworkReply::metaDataChanged, this, [this, key, reply] {
        auto pending = pending_.find(key);
        if (pending == pending_.end() || pending->reply != reply) return;
        bool ok = false;
        const auto declaredSize = reply->header(QNetworkRequest::ContentLengthHeader)
                                      .toLongLong(&ok);
        if (ok && declaredSize > maximumArtworkBytes) {
            pending->tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, key, reply](qint64 received, qint64 total) {
        auto pending = pending_.find(key);
        if (pending == pending_.end() || pending->reply != reply) return;
        if (received > maximumArtworkBytes || total > maximumArtworkBytes) {
            pending->tooLarge = true;
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, key, reply] { finish(key, reply); });
}

void ArtworkLoader::finish(const QString &key, QNetworkReply *reply) {
    auto pending = pending_.find(key);
    if (pending == pending_.end() || pending->reply != reply) {
        reply->deleteLater();
        return;
    }
    const auto remainder = reply->isReadable() ? reply->readAll() : QByteArray();
    if (pending->bytes.size() + qint64(remainder.size()) > maximumArtworkBytes) {
        pending->tooLarge = true;
    } else {
        pending->bytes.append(remainder);
    }
    const auto networkError = reply->error();
    const auto redirectValue = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    const auto redirect = redirectValue.isValid()
        ? pending->url.resolved(redirectValue.toUrl()) : QUrl();
    pending->reply = nullptr;
    reply->deleteLater();

    if (redirectValue.isValid()) {
        if (pending->redirectCount >= maximumRedirects ||
            !isUrlAllowed(redirect, networkPolicy_)) {
            complete(key);
            return;
        }
        ++pending->redirectCount;
        pending->url = redirect;
        pending->bytes.clear();
        pending->tooLarge = false;
        issueNetworkRequest(key);
        return;
    }
    if (networkError != QNetworkReply::NoError || pending->tooLarge ||
        pending->bytes.isEmpty()) {
        complete(key);
        return;
    }

    prune(key);
    if (!pending_.contains(key)) return;
    startDecode(key);
}

void ArtworkLoader::startDecode(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end() || pending->decoding) return;
    prune(key);
    pending = pending_.find(key);
    if (pending == pending_.end()) return;
    if (pending->consumers.isEmpty()) {
        complete(key);
        return;
    }

    pending->decodingConsumers = std::move(pending->consumers);
    pending->consumers.clear();
    pending->decoding = true;
    emit decodeStarted(pending->url);

    QList<QPair<QSize, Transform>> transforms;
    for (const auto &consumer : pending->decodingConsumers) {
        const auto transform = qMakePair(consumer.targetSize, consumer.transform);
        if (!transforms.contains(transform)) transforms.append(transform);
    }

    using DecodeResult = QHash<QString, QImage>;
    auto *watcher = new QFutureWatcher<DecodeResult>(this);
    connect(watcher, &QFutureWatcher<DecodeResult>::finished, this,
            [this, watcher, key] {
        finishDecode(key, watcher->result());
        watcher->deleteLater();
    });
    const auto bytes = pending->bytes;
    watcher->setFuture(QtConcurrent::run(&decodePool_, [bytes, transforms] {
        DecodeResult result;
        for (const auto &[size, transform] : transforms) {
            const auto image = decodeAtDisplaySize(bytes, size, transform);
            if (!image.isNull()) result.insert(transformKey(size, transform), image);
        }
        return result;
    }));
}

void ArtworkLoader::finishDecode(const QString &key,
                                 const QHash<QString, QImage> &images) {
    auto pending = pending_.find(key);
    if (pending == pending_.end()) return;
    pending->decoding = false;

    auto completedConsumers = std::move(pending->decodingConsumers);
    pending->decodingConsumers.clear();
    QList<Consumer> stillWaiting;
    for (auto &consumer : pending->consumers) {
        if (images.contains(transformKey(consumer.targetSize, consumer.transform))) {
            completedConsumers.append(std::move(consumer));
        } else {
            stillWaiting.append(std::move(consumer));
        }
    }
    pending->consumers = std::move(stillWaiting);
    deliver(completedConsumers, images);

    prune(key);
    pending = pending_.find(key);
    if (pending == pending_.end()) return;
    if (!pending->consumers.isEmpty()) startDecode(key);
    else complete(key);
}

void ArtworkLoader::prune(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end()) return;
    const auto removeDead = [](QList<Consumer> &consumers) {
        consumers.erase(std::remove_if(consumers.begin(), consumers.end(),
            [](const Consumer &consumer) { return consumer.context.isNull(); }),
            consumers.end());
    };
    removeDead(pending->consumers);
    removeDead(pending->decodingConsumers);
    if (!pending->consumers.isEmpty() || !pending->decodingConsumers.isEmpty()) return;

    if (pending->reply) {
        pending->reply->abort();
        return;
    }
    if (pending->hostLookupId >= 0) {
        removeHostLookupWaiter(key, pending->hostLookupId);
        pending->hostLookupId = -1;
        pending->resolvingHost = false;
    }
    if (!pending->decoding) complete(key);
}

void ArtworkLoader::complete(const QString &key) {
    auto pending = pending_.find(key);
    if (pending == pending_.end()) return;
    const bool wasActive = pending->active;
    if (pending->hostLookupId >= 0) {
        removeHostLookupWaiter(key, pending->hostLookupId);
    }
    pending_.erase(pending);
    queue_.removeAll(key);
    if (wasActive && activeRequests_ > 0) --activeRequests_;
    QTimer::singleShot(0, this, [this] { pump(); });
}

void ArtworkLoader::removeHostLookupWaiter(const QString &key, int lookupId) {
    for (auto it = hostLookups_.begin(); it != hostLookups_.end(); ++it) {
        if (it->id != lookupId) continue;
        it->requestKeys.removeAll(key);
        if (it->requestKeys.isEmpty()) {
            QHostInfo::abortHostLookup(it->id);
            hostLookups_.erase(it);
        }
        return;
    }
}

QImage ArtworkLoader::decodeAtDisplaySize(const QByteArray &bytes, const QSize &targetSize,
                                          Transform transform) {
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) return {};
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const auto sourceSize = reader.size();
    if (!sourceSize.isValid() || sourceSize.width() <= 0 || sourceSize.height() <= 0 ||
        qint64(sourceSize.width()) * qint64(sourceSize.height()) > maximumSourcePixels) {
        return {};
    }
    const auto decodeSize = decodeSizeFor(sourceSize, targetSize, transform);
    if (decodeSize.isEmpty()) return {};
    if (decodeSize.width() < sourceSize.width() || decodeSize.height() < sourceSize.height()) {
        reader.setScaledSize(decodeSize);
    }
    const auto decoded = reader.read();
    if (transform == FitInside) {
        return decoded.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return ArtworkSizing::centerCrop(decoded, targetSize);
}

void ArtworkLoader::deliver(const QList<Consumer> &consumers,
                            const QHash<QString, QImage> &images) {
    for (const auto &consumer : consumers) {
        if (!consumer.context) continue;
        const auto image = images.value(transformKey(consumer.targetSize, consumer.transform));
        if (image.isNull()) continue;
        if (!memoryCache_.contains(consumer.cacheKey)) {
            const auto cost = std::max(1, int(image.sizeInBytes() / 1024));
            memoryCache_.insert(consumer.cacheKey, new QImage(image), cost);
        }
        consumer.callback(image);
    }
}

} // namespace CloudStream
