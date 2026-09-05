#include "../player/MpvPlayerWidget.h"
#include "../player/IntegratedPlayerWindow.h"

#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QSignalSpy>
#include <QSlider>
#include <QStackedLayout>
#include <QTemporaryDir>
#include <QVBoxLayout>
#include <QtTest>
#include <algorithm>

class MpvPlayerWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void rendersAndAdvancesGeneratedVideo() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("test.mp4");
        const auto subtitlePath = directory.filePath("test.srt");
        QFile subtitleFile(subtitlePath);
        QVERIFY(subtitleFile.open(QIODevice::WriteOnly));
        subtitleFile.write("1\n00:00:00,000 --> 00:00:03,000\nCloudStream subtitle\n");
        subtitleFile.close();
        const QStringList ffmpegArguments{
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=640x360:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=48000",
            "-t", "4", "-shortest", "-pix_fmt", "yuv420p",
            "-map", "0:v", "-map", "1:a", "-map", "2:a",
            "-metadata:s:a:0", "language=eng", "-metadata:s:a:1", "language=deu",
            "-c:v", "libx264", "-c:a", "aac", videoPath,
        };
        QCOMPARE(QProcess::execute("ffmpeg", ffmpegArguments), 0);

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        CloudStream::MpvPlayerWidget player;
        layout->addWidget(&player);
        window.resize(640, 360);
        window.show();
        QVERIFY(player.isAvailable());

        CloudStream::PlaybackSource source;
        source.source = "Local test";
        source.name = "Generated video";
        source.url = videoPath;
        source.type = "VIDEO";
        QSignalSpy loaded(&player, &CloudStream::MpvPlayerWidget::fileLoaded);
        CloudStream::PlaybackSubtitle subtitle;
        subtitle.language = "English test";
        subtitle.url = subtitlePath;
        player.loadSource(source, {subtitle});

        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(player.currentVideoOutput(), QString("libmpv"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(([&player] {
            const auto tracks = player.tracks();
            return std::count_if(tracks.cbegin(), tracks.cend(),
                [](const CloudStream::MpvTrack &track) { return track.type == "audio"; }) == 2;
        }()), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(([&player] {
            const auto currentTracks = player.tracks();
            return std::any_of(currentTracks.cbegin(), currentTracks.cend(),
                [](const CloudStream::MpvTrack &track) { return track.type == "sub"; });
        }()), 5000);
        const auto tracks = player.tracks();
        const auto secondAudio = std::find_if(tracks.crbegin(), tracks.crend(),
            [](const CloudStream::MpvTrack &track) { return track.type == "audio"; });
        QVERIFY(secondAudio != tracks.crend());
        player.setAudioTrack(secondAudio->id);
        QTRY_VERIFY_WITH_TIMEOUT(([&player, id = secondAudio->id] {
            const auto currentTracks = player.tracks();
            return std::any_of(currentTracks.cbegin(), currentTracks.cend(),
                [id](const CloudStream::MpvTrack &track) { return track.type == "audio" && track.id == id && track.selected; });
        }()), 2000);
        const auto updatedTracks = player.tracks();
        const auto subtitleTrack = std::find_if(updatedTracks.cbegin(), updatedTracks.cend(),
            [](const CloudStream::MpvTrack &track) { return track.type == "sub"; });
        QVERIFY(subtitleTrack != updatedTracks.cend());
        player.setSubtitleTrack(subtitleTrack->id);
        QTRY_VERIFY_WITH_TIMEOUT(([&player, id = subtitleTrack->id] {
            const auto currentTracks = player.tracks();
            return std::any_of(currentTracks.cbegin(), currentTracks.cend(),
                [id](const CloudStream::MpvTrack &track) { return track.type == "sub" && track.id == id && track.selected; });
        }()), 2000);
        player.setSubtitleTrack(-1);
        QTRY_VERIFY_WITH_TIMEOUT(([&player] {
            const auto currentTracks = player.tracks();
            return std::none_of(currentTracks.cbegin(), currentTracks.cend(),
                [](const CloudStream::MpvTrack &track) { return track.type == "sub" && track.selected; });
        }()), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 0.5, 5000);
        player.setPaused(true);
        QTRY_VERIFY_WITH_TIMEOUT(player.isPaused(), 2000);
        player.seekTo(2.0);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 1.5, 2000);
        player.setPaused(false);
        player.setPlaybackSpeed(1.25);
        QCOMPARE(player.playbackSpeed(), 1.25);
        player.setFillMode(true);
        QVERIFY(player.fillMode());
        player.setFillMode(false);
        QVERIFY(!player.fillMode());
        QTRY_VERIFY_WITH_TIMEOUT(player.renderedFrameCount() > 0, 5000);
        const auto frame = player.grabFramebuffer();
        QSet<QRgb> colors;
        for (int y = 0; y < frame.height(); y += 12) {
            for (int x = 0; x < frame.width(); x += 12) colors.insert(frame.pixel(x, y));
        }
        QVERIFY2(colors.size() >= 8, "libmpv framebuffer stayed blank");
    }

    void loadsSeparateAudioRenditionWithVideoOnlySource() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("video-only.mp4");
        const auto audioPath = directory.filePath("audio-only.m4a");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=320x180:rate=24",
            "-t", "3", "-an", "-pix_fmt", "yuv420p", "-c:v", "libx264",
            videoPath,
        }), 0);
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-t", "3", "-vn", "-c:a", "aac", audioPath,
        }), 0);

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        CloudStream::MpvPlayerWidget player;
        layout->addWidget(&player);
        window.resize(480, 270);
        window.show();
        CloudStream::PlaybackSource source;
        source.source = "Separate rendition";
        source.url = videoPath;
        source.type = "DASH";
        source.audioTracks << CloudStream::PlaybackAudioTrack{audioPath, {}};
        QSignalSpy loaded(&player, &CloudStream::MpvPlayerWidget::fileLoaded);

        player.loadSource(source);

        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(
            player.tracks().cbegin(), player.tracks().cend(),
            [](const CloudStream::MpvTrack &track) {
                return track.type == "audio" && track.external;
            }), 5000);
    }

    void playsConcatenatedSegmentsOnOneTimeline() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto firstPath = directory.filePath("first.mp4");
        const auto secondPath = directory.filePath("second.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "color=c=red:size=320x180:rate=24",
            "-t", "1.2", "-pix_fmt", "yuv420p", "-c:v", "libx264", firstPath,
        }), 0);
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "color=c=blue:size=320x180:rate=24",
            "-t", "1.5", "-pix_fmt", "yuv420p", "-c:v", "libx264", secondPath,
        }), 0);

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        CloudStream::MpvPlayerWidget player;
        layout->addWidget(&player);
        window.resize(480, 270);
        window.show();
        CloudStream::PlaybackSource source;
        source.source = "Concatenated source";
        source.type = "VIDEO";
        source.playlist = {
            {firstPath, 1'200'000},
            {secondPath, 1'500'000},
        };
        QSignalSpy loaded(&player, &CloudStream::MpvPlayerWidget::fileLoaded);

        player.loadSource(source);

        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(player.duration() > 2.4, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(player.renderedFrameCount() > 0, 5000);
    }

    void integratedWindowExposesLiveTrackMenus() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("tracks.mp4");
        const auto subtitlePath = directory.filePath("tracks.srt");
        QFile subtitleFile(subtitlePath);
        QVERIFY(subtitleFile.open(QIODevice::WriteOnly));
        subtitleFile.write("1\n00:00:00,000 --> 00:00:03,000\nTrack picker subtitle\n");
        subtitleFile.close();
        const QStringList ffmpegArguments{
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc=size=640x360:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=48000",
            "-t", "4", "-shortest", "-pix_fmt", "yuv420p",
            "-map", "0:v", "-map", "1:a", "-map", "2:a",
            "-metadata:s:a:0", "language=eng", "-metadata:s:a:1", "language=deu",
            "-c:v", "libx264", "-c:a", "aac", videoPath,
        };
        QCOMPARE(QProcess::execute("ffmpeg", ffmpegArguments), 0);

        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        CloudStream::PlaybackSource source;
        source.source = "Local test";
        source.name = "Track menu fixture";
        source.url = videoPath;
        source.type = "VIDEO";
        discovery.sources << source;
        CloudStream::PlaybackSubtitle subtitle;
        subtitle.language = "English test";
        subtitle.url = subtitlePath;
        discovery.subtitles << subtitle;

        CloudStream::IntegratedPlayerWindow window(discovery, "Track menu test");
        window.show();
        auto *audio = window.findChild<QComboBox *>("audioTrackSelector");
        auto *subtitles = window.findChild<QComboBox *>("subtitleTrackSelector");
        auto *videoStack = window.findChild<QStackedLayout *>("playerVideoStack");
        auto *chrome = window.findChild<QWidget *>("playerChrome");
        QVERIFY(audio);
        QVERIFY(subtitles);
        QVERIFY(videoStack);
        QVERIFY(chrome);
        auto *surface = window.findChild<CloudStream::MpvPlayerWidget *>();
        QVERIFY(surface);
        QCOMPARE(chrome->parentWidget(), static_cast<QWidget *>(surface));
        QCOMPARE(videoStack->currentWidget(), static_cast<QWidget *>(surface));
        QVERIFY(chrome->testAttribute(Qt::WA_NoSystemBackground));
        QVERIFY(chrome->testAttribute(Qt::WA_TranslucentBackground));
        QVERIFY(!window.findChild<QWidget *>("playerCenterControls"));
        QTRY_COMPARE_WITH_TIMEOUT(audio->count(), 3, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(subtitles->count() >= 2, 5000);
        auto *sourcesAction = window.findChild<QPushButton *>("playerSources");
        auto *tracksAction = window.findChild<QPushButton *>("playerTracks");
        auto *speedAction = window.findChild<QPushButton *>("playerSpeed");
        auto *backAction = window.findChild<QPushButton *>("playerBack");
        auto *title = window.findChild<QLabel *>("playerTitle");
        auto *position = window.findChild<QLabel *>("playerPosition");
        auto *duration = window.findChild<QLabel *>("playerDuration");
        QVERIFY(sourcesAction);
        QVERIFY(tracksAction);
        QVERIFY(speedAction);
        QVERIFY(backAction);
        QVERIFY(title);
        QVERIFY(position);
        QVERIFY(duration);
        QVERIFY(backAction->geometry().left() < title->geometry().left());
        QVERIFY(position->text().contains(":"));
        QVERIFY(duration->text().contains(":"));
        QVERIFY(audio->isHidden());
        QVERIFY(subtitles->isHidden());

        QTest::mouseClick(sourcesAction, Qt::LeftButton);
        auto *sourceDialog = window.findChild<QDialog *>("playerSourceDialog");
        QVERIFY(sourceDialog);
        QTRY_VERIFY_WITH_TIMEOUT(sourceDialog->isVisible(), 500);
        auto *sourceList = sourceDialog->findChild<QListWidget *>("playerSourceList");
        QVERIFY(sourceList);
        QCOMPARE(sourceList->count(), 1);
        sourceDialog->reject();

        QTest::mouseClick(tracksAction, Qt::LeftButton);
        auto *trackDialog = window.findChild<QDialog *>("playerTrackDialog");
        QVERIFY(trackDialog);
        QTRY_VERIFY_WITH_TIMEOUT(trackDialog->isVisible(), 500);
        QVERIFY(trackDialog->findChild<QListWidget *>("playerAudioTracks"));
        QVERIFY(trackDialog->findChild<QListWidget *>("playerSubtitleTracks"));
        trackDialog->reject();

        QTest::mouseClick(speedAction, Qt::LeftButton);
        auto *speedDialog = window.findChild<QDialog *>("playerSpeedDialog");
        QVERIFY(speedDialog);
        QTRY_VERIFY_WITH_TIMEOUT(speedDialog->isVisible(), 500);
        speedDialog->reject();
    }

    void integratedWindowAppliesDesktopPlayerPreferences() {
        CloudStream::SourceDiscovery discovery;
        CloudStream::PlayerPreferences preferences;
        preferences.seekSeconds = 25;
        preferences.initialVolume = 35;
        preferences.showInformation = false;
        preferences.automaticFallback = false;
        preferences.selectFirstSubtitle = true;
        CloudStream::IntegratedPlayerWindow window(
            discovery, "Preference test", 0.0, nullptr, preferences);
        auto *volume = window.findChild<QSlider *>("playerVolume");
        auto *back = window.findChild<QPushButton *>("playerRewind");
        auto *forward = window.findChild<QPushButton *>("playerForward");
        auto *information = window.findChild<QWidget *>("playerTopBar");
        QVERIFY(volume);
        QVERIFY(back);
        QVERIFY(forward);
        QVERIFY(information);
        QCOMPARE(volume->value(), 35);
        QVERIFY(back->toolTip().contains("25"));
        QVERIFY(forward->toolTip().contains("25"));
        QVERIFY(information->isHidden());
    }

    void integratedWindowKeepsRendererAliveDuringSourceFallback() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("fallback.mp4");
        const QStringList ffmpegArguments{
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-t", "4", "-shortest", "-pix_fmt", "yuv420p",
            "-c:v", "libx264", "-c:a", "aac", videoPath,
        };
        QCOMPARE(QProcess::execute("ffmpeg", ffmpegArguments), 0);

        CloudStream::PlaybackSource broken;
        broken.source = "Broken host";
        broken.name = "Unavailable source";
        broken.url = directory.filePath("missing.mp4");
        broken.type = "VIDEO";
        CloudStream::PlaybackSource working;
        working.source = "Working host";
        working.name = "Fallback source";
        working.url = videoPath;
        working.type = "VIDEO";
        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        discovery.sources = {broken, working};

        CloudStream::IntegratedPlayerWindow window(discovery, "Fallback test");
        auto *surface = window.findChild<CloudStream::MpvPlayerWidget *>();
        auto *stack = window.findChild<QStackedLayout *>("playerVideoStack");
        QVERIFY(surface);
        QVERIFY(stack);
        bool rendererWasReplaced = false;
        connect(surface, &CloudStream::MpvPlayerWidget::loadingChanged, &window,
                [&rendererWasReplaced, stack, surface](bool loading) {
            if (loading && stack->currentWidget() != surface) rendererWasReplaced = true;
        });
        window.show();

        QTRY_COMPARE_WITH_TIMEOUT(window.currentSourceIndex(), 1, 8000);
        QTRY_VERIFY_WITH_TIMEOUT(surface->position() > 0.25, 5000);
        QVERIFY2(!rendererWasReplaced,
                 "The loading UI hid the QOpenGLWidget while switching sources");
    }

    void integratedWindowAutoHidesAndRestoresMobileChrome() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("chrome.mp4");
        const QStringList ffmpegArguments{
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30",
            "-t", "4", "-pix_fmt", "yuv420p", "-c:v", "libx264", videoPath,
        };
        QCOMPARE(QProcess::execute("ffmpeg", ffmpegArguments), 0);

        CloudStream::PlaybackSource source;
        source.source = "Local test";
        source.name = "Chrome fixture";
        source.url = videoPath;
        source.type = "VIDEO";
        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        discovery.sources = {source};
        CloudStream::PlayerPreferences preferences;
        preferences.autoHideDelayMs = 120;
        CloudStream::IntegratedPlayerWindow window(discovery, "Chrome test", 0.0,
                                                    nullptr, preferences);
        auto *surface = window.findChild<CloudStream::MpvPlayerWidget *>();
        auto *chrome = window.findChild<QWidget *>("playerChrome");
        auto *top = window.findChild<QWidget *>("playerTopBar");
        auto *bottom = window.findChild<QWidget *>("playerControls");
        auto *playPause = window.findChild<QPushButton *>("playerPlayPause");
        QVERIFY(surface);
        QVERIFY(chrome);
        QVERIFY(top);
        QVERIFY(bottom);
        QVERIFY(playPause);
        window.show();
        QSignalSpy loaded(surface, &CloudStream::MpvPlayerWidget::fileLoaded);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(bottom->isHidden(), 1500);
        QVERIFY(top->isHidden());

        QTest::mouseClick(chrome, Qt::LeftButton, Qt::NoModifier,
                          QPoint(chrome->width() / 2, 10));
        QTRY_VERIFY_WITH_TIMEOUT(bottom->isVisible(), 500);
        QVERIFY(top->isVisible());

        QTest::mouseClick(playPause, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(surface->isPaused(), 1000);
        QTest::qWait(250);
        QVERIFY2(bottom->isVisible(), "Paused playback must keep controls visible");
    }

    void integratedTransportControlsDriveMpv() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("controls.mp4");
        const QStringList ffmpegArguments{
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30",
            "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
            "-t", "12", "-shortest", "-pix_fmt", "yuv420p",
            "-c:v", "libx264", "-c:a", "aac", videoPath,
        };
        QCOMPARE(QProcess::execute("ffmpeg", ffmpegArguments), 0);

        CloudStream::PlaybackSource source;
        source.source = "Local test";
        source.name = "Control fixture";
        source.url = videoPath;
        source.type = "VIDEO";
        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        discovery.sources = {source};
        CloudStream::PlayerPreferences preferences;
        preferences.seekSeconds = 5;
        preferences.autoHideDelayMs = 0;
        CloudStream::IntegratedPlayerWindow window(discovery, "Control test", 0.0,
                                                    nullptr, preferences);
        auto *surface = window.findChild<CloudStream::MpvPlayerWidget *>();
        auto *playPause = window.findChild<QPushButton *>("playerPlayPause");
        auto *rewind = window.findChild<QPushButton *>("playerRewind");
        auto *forward = window.findChild<QPushButton *>("playerForward");
        auto *scale = window.findChild<QPushButton *>("playerScale");
        auto *speed = window.findChild<QPushButton *>("playerSpeed");
        auto *mute = window.findChild<QPushButton *>("playerMute");
        auto *fullscreen = window.findChild<QPushButton *>("playerFullscreen");
        auto *volume = window.findChild<QSlider *>("playerVolume");
        QVERIFY(surface);
        QVERIFY(playPause);
        QVERIFY(rewind);
        QVERIFY(forward);
        QVERIFY(scale);
        QVERIFY(speed);
        QVERIFY(mute);
        QVERIFY(fullscreen);
        QVERIFY(volume);
        window.show();
        QSignalSpy loaded(surface, &CloudStream::MpvPlayerWidget::fileLoaded);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(surface->position() > 0.4, 3000);

        QTest::mouseClick(playPause, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(surface->isPaused(), 1000);
        QTest::mouseClick(playPause, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!surface->isPaused(), 1000);

        const auto beforeForward = surface->position();
        QTest::mouseClick(forward, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(surface->position() > beforeForward + 3.0, 1500);
        const auto beforeRewind = surface->position();
        QTest::mouseClick(rewind, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(surface->position() < beforeRewind - 3.0, 1500);

        volume->setValue(27);
        QTRY_COMPARE_WITH_TIMEOUT(surface->volume(), 27, 1000);
        QTest::mouseClick(mute, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(surface->isMuted(), 1000);
        QTest::mouseClick(mute, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!surface->isMuted(), 1000);

        QTest::mouseClick(scale, Qt::LeftButton);
        QVERIFY(surface->fillMode());
        QTest::mouseClick(speed, Qt::LeftButton);
        auto *speedDialog = window.findChild<QDialog *>("playerSpeedDialog");
        QVERIFY(speedDialog);
        auto *speedSlider = speedDialog->findChild<QSlider *>("playerSpeedSlider");
        QVERIFY(speedSlider);
        speedSlider->setValue(125);
        QCOMPARE(surface->playbackSpeed(), 1.25);
        speedDialog->accept();
        QTest::mouseClick(fullscreen, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(window.isFullScreen(), 1000);
        QTest::mouseClick(fullscreen, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!window.isFullScreen(), 1000);
    }

    void manualSourceSelectionCancelsStaleAutomaticFallback() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto videoPath = directory.filePath("manual-source.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {
            "-hide_banner", "-loglevel", "error", "-y",
            "-f", "lavfi", "-i", "testsrc2=size=640x360:rate=30",
            "-t", "4", "-pix_fmt", "yuv420p", "-c:v", "libx264", videoPath,
        }), 0);
        CloudStream::PlaybackSource broken;
        broken.source = "Broken";
        broken.name = "Broken";
        broken.url = directory.filePath("missing.mp4");
        broken.type = "VIDEO";
        CloudStream::PlaybackSource fallback;
        fallback.source = "Fallback";
        fallback.name = "Automatic fallback";
        fallback.url = videoPath;
        fallback.type = "VIDEO";
        CloudStream::PlaybackSource manual = fallback;
        manual.source = "Manual";
        manual.name = "User selection";
        CloudStream::SourceDiscovery discovery;
        discovery.success = true;
        discovery.sources = {broken, fallback, manual};
        CloudStream::PlayerPreferences preferences;
        preferences.autoHideDelayMs = 0;
        CloudStream::IntegratedPlayerWindow window(discovery, "Source race", 0.0,
                                                    nullptr, preferences);
        auto *surface = window.findChild<CloudStream::MpvPlayerWidget *>();
        auto *selector = window.findChild<QComboBox *>("sourceSelector");
        QVERIFY(surface);
        QVERIFY(selector);
        QSignalSpy errors(surface, &CloudStream::MpvPlayerWidget::playbackError);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);

        selector->setCurrentIndex(2);
        QTRY_COMPARE_WITH_TIMEOUT(window.currentSourceIndex(), 2, 1000);
        QTest::qWait(900);
        QCOMPARE(window.currentSourceIndex(), 2);
    }
};

QTEST_MAIN(MpvPlayerWidgetTest)
#include "test_mpv_player_widget.moc"
