#pragma once

#include <QList>
#include <QMap>
#include <QString>
#include <optional>

namespace CloudStream {

enum class DownloadState {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
};

struct DownloadEntry {
    QString id;
    QString title;
    QString url;
    QString targetPath;
    QString artifactPath;
    QString provider;
    QString playbackData;
    QString sourceName;
    QString sourceHoster;
    int sourceQuality = 0;
    QString sourceType = "VIDEO";
    QMap<QString, QString> headers;
    DownloadState state = DownloadState::Queued;
    qint64 bytesReceived = 0;
    qint64 bytesTotal = 0;
    qint64 createdAt = 0;
    qint64 updatedAt = 0;
    QString error;

    QString partPath() const { return targetPath + ".part"; }
};

class DownloadQueueStore final {
public:
    explicit DownloadQueueStore(QString path);

    QList<DownloadEntry> entries() const;
    std::optional<DownloadEntry> entry(const QString &id) const;
    bool upsert(DownloadEntry entry);
    bool remove(const QString &id);
    QString errorString() const;

    static QString stateName(DownloadState state);
    static DownloadState stateFromName(const QString &state);

private:
    bool load();
    bool persist();

    QString path_;
    QList<DownloadEntry> entries_;
    QString error_;
};

} // namespace CloudStream
