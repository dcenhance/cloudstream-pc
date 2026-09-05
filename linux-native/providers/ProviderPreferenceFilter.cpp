#include "ProviderPreferenceFilter.h"

#include <QJsonArray>
#include <QMap>

namespace CloudStream {

QList<QJsonObject> ProviderPreferenceFilter::apply(const QList<QJsonObject> &providers,
                                                   const QString &language,
                                                   bool allowNsfw) {
    static const QMap<QString, QString> languageCodes{
        {"German", "de"}, {"English", "en"}, {"Italian", "it"},
        {"Spanish", "es"}, {"French", "fr"}, {"Vietnamese", "vi"},
    };
    const auto requiredLanguage = languageCodes.value(language);
    QList<QJsonObject> filtered;
    for (const auto &provider : providers) {
        bool nsfw = false;
        for (const auto &type : provider.value("supportedTypes").toArray()) {
            if (type.toString().compare("NSFW", Qt::CaseInsensitive) == 0) {
                nsfw = true;
                break;
            }
        }
        if (nsfw && !allowNsfw) continue;
        if (!requiredLanguage.isEmpty() &&
            provider.value("language").toString().compare(requiredLanguage, Qt::CaseInsensitive) != 0) continue;
        filtered.append(provider);
    }
    return filtered;
}

} // namespace CloudStream
