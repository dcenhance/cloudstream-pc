#include "EpisodeCatalog.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <algorithm>

namespace CloudStream {

QList<EpisodeEntry> EpisodeCatalog::fromJson(const QJsonArray &values) {
    QList<EpisodeEntry> result;
    QHash<QString, qsizetype> indexes;
    for (const auto &value : values) {
        const auto object = value.toObject();
        EpisodeEntry candidate;
        candidate.data = object.value("data").toString();
        candidate.name = object.value("name").toString();
        candidate.description = object.value("description").toString();
        candidate.posterUrl = object.value("posterUrl").toString();
        candidate.season = object.value("season").toInt();
        candidate.number = object.value("episode").toInt();
        const auto dubStatus = object.value("dubStatus").toString();
        if (!dubStatus.isEmpty() && dubStatus != "None") candidate.dubStatuses << dubStatus;
        auto identity = candidate.data;
        if (identity.isEmpty()) {
            identity = QString::number(candidate.season) + "\n" + QString::number(candidate.number) + "\n" + candidate.name;
        }
        if (!indexes.contains(identity)) {
            indexes.insert(identity, result.size());
            result << candidate;
            continue;
        }
        auto &existing = result[indexes.value(identity)];
        for (const auto &status : candidate.dubStatuses) {
            if (!existing.dubStatuses.contains(status)) existing.dubStatuses << status;
        }
        if (existing.name.isEmpty()) existing.name = candidate.name;
        if (existing.description.isEmpty()) existing.description = candidate.description;
        if (existing.posterUrl.isEmpty()) existing.posterUrl = candidate.posterUrl;
    }
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.season != right.season) return left.season < right.season;
        if (left.number != right.number) return left.number < right.number;
        return left.name.localeAwareCompare(right.name) < 0;
    });
    return result;
}

QList<int> EpisodeCatalog::seasons(const QList<EpisodeEntry> &episodes) {
    QSet<int> values;
    for (const auto &episode : episodes) {
        if (episode.season > 0) values.insert(episode.season);
    }
    auto result = values.values();
    std::sort(result.begin(), result.end());
    return result;
}

QStringList EpisodeCatalog::dubStatuses(const QList<EpisodeEntry> &episodes) {
    QSet<QString> values;
    for (const auto &episode : episodes) {
        for (const auto &status : episode.dubStatuses) values.insert(status);
    }
    QStringList result;
    for (const auto &preferred : {QString("Dubbed"), QString("Subbed")}) {
        if (values.remove(preferred)) result << preferred;
    }
    auto remainder = values.values();
    std::sort(remainder.begin(), remainder.end());
    result << remainder;
    return result;
}

QList<EpisodeEntry> EpisodeCatalog::filter(const QList<EpisodeEntry> &episodes,
                                           int season, const QString &dubStatus,
                                           const QString &query) {
    QList<EpisodeEntry> result;
    const auto term = query.trimmed();
    for (const auto &episode : episodes) {
        if (season > 0 && episode.season != season) continue;
        if (!dubStatus.isEmpty() && !episode.dubStatuses.contains(dubStatus)) continue;
        if (!term.isEmpty() && !episode.name.contains(term, Qt::CaseInsensitive) &&
            !episode.description.contains(term, Qt::CaseInsensitive)) continue;
        result << episode;
    }
    return result;
}

QList<EpisodeEntry> EpisodeCatalog::page(const QList<EpisodeEntry> &episodes,
                                         int offset, int limit) {
    if (limit <= 0 || episodes.isEmpty()) return {};
    const auto start = std::max(0, offset);
    if (start >= episodes.size()) return {};
    return episodes.mid(start, limit);
}

} // namespace CloudStream
