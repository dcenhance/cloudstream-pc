#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>

namespace CloudStream {

struct PlaybackAudioTrack {
    QString url;
    QMap<QString, QString> headers;
};

struct PlaybackSegment {
    QString url;
    qint64 durationUs = 0;
};

struct PlaybackSource {
    QString source;
    QString name;
    QString url;
    QString referer;
    int quality = 0;
    QString type;
    QMap<QString, QString> headers;
    QList<PlaybackAudioTrack> audioTracks;
    QList<PlaybackSegment> playlist;

    QString hosterName() const;
    QString displayLabel() const;
    QMap<QString, QString> httpHeaders() const;
};

struct PlaybackSubtitle {
    QString language;
    QString url;
    QMap<QString, QString> headers;
};

struct SourceDiscovery {
    bool success = false;
    QList<PlaybackSource> sources;
    QList<PlaybackSubtitle> subtitles;
};

class SourceCatalog final {
public:
    static SourceDiscovery parse(const QByteArray &payload);
};

} // namespace CloudStream
