#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace CloudStream {

class DetailsPresentation final {
public:
    static QString backdropUrl(const QJsonObject &details);
    static QStringList facts(const QJsonObject &details, const QString &provider);
};

} // namespace CloudStream
