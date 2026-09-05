#include <QtTest>

#include "../downloads/DownloadManager.h"
#include "../downloads/DownloadQueueStore.h"
#include "../downloads/ProcessSuspension.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>

class RangeServer final : public QTcpServer {
    Q_OBJECT
public:
    QByteArray payload;
    QByteArray lastRange;

    explicit RangeServer(QByteArray body, QObject *parent = nullptr)
        : QTcpServer(parent), payload(std::move(body)) {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    const auto request = socket->readAll();
                    if (!request.contains("\r\n\r\n")) return;
                    const auto lines = request.split('\n');
                    qint64 offset = 0;
                    for (auto line : lines) {
                        line = line.trimmed();
                        if (!line.toLower().startsWith("range:")) continue;
                        lastRange = line.mid(line.indexOf(':') + 1).trimmed();
                        const auto value = lastRange;
                        if (value.startsWith("bytes=") && value.endsWith('-')) {
                            offset = value.mid(6, value.size() - 7).toLongLong();
                        }
                    }
                    const bool partial = offset > 0 && offset < payload.size();
                    const auto body = partial ? payload.mid(offset) : payload;
                    QByteArray response = partial ? "HTTP/1.1 206 Partial Content\r\n"
                                                  : "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: video/mp4\r\n";
                    response += "Accept-Ranges: bytes\r\n";
                    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                    if (partial) {
                        response += "Content-Range: bytes " + QByteArray::number(offset) + "-" +
                                    QByteArray::number(payload.size() - 1) + "/" +
                                    QByteArray::number(payload.size()) + "\r\n";
                    }
                    response += "Connection: close\r\n\r\n";
                    socket->write(response);
                    socket->write(body);
                    socket->disconnectFromHost();
                });
            }
        });
    }

    QUrl url() const {
        return QUrl("http://127.0.0.1:" + QString::number(serverPort()) + "/episode.mp4");
    }
};

class StaticMediaServer final : public QTcpServer {
    Q_OBJECT
public:
    explicit StaticMediaServer(QString root, int responseDelayMs = 0, QObject *parent = nullptr)
        : QTcpServer(parent), root_(std::move(root)), responseDelayMs_(responseDelayMs) {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                    const auto request = socket->readAll();
                    if (!request.contains("\r\n\r\n")) return;
                    const auto target = request.split('\n').constFirst().split(' ').value(1);
                    const auto relative = QUrl::fromPercentEncoding(target).section('?', 0, 0);
                    const auto path = QDir(root_).filePath(relative.startsWith('/')
                        ? relative.mid(1) : relative);
                    QFile file(path);
                    if (!QFileInfo(path).canonicalFilePath().startsWith(
                            QFileInfo(root_).canonicalFilePath()) ||
                        !file.open(QIODevice::ReadOnly)) {
                        socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    const auto payload = file.readAll();
                    const auto contentType = path.endsWith(".m3u8")
                        ? QByteArray("application/vnd.apple.mpegurl")
                        : path.endsWith(".mpd") ? QByteArray("application/dash+xml")
                        : path.endsWith(".m4s") ? QByteArray("video/iso.segment")
                        : QByteArray("video/mp2t");
                    const auto response = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
                        "\r\nContent-Length: " + QByteArray::number(payload.size()) +
                        "\r\nConnection: close\r\n\r\n" + payload;
                    const auto send = [socket, response] {
                        socket->write(response);
                        socket->disconnectFromHost();
                    };
                    if (responseDelayMs_ > 0) QTimer::singleShot(responseDelayMs_, socket, send);
                    else send();
                });
            }
        });
    }

    QUrl url(const QString &name) const {
        return QUrl("http://127.0.0.1:" + QString::number(serverPort()) + "/" + name);
    }

private:
    QString root_;
    int responseDelayMs_ = 0;
};

