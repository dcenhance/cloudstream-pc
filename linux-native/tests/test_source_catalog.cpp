#include "../player/SourceCatalog.h"

#include <QtTest>

#include <algorithm>

class SourceCatalogTest : public QObject {
    Q_OBJECT

private slots:
    void parsesDeduplicatesAndSortsPlayableSources() {
        const QByteArray payload = QByteArrayLiteral(
            "{\"success\":true,\"links\":["
            "{\"source\":\"VOE • German Sub\",\"name\":\"VOE 720p\",\"url\":\"https://cdn.example/720.m3u8\",\"referer\":\"https://voe.sx/e/1\",\"quality\":720,\"type\":\"M3U8\",\"headers\":{\"Origin\":\"https://voe.sx\"}},"
            "{\"source\":\"VOE • German Sub\",\"name\":\"VOE 1080p\",\"url\":\"https://cdn.example/1080.m3u8\",\"referer\":\"https://voe.sx/e/1\",\"quality\":1080,\"type\":\"M3U8\",\"headers\":{}},"
            "{\"source\":\"Duplicate\",\"name\":\"Duplicate\",\"url\":\"https://cdn.example/1080.m3u8\",\"referer\":\"\",\"quality\":1080,\"type\":\"M3U8\",\"headers\":{}},"
            "{\"source\":\"Torrent\",\"name\":\"Torrent\",\"url\":\"magnet:?xt=test\",\"referer\":\"\",\"quality\":2160,\"type\":\"MAGNET\",\"headers\":{}}],"
            "\"subtitles\":[{\"language\":\"German\",\"url\":\"https://cdn.example/de.vtt\",\"headers\":{\"Authorization\":\"token\"}},"
            "{\"language\":\"German\",\"url\":\"https://cdn.example/de.vtt\",\"headers\":{}}]}");

        const auto catalog = CloudStream::SourceCatalog::parse(payload);

        QVERIFY(catalog.success);
        QCOMPARE(catalog.sources.size(), 2);
        QCOMPARE(catalog.sources[0].quality, 1080);
        QCOMPARE(catalog.sources[0].hosterName(), QString("VOE"));
        QCOMPARE(catalog.sources[0].httpHeaders().value("Referer"), QString("https://voe.sx/e/1"));
        QCOMPARE(catalog.sources[1].httpHeaders().value("Origin"), QString("https://voe.sx"));
        QCOMPARE(catalog.subtitles.size(), 1);
        QCOMPARE(catalog.subtitles[0].language, QString("German"));
        QCOMPARE(catalog.subtitles[0].headers.value("Authorization"), QString("token"));
    }

    void acceptsLegacyLinkArrays() {
        const auto catalog = CloudStream::SourceCatalog::parse(
            QByteArrayLiteral("[{\"source\":\"Direct\",\"name\":\"Direct\",\"url\":\"https://example.org/video.mp4\",\"quality\":480,\"type\":\"VIDEO\",\"headers\":{}}]"));
        QVERIFY(catalog.success);
        QCOMPARE(catalog.sources.size(), 1);
        QVERIFY(catalog.subtitles.isEmpty());
    }

    void acceptsLibmpvNetworkProtocolsAndInfersManifestTypes() {
        const auto catalog = CloudStream::SourceCatalog::parse(QByteArrayLiteral(
            "{\"success\":true,\"links\":["
            "{\"source\":\"Secure RTMP\",\"url\":\"rtmps://media.example/live\",\"type\":\"VIDEO\"},"
            "{\"source\":\"SRT\",\"url\":\"srt://media.example:9000?streamid=test\",\"type\":\"VIDEO\"},"
            "{\"source\":\"HLS mislabeled\",\"url\":\"https://media.example/master.m3u8?token=1\",\"type\":\"VIDEO\"},"
            "{\"source\":\"DASH inferred\",\"url\":\"https://media.example/manifest.mpd\",\"type\":\"UNKNOWN\"},"
            "{\"source\":\"Unsafe local\",\"url\":\"file:///etc/passwd\",\"type\":\"VIDEO\"},"
            "{\"source\":\"Torrent\",\"url\":\"magnet:?xt=urn:test\",\"type\":\"TORRENT\"}]}"));

        QVERIFY(catalog.success);
        QCOMPARE(catalog.sources.size(), 4);
        const auto hls = std::find_if(catalog.sources.cbegin(), catalog.sources.cend(),
                                      [](const auto &source) { return source.url.contains(".m3u8"); });
        const auto dash = std::find_if(catalog.sources.cbegin(), catalog.sources.cend(),
                                       [](const auto &source) { return source.url.contains(".mpd"); });
        QVERIFY(hls != catalog.sources.cend());
        QVERIFY(dash != catalog.sources.cend());
        QCOMPARE(hls->type, QString("M3U8"));
        QCOMPARE(dash->type, QString("DASH"));
        QVERIFY(std::any_of(catalog.sources.cbegin(), catalog.sources.cend(),
                            [](const auto &source) { return source.url.startsWith("rtmps://"); }));
        QVERIFY(std::any_of(catalog.sources.cbegin(), catalog.sources.cend(),
                            [](const auto &source) { return source.url.startsWith("srt://"); }));
    }

    void preservesSeparateAudioTracksAndTheirHeaders() {
        const auto catalog = CloudStream::SourceCatalog::parse(QByteArrayLiteral(
            "{\"success\":true,\"links\":[{"
            "\"source\":\"DASH\",\"url\":\"https://media.example/video.mpd\",\"type\":\"DASH\","
            "\"audioTracks\":[{\"url\":\"https://media.example/audio-en.m4a\","
            "\"headers\":{\"Authorization\":\"Bearer value\"}},{"
            "\"url\":\"https://media.example/audio-de.m4a\",\"headers\":{}}]}]}"));

        QCOMPARE(catalog.sources.size(), 1);
        QCOMPARE(catalog.sources.first().audioTracks.size(), 2);
        QCOMPARE(catalog.sources.first().audioTracks.first().url,
                 QString("https://media.example/audio-en.m4a"));
        QCOMPARE(catalog.sources.first().audioTracks.first().headers.value("Authorization"),
                 QString("Bearer value"));
    }

    void preservesConcatenatedPlaylistSegmentsWithDurations() {
        const auto payload = QByteArrayLiteral(
            "{\"success\":true,\"links\":[{"
            "\"source\":\"Segment host\",\"name\":\"Segmented video\",\"url\":\"\","
            "\"referer\":\"https://provider.example/\",\"quality\":720,\"type\":\"VIDEO\","
            "\"headers\":{\"Origin\":\"https://provider.example\"},\"playlist\":["
            "{\"url\":\"https://cdn.example/part-1.mp4\",\"durationUs\":2000000},"
            "{\"url\":\"https://cdn.example/part-2.mp4\",\"durationUs\":3500000}]}]}");

        const auto catalog = CloudStream::SourceCatalog::parse(payload);

        QCOMPARE(catalog.sources.size(), 1);
        QCOMPARE(catalog.sources[0].playlist.size(), 2);
        QCOMPARE(catalog.sources[0].playlist[0].url,
                 QString("https://cdn.example/part-1.mp4"));
        QCOMPARE(catalog.sources[0].playlist[1].durationUs, qint64(3500000));
    }
};

QTEST_APPLESS_MAIN(SourceCatalogTest)
#include "test_source_catalog.moc"
