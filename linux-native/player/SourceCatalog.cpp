#include "SourceCatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace CloudStream {
namespace {
QMap<QString, QString> stringMap(const QJsonObject &object) {
    QMap<QString, QString> values;
    for (auto iterator = object.begin(); iterator != object.end(); ++iterator) {
        if (iterator.value().isString()) values.insert(iterator.key(), iterator.value().toString());
    }
    return values;
}

QString normalizedType(const QString &url, const QString &declaredType) {
    const auto path = QUrl(url).path().toLower();
    if (path.endsWith(".m3u8") || path.endsWith(".m3u")) return "M3U8";
    if (path.endsWith(".mpd")) return "DASH";
    const auto declared = declaredType.trimmed().toUpper();
    if (declared == "M3U8" || declared == "DASH" || declared == "TORRENT") {
        return declared;
    }
    return "VIDEO";
}

bool playable(const QString &url, const QString &type) {
    if (type == "TORRENT") return false;
    static const QSet<QString> networkProtocols{
        "http", "https", "dav", "davs", "webdav", "webdavs",
        "ftp", "gopher", "gophers", "ipfs", "ipns",
        "mms", "mmsh", "mmshttp", "mmst",
        "rtmp", "rtmpe", "rtmps", "rtmpt", "rtmpte", "rtmpts",
        "rtp", "srtp", "rist", "srt", "rtsp", "rtsps",
        "udp", "udplite",
    };
    const QUrl parsed(url);
    const auto scheme = parsed.scheme().toLower();
    return parsed.isValid() && networkProtocols.contains(scheme);
}
} // namespace

QString PlaybackSource::hosterName() const {
    auto hoster = source.section(QChar(0x2022), 0, 0).trimmed();
    if (hoster.isEmpty()) hoster = QUrl(url).host();
    return hoster.isEmpty() ? QStringLiteral("Direct") : hoster;
}

QString PlaybackSource::displayLabel() const {
    QStringList facts{hosterName()};
    if (quality > 0) facts << QString::number(quality) + "p";
    const auto normalizedType = type.toUpper();
    if (normalizedType == "M3U8") facts << "HLS";
    else if (normalizedType == "DASH") facts << "DASH";
    else if (!normalizedType.isEmpty()) facts << normalizedType;
    if (!name.isEmpty() && name != source && !name.startsWith(hosterName())) facts << name;
    return facts.join("  •  ");
}

QMap<QString, QString> PlaybackSource::httpHeaders() const {
    auto result = headers;
    bool hasReferer = false;
    for (auto iterator = result.cbegin(); iterator != result.cend(); ++iterator) {
        if (iterator.key().compare("Referer", Qt::CaseInsensitive) == 0) {
            hasReferer = true;
            break;
        }
    }
    if (!referer.isEmpty() && !hasReferer) result.insert("Referer", referer);
    return result;
}

SourceDiscovery SourceCatalog::parse(const QByteArray &payload) {
    SourceDiscovery result;
    const auto document = QJsonDocument::fromJson(payload);
    if (document.isNull()) return result;
    QJsonArray links;
    QJsonArray subtitles;
    if (document.isArray()) {
        links = document.array();
        result.success = true;
    } else if (document.isObject()) {
        const auto object = document.object();
        links = object.value("links").toArray();
        subtitles = object.value("subtitles").toArray();
        result.success = object.value("success").toBool(true);
    } else {
        return result;
    }

    QSet<QString> sourceUrls;
    for (const auto &value : links) {
        const auto object = value.toObject();
        PlaybackSource source;
        source.source = object.value("source").toString();
        source.name = object.value("name").toString();
        source.url = object.value("url").toString();
        source.referer = object.value("referer").toString();
        source.quality = object.value("quality").toInt();
        source.type = normalizedType(source.url, object.value("type").toString());
        source.headers = stringMap(object.value("headers").toObject());
        QSet<QString> audioUrls;
        for (const auto &audioValue : object.value("audioTracks").toArray()) {
            const auto audioObject = audioValue.toObject();
            PlaybackAudioTrack audio;
            audio.url = audioObject.value("url").toString();
            audio.headers = stringMap(audioObject.value("headers").toObject());
            if (!playable(audio.url, "VIDEO") || audioUrls.contains(audio.url)) continue;
            audioUrls.insert(audio.url);
            source.audioTracks << audio;
        }
        bool playlistValid = !object.value("playlist").toArray().isEmpty();
        QStringList playlistIdentity;
        for (const auto &segmentValue : object.value("playlist").toArray()) {
            const auto segmentObject = segmentValue.toObject();
            PlaybackSegment segment;
            segment.url = segmentObject.value("url").toString();
            segment.durationUs = std::max<qint64>(
                0, segmentObject.value("durationUs").toVariant().toLongLong());
            if (!playable(segment.url, "VIDEO")) {
                playlistValid = false;
                source.playlist.clear();
                break;
            }
            source.playlist << segment;
            playlistIdentity << segment.url + "@" + QString::number(segment.durationUs);
        }
        const auto identity = source.playlist.isEmpty()
            ? source.url : QStringLiteral("playlist:") + playlistIdentity.join('|');
        const bool sourcePlayable = !source.playlist.isEmpty()
            ? playlistValid : playable(source.url, source.type);
        if (!sourcePlayable || sourceUrls.contains(identity)) continue;
        sourceUrls.insert(identity);
        result.sources << source;
    }
    std::sort(result.sources.begin(), result.sources.end(), [](const auto &left, const auto &right) {
        if (left.quality != right.quality) return left.quality > right.quality;
        return left.displayLabel().localeAwareCompare(right.displayLabel()) < 0;
    });

    QSet<QString> subtitleKeys;
    for (const auto &value : subtitles) {
        const auto object = value.toObject();
        PlaybackSubtitle subtitle;
        subtitle.language = object.value("language").toString(object.value("lang").toString());
        subtitle.url = object.value("url").toString();
        subtitle.headers = stringMap(object.value("headers").toObject());
        const auto parsed = QUrl(subtitle.url);
        if (!parsed.isValid() || (parsed.scheme() != "http" && parsed.scheme() != "https")) continue;
        const auto key = subtitle.language + "\n" + subtitle.url;
        if (subtitleKeys.contains(key)) continue;
        subtitleKeys.insert(key);
        result.subtitles << subtitle;
    }
    return result;
}

} // namespace CloudStream
