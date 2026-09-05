#pragma once

#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace CloudStream {

struct EpisodeEntry {
    QString data;
    QString name;
    QString description;
    QString posterUrl;
    int season = 0;
    int number = 0;
    QStringList dubStatuses;
};

class EpisodeCatalog final {
public:
    static QList<EpisodeEntry> fromJson(const QJsonArray &values);
    static QList<int> seasons(const QList<EpisodeEntry> &episodes);
    static QStringList dubStatuses(const QList<EpisodeEntry> &episodes);
    static QList<EpisodeEntry> filter(const QList<EpisodeEntry> &episodes,
                                      int season, const QString &dubStatus,
                                      const QString &query);
    static QList<EpisodeEntry> page(const QList<EpisodeEntry> &episodes,
                                    int offset, int limit);
};

} // namespace CloudStream
