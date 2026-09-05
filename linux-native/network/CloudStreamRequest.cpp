#include "CloudStreamRequest.h"

namespace CloudStream {

QNetworkRequest CloudStreamRequest::metadata(const QUrl &url) {
    QNetworkRequest request(url);
    request.setRawHeader(
        "User-Agent",
        "CloudStream-Linux/0.1 (+https://github.com/recloudstream/cloudstream)");
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy);
    // Large repository indexes fan out to many URLs on the same host. Some
    // GitHub/CDN endpoints refuse concurrent HTTP/2 streams, so metadata uses
    // Qt's bounded HTTP/1.1 connection pool instead.
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // Millisecond overload also supports the Qt 6.4 Ubuntu LTS baseline.
    request.setTransferTimeout(20000);
    return request;
}

QNetworkRequest CloudStreamRequest::artwork(const QUrl &url) {
    auto request = metadata(url);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    return request;
}

} // namespace CloudStream
