#pragma once

#include <QNetworkRequest>
#include <QUrl>

namespace CloudStream {

class CloudStreamRequest final {
public:
    static QNetworkRequest metadata(const QUrl &url);
    static QNetworkRequest artwork(const QUrl &url);
};

} // namespace CloudStream
