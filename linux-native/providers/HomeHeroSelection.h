#pragma once

#include <QJsonArray>
#include <QString>

namespace CloudStream {

struct HomeHeroItem {
    QString name;
    QString url;
    QString posterUrl;
    QString providerName;
    bool valid = false;
};

class HomeHeroSelection final {
public:
    static HomeHeroItem select(const QJsonArray &sections, const QString &fallbackProvider);
};

} // namespace CloudStream