class DownloadManagerTest final : public QObject {
    Q_OBJECT
private slots:
    void processSuspensionRejectsNonRunningProcesses() {
        QProcess process;
        QString error;
        QVERIFY(!CloudStream::ProcessSuspension::setSuspended(&process, true, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!CloudStream::ProcessSuspension::setSuspended(&process, false, &error));
        QVERIFY(!error.isEmpty());
    }

    void processSuspensionStopsAndRestartsRealWork() {
        QProcess process;
        process.start("ffmpeg", {"-hide_banner", "-loglevel", "error", "-re",
            "-f", "lavfi", "-i", "testsrc=size=16x16:rate=20", "-t", "30",
            "-stats_period", "0.05", "-progress", "pipe:1", "-f", "null", "-"});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForReadyRead(5000));
        const auto pid = process.processId();
        QString error;
        QVERIFY2(CloudStream::ProcessSuspension::setSuspended(&process, true, &error), qPrintable(error));
        QTest::qWait(200); // Drain progress written just before the OS suspended it.
        process.readAllStandardOutput();
        QTest::qWait(350);
        QCOMPARE(process.readAllStandardOutput(), QByteArray{});
        QCOMPARE(process.state(), QProcess::Running);
        QVERIFY2(CloudStream::ProcessSuspension::setSuspended(&process, false, &error), qPrintable(error));
        QCOMPARE(process.processId(), pid);
        QVERIFY(process.waitForReadyRead(5000));
        QVERIFY(!process.readAllStandardOutput().isEmpty());
        process.kill();
        QVERIFY(process.waitForFinished());
    }

    void persistsQueueAndNormalizesInterruptedState() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath("download-queue.json");
        {
            CloudStream::DownloadQueueStore store(path);
            CloudStream::DownloadEntry entry;
            entry.id = "entry-1";
            entry.title = "Episode 1";
            entry.url = "https://cdn.example/episode.mp4";
            entry.targetPath = temporary.filePath("Episode 1.mp4");
            entry.artifactPath = "/providers/example.jar";
            entry.provider = "ExampleProvider";
            entry.playbackData = "episode-token";
            entry.sourceName = "Fast host • 1080p";
            entry.sourceHoster = "Fast host";
            entry.sourceQuality = 1080;
            entry.headers.insert("Referer", "https://provider.example/");
            entry.state = CloudStream::DownloadState::Downloading;
            entry.bytesReceived = 128;
            entry.bytesTotal = 1024;
            QVERIFY(store.upsert(entry));
#ifdef Q_OS_UNIX
            QCOMPARE(QFileInfo(path).permissions() &
                         (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                          QFileDevice::ReadOther | QFileDevice::WriteOther),
                     QFileDevice::Permissions{});
#endif
        }

