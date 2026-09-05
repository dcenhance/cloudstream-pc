#include "../player/PlayerCommand.h"

#include <QTemporaryFile>
#include <QtTest>

class PlayerCommandTest : public QObject {
    Q_OBJECT

private slots:
    void acceptsSupportedNetworkTargets() {
        QVERIFY(CloudStream::PlayerCommand::isValidTarget("https://example.org/video.m3u8"));
        QVERIFY(CloudStream::PlayerCommand::isValidTarget("rtsp://example.org/live"));
        QVERIFY(CloudStream::PlayerCommand::isValidTarget("magnet:?xt=urn:btih:abc"));
    }

    void acceptsExistingLocalFile() {
        QTemporaryFile file;
        QVERIFY(file.open());
        QVERIFY(CloudStream::PlayerCommand::isValidTarget(file.fileName()));
    }

    void rejectsInvalidTargets() {
        QVERIFY(!CloudStream::PlayerCommand::isValidTarget(""));
        QVERIFY(!CloudStream::PlayerCommand::isValidTarget("javascript:alert(1)"));
        QVERIFY(!CloudStream::PlayerCommand::isValidTarget("/definitely/missing/video.mkv"));
    }

    void buildsResumeAndIpcArguments() {
        const auto args = CloudStream::PlayerCommand::arguments(
            "https://example.org/video.m3u8", "/tmp/cloudstream.sock",
            "/tmp/watch-later", "/tmp/subtitle.srt");
        QVERIFY(args.contains("--input-ipc-server=/tmp/cloudstream.sock"));
        QVERIFY(args.contains("--save-position-on-quit=yes"));
        QVERIFY(args.contains("--resume-playback=yes"));
        QVERIFY(args.contains("--watch-later-directory=/tmp/watch-later"));
        QVERIFY(args.contains("--sub-file=/tmp/subtitle.srt"));
        QCOMPARE(args.last(), QString("https://example.org/video.m3u8"));
    }

    void addsExplicitResumePositionWhenKnown() {
        const auto resumed = CloudStream::PlayerCommand::arguments(
            "https://example.org/video.m3u8", "/tmp/cloudstream.sock",
            "/tmp/watch-later", {}, 123.5);
        QVERIFY(resumed.contains("--start=123.5"));

        const auto fresh = CloudStream::PlayerCommand::arguments(
            "https://example.org/video.m3u8", "/tmp/cloudstream.sock",
            "/tmp/watch-later", {}, 0.0);
        for (const auto &argument : fresh) QVERIFY(!argument.startsWith("--start="));
    }
};

QTEST_APPLESS_MAIN(PlayerCommandTest)
#include "test_player_command.moc"
