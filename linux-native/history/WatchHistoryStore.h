#pragma once

#include <QList>
#include <QString>

namespace CloudStream {

struct WatchEntry {
    QString id;
    QString name;
    QString sourceUrl;
    QString provider;
    QString jarPath;
    QString posterUrl;
    QString playbackData;
    QString episodeName;
    QString state = "Watching";
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
    qint64 updatedAt = 0;
};

class WatchHistoryStore final {
public:
    explicit WatchHistoryStore(QString filePath);

    static QString idFor(const QString &provider, const QString &sourceUrl);
    QList<WatchEntry> entries(const QString &state = {}) const;
    bool upsert(const WatchEntry &entry);
    bool updateProgress(const QString &id, double positionSeconds,
                        double durationSeconds, qint64 updatedAt = 0);
    bool setState(const QString &id, const QString &state);
    bool remove(const QString &id);

private:
    QString filePath;
    QList<WatchEntry> readEntries() const;
    bool writeEntries(const QList<WatchEntry> &entries) const;
};

} // namespace CloudStream
