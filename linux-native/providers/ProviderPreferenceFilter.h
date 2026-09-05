#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

namespace CloudStream {

class ProviderPreferenceFilter final {
public:
    static QList<QJsonObject> apply(const QList<QJsonObject> &providers,
                                    const QString &language,
                                    bool allowNsfw);
};

} // namespace CloudStream
