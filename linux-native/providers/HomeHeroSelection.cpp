#include "HomeHeroSelection.h"

#include <QJsonObject>

namespace CloudStream {

HomeHeroItem HomeHeroSelection::select(const QJsonArray &sections, const QString &fallbackProvider) {
    HomeHeroItem fallback;
    for (const auto &sectionValue : sections) {
        const auto section = sectionValue.toObject();
        for (const auto &itemValue : section.value("items").toArray()) {
            const auto item = itemValue.toObject();
            HomeHeroItem candidate;
            candidate.name = item.value("name").toString().trimmed();
            candidate.url = item.value("url").toString().trimmed();
            candidate.posterUrl = item.value("backdropUrl").toString().trimmed();
            if (candidate.posterUrl.isEmpty()) candidate.posterUrl = item.value("posterUrl").toString().trimmed();
            candidate.providerName = item.value("apiName").toString().trimmed();
            if (candidate.providerName.isEmpty()) candidate.providerName = fallbackProvider;
            candidate.valid = !candidate.name.isEmpty() && !candidate.url.isEmpty();
            if (!candidate.valid) continue;
            if (!fallback.valid) fallback = candidate;
            if (!candidate.posterUrl.isEmpty()) return candidate;
        }
    }
    return fallback;
}

} // namespace CloudStream