        CloudStream::DownloadQueueStore reloaded(path);
        QCOMPARE(reloaded.entries().size(), 1);
        const auto restored = reloaded.entries().first();
        QCOMPARE(restored.title, QString("Episode 1"));
        QCOMPARE(restored.headers.value("Referer"), QString("https://provider.example/"));
        QCOMPARE(restored.artifactPath, QString("/providers/example.jar"));
        QCOMPARE(restored.provider, QString("ExampleProvider"));
        QCOMPARE(restored.playbackData, QString("episode-token"));
        QCOMPARE(restored.sourceName, QString("Fast host • 1080p"));
        QCOMPARE(restored.sourceHoster, QString("Fast host"));
        QCOMPARE(restored.sourceQuality, 1080);
        QCOMPARE(restored.state, CloudStream::DownloadState::Paused);
        QCOMPARE(restored.bytesReceived, qint64(128));
        QCOMPARE(restored.bytesTotal, qint64(1024));
    }

    void resumesDirectDownloadWithRangeAndCompletesAtomically() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        RangeServer server(QByteArray(256 * 1024, 'x'));
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource source;
        source.url = server.url().toString();
        source.type = "VIDEO";
        source.source = "Local test";
        source.referer = "https://provider.example/";
        CloudStream::DownloadOrigin origin;
        origin.artifactPath = "/providers/test.jar";
        origin.provider = "TestProvider";
        origin.playbackData = "episode-1";
        QString error;
        const auto id = manager.enqueue("Series • Episode 1", source, temporary.path(), &error, origin);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        auto entry = store.entry(id);
        QVERIFY(entry.has_value());
        QCOMPARE(entry->artifactPath, origin.artifactPath);
        QCOMPARE(entry->provider, origin.provider);
        QCOMPARE(entry->playbackData, origin.playbackData);
        QCOMPARE(entry->sourceHoster, QString("Local test"));
        QFile part(entry->partPath());
        QVERIFY(part.open(QIODevice::WriteOnly));
        QCOMPARE(part.write(server.payload.first(32768)), qint64(32768));
        part.close();

        manager.start();
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state, CloudStream::DownloadState::Completed, 5000);
        QCOMPARE(server.lastRange, QByteArray("bytes=32768-"));
        QFile completed(store.entry(id)->targetPath);
        QVERIFY(completed.open(QIODevice::ReadOnly));
        QCOMPARE(completed.readAll(), server.payload);
        QVERIFY(!QFileInfo::exists(store.entry(id)->partPath()));
        QCOMPARE(store.entry(id)->bytesReceived, qint64(server.payload.size()));
        QCOMPARE(store.entry(id)->bytesTotal, qint64(server.payload.size()));
    }

    void downloadsHlsVodToAPlayableMatroskaFile() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto manifest = temporary.filePath("master.m3u8");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-t", "2", "-pix_fmt", "yuv420p", "-c:v", "libx264", "-c:a", "aac",
            "-f", "hls", "-hls_time", "0.5", "-hls_playlist_type", "vod",
            "-hls_segment_filename", temporary.filePath("segment%03d.ts"), manifest,
        }), 0);
        StaticMediaServer server(temporary.path());
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource source;
        source.url = server.url("master.m3u8").toString();
        source.type = "M3U8";
        source.source = "Generated HLS";
        QString error;
        const auto id = manager.enqueue("HLS episode", source, temporary.path(), &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        QVERIFY(store.entry(id)->targetPath.endsWith(".mkv"));

        manager.start();
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state,
                                  CloudStream::DownloadState::Completed, 15000);
        QVERIFY(QFileInfo(store.entry(id)->targetPath).size() > 1024);
        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-select_streams", "v:0",
                                "-show_entries", "stream=codec_type", "-of", "csv=p=0",
                                store.entry(id)->targetPath});
        QVERIFY(probe.waitForFinished(5000));
        QCOMPARE(probe.exitCode(), 0);
        QVERIFY(QString::fromUtf8(probe.readAllStandardOutput()).contains("video"));
    }

    void pausesAndResumesAnActiveHlsDownload() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto manifest = temporary.filePath("pause.m3u8");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=160x90:rate=12",
            "-t", "6", "-pix_fmt", "yuv420p", "-c:v", "libx264",
            "-f", "hls", "-hls_time", "0.5", "-hls_playlist_type", "vod",
            "-hls_segment_filename", temporary.filePath("pause%03d.ts"), manifest,
        }), 0);
        StaticMediaServer server(temporary.path(), 120);
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CloudStream::DownloadQueueStore store(temporary.filePath("pause-queue.json"));
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource source;
        source.url = server.url("pause.m3u8").toString();
        source.type = "M3U8";
        QString error;
        const auto id = manager.enqueue("Pause HLS", source, temporary.path(), &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        manager.start();
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state,
                                  CloudStream::DownloadState::Downloading, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(manager.findChild<QProcess *>() &&
            manager.findChild<QProcess *>()->state() == QProcess::Running, 3000);
        const auto pid = manager.findChild<QProcess *>()->processId();
        QVERIFY(manager.pause(id));
        QVERIFY(!manager.pause(id));
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state,
                                  CloudStream::DownloadState::Paused, 3000);
        QTest::qWait(500);
        QCOMPARE(store.entry(id)->state, CloudStream::DownloadState::Paused);
        QVERIFY(manager.resume(id));
        QCOMPARE(manager.findChild<QProcess *>()->processId(), pid);
        QVERIFY(!manager.resume(id));
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state,
                                  CloudStream::DownloadState::Completed, 15000);
        QVERIFY(QFileInfo(store.entry(id)->targetPath).size() > 1024);
    }

    void cleansUpSuspendedAdaptiveProcess_data() {
        QTest::addColumn<bool>("remove");
        QTest::newRow("remove") << true;
        QTest::newRow("shutdown") << false;
    }

    void cleansUpSuspendedAdaptiveProcess() {
        QFETCH(bool, remove);
        QTemporaryDir temporary;
        QTcpServer server; // Accept the manifest request but leave FFmpeg waiting.
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        auto manager = std::make_unique<CloudStream::DownloadManager>(&store);
        CloudStream::PlaybackSource source;
        source.url = "http://127.0.0.1:" + QString::number(server.serverPort()) + "/wait.m3u8";
        source.type = "M3U8";
        const auto id = manager->enqueue("Waiting HLS", source, temporary.path());
        QVERIFY(!id.isEmpty());
        manager->start();
        QTRY_VERIFY(server.hasPendingConnections());
        auto *process = manager->findChild<QProcess *>();
        QVERIFY(process);
        QSignalSpy finished(process, &QProcess::finished);
        QVERIFY(manager->pause(id));
        QVERIFY(!manager->pause(id)); // Never increment a Windows suspend count twice.
        QCOMPARE(store.entry(id)->state, CloudStream::DownloadState::Paused);
        if (remove) {
            QVERIFY(manager->remove(id, true));
            QTRY_VERIFY_WITH_TIMEOUT(!store.entry(id).has_value(), 5000);
        } else {
            manager.reset();
            QCOMPARE(store.entry(id)->state, CloudStream::DownloadState::Paused);
        }
        QCOMPARE(finished.count(), 1);
    }

    void downloadsDashVodWithSeparateAdaptationSets() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto manifest = temporary.filePath("stream.mpd");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24",
            "-f", "lavfi", "-i", "sine=frequency=660:sample_rate=48000",
            "-t", "2", "-pix_fmt", "yuv420p", "-c:v", "libx264", "-c:a", "aac",
            "-f", "dash", "-seg_duration", "0.5", manifest,
        }), 0);
        StaticMediaServer server(temporary.path());
        QVERIFY(server.listen(QHostAddress::LocalHost));
        CloudStream::DownloadQueueStore store(temporary.filePath("dash-queue.json"));
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource source;
        source.url = server.url("stream.mpd").toString();
        source.type = "DASH";
        QString error;
        const auto id = manager.enqueue("DASH episode", source, temporary.path(), &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        manager.start();
        QTRY_COMPARE_WITH_TIMEOUT(store.entry(id)->state,
                                  CloudStream::DownloadState::Completed, 15000);
        QProcess probe;
        probe.start("ffprobe", {"-v", "error", "-show_entries", "stream=codec_type",
                                "-of", "csv=p=0", store.entry(id)->targetPath});
        QVERIFY(probe.waitForFinished(5000));
        QCOMPARE(probe.exitCode(), 0);
        const auto streams = QString::fromUtf8(probe.readAllStandardOutput());
        QVERIFY(streams.contains("video"));
        QVERIFY(streams.contains("audio"));
    }

    void cancellationRemovesPartialFileAndQueueRecord() {
        QTemporaryDir temporary;
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource source;
        source.url = "https://cdn.example/episode.mp4";
        source.type = "VIDEO";
        QString error;
        const auto id = manager.enqueue("Episode", source, temporary.path(), &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const auto partial = store.entry(id)->partPath();
        QFile file(partial);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("partial");
        file.close();
        QVERIFY(manager.remove(id, true));
        QVERIFY(!store.entry(id).has_value());
        QVERIFY(!QFileInfo::exists(partial));
    }

    void sanitizesTitlesAndAvoidsOverwritingExistingFiles() {
        QTemporaryDir temporary;
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadManager manager(&store);
        QFile existing(temporary.filePath("Series Episode 1.mp4"));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("existing");
        existing.close();
        CloudStream::PlaybackSource source;
        source.url = "https://cdn.example/episode.mp4";
        source.type = "VIDEO";
        QString error;
        const auto id = manager.enqueue("Series / Episode 1", source, temporary.path(), &error);
        QVERIFY2(!id.isEmpty(), qPrintable(error));
        const auto target = store.entry(id)->targetPath;
        QVERIFY(target.endsWith("Series Episode 1 (2).mp4"));
        QVERIFY(QFileInfo::exists(existing.fileName()));
    }

    void queuesCompletedEntryAgainWhenItsFileIsMissing() {
        QTemporaryDir temporary;
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadEntry entry;
        entry.id = "missing-completed";
        entry.title = "Deleted episode";
        entry.url = "https://cdn.example/deleted.mp4";
        entry.targetPath = temporary.filePath("Deleted episode.mp4");
        entry.headers.insert("Referer", "https://provider.example/");
        entry.state = CloudStream::DownloadState::Completed;
        entry.bytesReceived = 1024;
        entry.bytesTotal = 1024;
        QVERIFY(store.upsert(entry));
        CloudStream::DownloadManager manager(&store);

        QVERIFY(manager.resume(entry.id));
        const auto queued = store.entry(entry.id);
        QVERIFY(queued.has_value());
        QCOMPARE(queued->state, CloudStream::DownloadState::Queued);
        QCOMPARE(queued->bytesReceived, qint64(0));
        QCOMPARE(queued->bytesTotal, qint64(0));
        QCOMPARE(queued->headers.value("Referer"), QString("https://provider.example/"));
    }

    void refreshesExpiringSourceWithoutLosingPartialDownload() {
        QTemporaryDir temporary;
        CloudStream::DownloadQueueStore store(temporary.filePath("queue.json"));
        CloudStream::DownloadEntry entry;
        entry.id = "refresh-source";
        entry.title = "Episode";
        entry.url = "https://old.example/expired.mp4";
        entry.targetPath = temporary.filePath("Episode.mp4");
        entry.artifactPath = "/providers/example.jar";
        entry.provider = "ExampleProvider";
        entry.playbackData = "stable-episode-data";
        entry.sourceName = "Host • 720p";
        entry.sourceHoster = "Host";
        entry.sourceQuality = 720;
        entry.state = CloudStream::DownloadState::Paused;
        entry.bytesReceived = 4096;
        entry.bytesTotal = 16384;
        QVERIFY(store.upsert(entry));
        QFile partial(entry.partPath());
        QVERIFY(partial.open(QIODevice::WriteOnly));
        partial.write(QByteArray(4096, 'x'));
        partial.close();
        CloudStream::DownloadManager manager(&store);
        CloudStream::PlaybackSource replacement;
        replacement.url = "https://new.example/fresh.mp4";
        replacement.type = "VIDEO";
        replacement.name = "Host • 720p";
        replacement.source = "Host • 720p";
        replacement.quality = 720;
        replacement.referer = "https://new.example/watch";

        QVERIFY(manager.updateSource(entry.id, replacement));
        const auto refreshed = store.entry(entry.id);
        QCOMPARE(refreshed->url, replacement.url);
        QCOMPARE(refreshed->headers.value("Referer"), replacement.referer);
        QCOMPARE(refreshed->sourceQuality, 720);
        QCOMPARE(refreshed->bytesReceived, qint64(4096));
        QCOMPARE(refreshed->targetPath, entry.targetPath);
        QCOMPARE(QFileInfo(refreshed->partPath()).size(), qint64(4096));
        QCOMPARE(refreshed->playbackData, QString("stable-episode-data"));
    }
};

QTEST_GUILESS_MAIN(DownloadManagerTest)
#include "test_download_manager.moc"
