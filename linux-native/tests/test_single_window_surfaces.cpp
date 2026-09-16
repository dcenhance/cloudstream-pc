#include <QtTest>
#include <QWindow>
#include "../player/MpvPlayerWidget.h"
#include "../updates/ReleaseUpdater.h"
#define main cloudstreamApplicationMain
#include "../main.cpp"
#undef main

class SingleWindowSurfacesTest final : public QObject {
    Q_OBJECT
    QTemporaryDir profile;

    static QImage renderWithUnderlay(QWidget *host, QWidget *underlay, const QColor &color) {
        underlay->setStyleSheet(QString("background:%1;").arg(color.name()));
        QApplication::processEvents();
        return host->grab().toImage();
    }

private slots:
    void init() {
        QSettings settings("CloudStream", "CloudStream Linux");
        settings.setValue("interface/windowMode", "Single-window navigation");
    }
    void initTestCase() {
        QVERIFY(profile.isValid());
        qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
        qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
        qputenv("XDG_CACHE_HOME", (profile.path() + "/cache").toUtf8());
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile.path());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, profile.path());
        QSettings settings("CloudStream", "CloudStream Linux");
        settings.setValue("interface/windowMode", "Single-window navigation");
        settings.sync();
        // Exercise the real application stylesheet, not a second hand-maintained theme.
        QFile source(QFINDTESTDATA("../main.cpp"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const auto textSource = QString::fromUtf8(source.readAll());
        const auto begin = textSource.indexOf("app.setStyleSheet(QString(R\"(");
        const auto end = textSource.indexOf(")\").arg(bg, text, surface, purple, surface2));", begin);
        QVERIFY(begin >= 0 && end > begin);
        defaultApplicationStyleSheet = textSource.mid(begin + QString("app.setStyleSheet(QString(R\"(").size(),
            end - begin - QString("app.setStyleSheet(QString(R\"(").size()).arg(bg, text, surface, purple, surface2);
        qApp->setStyle(QStyleFactory::create("Fusion"));
        qApp->setStyleSheet(defaultApplicationStyleSheet);

        QFile helper(profile.path() + "/provider-host");
        QVERIFY(helper.open(QIODevice::WriteOnly));
        helper.write("#!/bin/sh\nprintf '%s\\n' '{\"name\":\"Regression series\",\"plot\":\"Details must cover Search.\",\"episodes\":[{\"name\":\"First episode\",\"season\":1,\"episode\":1,\"data\":\"fixture\"}]}'\n");
        helper.close();
        QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("CLOUDSTREAM_PROVIDER_HOST", helper.fileName().toUtf8());
    }

    void realPlaybackFullscreenLifecycle_data() {
        QTest::addColumn<bool>("embedded");
        QTest::addColumn<bool>("maximized");
        QTest::newRow("embedded-normal") << true << false;
        QTest::newRow("embedded-maximized") << true << true;
        QTest::newRow("separate-normal") << false << false;
        QTest::newRow("separate-maximized") << false << true;
    }

    void realPlaybackFullscreenLifecycle() {
        QFETCH(bool, embedded);
        QFETCH(bool, maximized);
        QSettings settings("CloudStream", "CloudStream Linux");
        settings.setValue("interface/windowMode", embedded ? "Single-window navigation" : "Separate windows");
        settings.sync();
        QTemporaryDir media;
        const auto mediaPath = media.filePath("fixture.mp4");
        QCOMPARE(QProcess::execute("ffmpeg", {"-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
            "testsrc2=size=960x540:rate=30", "-t", "30", "-c:v", "libx264", "-preset", "ultrafast", mediaPath}), 0);
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        window.openPlayerForPreview(mediaPath, {});
        QPointer<CloudStream::IntegratedPlayerWindow> player = window.findChild<CloudStream::IntegratedPlayerWindow *>();
        QVERIFY(player);
        auto *surface = player->findChild<CloudStream::MpvPlayerWidget *>();
        auto *host = player->window();
        if (maximized) host->showMaximized();
        QVERIFY(QTest::qWaitForWindowExposed(host));
        QTRY_VERIFY(surface->renderedFrameCount() > 5);
        QTRY_VERIFY(surface->position() > 0.3);
        QCOMPARE(surface->currentVideoOutput(), QString("libmpv"));
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        QCOMPARE(pages->isVisible(), !embedded);
        QCOMPARE(player->isWindow(), !embedded);
        if (embedded) QCOMPARE(player->size(), window.centralWidget()->size());
        const QSize priorSize = host->size();
        const QRect priorGeometry = host->geometry();
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        const QString prefix = evidence + "/" + QTest::currentDataTag();
        auto capture = [&](const QString &name) {
            QTest::qWait(250);
            if (!evidence.isEmpty()) QVERIFY(host->grab().save(prefix + name + ".png"));
        };
        surface->setPaused(true);
        QTRY_VERIFY(surface->isPaused());
        capture("-window");
        // The control overlay must not paint an opaque strip over the movie.
        // testsrc2's left color bar is red through the transport row.
        const auto overlayPixel = player->grab().toImage().pixelColor(player->width()/8, player->height()/2);
        QVERIFY2(overlayPixel.red() > 50, "Opaque transport container covers the movie");
        QVERIFY2(overlayPixel.red() < 230, "Upstream 40-percent black scrim is missing");
        const auto frames = surface->renderedFrameCount();
        surface->setPaused(false);
        player->findChild<QPushButton *>("playerFullscreen")->click();
        QTRY_VERIFY(host->isFullScreen());
        QTRY_COMPARE(host->size(), host->screen()->geometry().size());
        QVERIFY(host->windowHandle()->windowState() == Qt::WindowFullScreen);
        QTRY_VERIFY(surface->renderedFrameCount() > frames);
        surface->setPaused(true);
        capture("-fullscreen");
        player->findChild<QPushButton *>("playerTracks")->click();
        QPointer<QDialog> panel = player->findChild<QDialog *>("playerTrackDialog");
        QVERIFY(panel && !panel->isWindow());
        capture("-tracks");
        QTest::keyClick(panel.data(), Qt::Key_Escape);
        QTRY_VERIFY(panel.isNull() || !panel->isVisible());
        QVERIFY(host->isFullScreen());
        for (const auto &name : {QString("Source"), QString("Speed")}) {
            player->findChild<QPushButton *>(name == "Source" ? "playerSources" : "playerSpeed")->click();
            QPointer<QDialog> menu = player->findChild<QDialog *>("player" + name + "Dialog");
            QVERIFY(menu && !menu->isWindow());
            capture("-" + name.toLower());
            QTest::keyClick(menu.data(), Qt::Key_Escape);
            QTRY_VERIFY(menu.isNull() || !menu->isVisible());
        }
        QTest::keyClick(player.data(), Qt::Key_F);
        QTRY_VERIFY(!host->isFullScreen());
        QTRY_COMPARE(host->size(), priorSize);
        if (QGuiApplication::platformName() == "xcb") QTRY_COMPARE(host->geometry(), priorGeometry);
        // Native maximization can trail the fullscreen flag/size transition.
        QTRY_COMPARE(host->isMaximized(), maximized);
        QVERIFY(QTest::qWaitForWindowActive(host));
        player->setFocus();
        QTest::keyClick(player.data(), Qt::Key_F11);
        QTRY_VERIFY(host->isFullScreen());
        QTRY_COMPARE(host->size(), host->screen()->geometry().size());
        QTest::keyClick(player.data(), Qt::Key_Escape);
        QTRY_VERIFY(!host->isFullScreen());
        QVERIFY(player && player->isVisible());
        capture("-restored");
        auto *chrome = player->findChild<QWidget *>("playerChrome");
        QTest::mouseDClick(chrome, Qt::LeftButton, Qt::NoModifier, QPoint(100, 150));
        QTRY_VERIFY(host->isFullScreen());
        player->findChild<QPushButton *>("playerBack")->click();
        QTRY_VERIFY(player.isNull());
        // Back restores the host through the compositor; the player can be
        // deleted before the native state/configure transition completes.
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(pages->isVisible());
        QCOMPARE(pages->currentIndex(), 1);
        if (embedded) QTRY_COMPARE(window.isMaximized(), maximized);
        if (!evidence.isEmpty()) QVERIFY(window.grab().save(prefix + "-navigation.png"));
        settings.setValue("interface/windowMode", "Single-window navigation");
    }

    void playbackOccupiesWholeContentAndRestoresNavigation() {
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.openPlayerForPreview(QString(), QString());
        QPointer<CloudStream::IntegratedPlayerWindow> player = window.findChild<CloudStream::IntegratedPlayerWindow *>();
        QVERIFY(player);
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        if (!evidence.isEmpty()) window.grab().save(evidence + "/embedded-before.png");
        QVERIFY2(!player->isWindow(), "Single-window playback must not create a separate player");
        QTRY_COMPARE(player->size(), window.centralWidget()->size());
        QVERIFY(!pages->isVisible());
        QVERIFY(!window.statusBar()->isVisible());
        window.resize(1280, 800);
        QTRY_COMPARE(player->size(), window.centralWidget()->size());
        player->close();
        QTRY_VERIFY(player.isNull());
        QVERIFY(pages->isVisible());
        QCOMPARE(pages->currentIndex(), 1);
    }

    void updaterSettingsRenderAndLiveCheck() {
        CloudStreamWindow window(false);
        window.resize(1280, 960);
        window.show();
        window.openSettingsSectionForPreview("Updates and backup");
        auto *button = window.findChild<QPushButton *>("checkAppUpdates");
        QVERIFY(button);
        auto *updater = window.findChild<CloudStream::Updates::ReleaseUpdater *>();
        QVERIFY(updater);
        if (qEnvironmentVariableIsSet("CLOUDSTREAM_UPDATER_LIVE")) {
            button->click();
            QTRY_VERIFY_WITH_TIMEOUT(!updater->busy(), 65000);
            QVERIFY2(!updater->release().version.isEmpty(), qPrintable(updater->status()));
            qInfo().noquote() << "LIVE GitHub updater:" << updater->currentVersion() << updater->release().version << updater->status();
        }
        QApplication::processEvents();
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            QVERIFY(window.grab().save(evidence + "/updates-settings.png"));
        }
    }
    void navigationDismissesDetailsBeforeRaisingSearch() {
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        QApplication::processEvents();
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        QVERIFY(pages);
        const auto searchOnly = pages->grab().toImage();
        window.openDetailsForPreview("fixture.jar", "Fixture", "https://fixture.invalid/title");
        QPointer<QDialog> details = window.findChild<QDialog *>("detailsDialog");
        QVERIFY(details);
        QTRY_VERIFY(details->findChild<QListWidget *>("episodeList"));
        window.selectPage(0);
        window.selectPage(1);
        QApplication::processEvents();
        const bool dismissed = details.isNull() || !details->isVisible();
        const auto returnedSearch = pages->grab().toImage();
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            window.grab().save(evidence + "/navigation-window.png");
        }
        // Cleanup even on RED: the original destroyed callback writes into the
        // main window's already-destructed QPointer during QWidget teardown.
        if (details) details->close();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY2(dismissed, "Details survives page navigation and mixes with raised Search");
        QCOMPARE(returnedSearch, searchOnly);
    }

    void embeddedDetailsCanOutliveAnOlderDetailsDialog() {
        CloudStreamWindow window(false);
        window.show();
        window.openDetailsForPreview("fixture.jar", "Fixture", "https://fixture.invalid/first");
        QPointer<QDialog> older = window.findChild<QDialog *>("detailsDialog");
        QVERIFY(older);
        window.openDetailsForPreview("fixture.jar", "Fixture", "https://fixture.invalid/second");
        QDialog *newer = nullptr;
        for (auto *candidate : window.findChildren<QDialog *>("detailsDialog"))
            if (candidate != older) newer = candidate;
        QVERIFY(newer);
        QTRY_VERIFY(older->findChild<QListWidget *>("episodeList"));
        QTRY_VERIFY(newer->findChild<QListWidget *>("episodeList"));
        older->close();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        window.resize(1280, 860);
        QApplication::processEvents();
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        const auto actual = newer->geometry();
        newer->close();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(actual, pages->rect());
    }

    void navigationWhileDetailsAreLoading() {
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        window.openDetailsForPreview("fixture.jar", "Fixture", "https://fixture.invalid/title");
        QPointer<QDialog> details = window.findChild<QDialog *>("detailsDialog");
        QVERIFY(details);
        window.selectPage(0);
        QTRY_VERIFY(details.isNull());
    }

    void navigationDismissesEmbeddedPlayerButNotSeparateWindows() {
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        QVERIFY(pages);
        // Use the real player with no network media; match resolveAndPlay's
        // embedding path without starting a provider or an external player.
        auto *player = new CloudStream::IntegratedPlayerWindow({}, "Fixture player", 0, &window);
        player->setWindowFlags(Qt::Widget);
        player->setParent(pages);
        player->setGeometry(pages->rect());
        player->show();
        QDialog separate(&window);
        separate.show();
        window.selectPage(0);
        const bool dismissed = !player->isVisible();
        const bool separateVisible = separate.isVisible();
        player->close();
        separate.close();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(dismissed);
        QVERIFY(separateVisible);
    }

    void detailsDoNotRevealSearch() {
        CloudStreamWindow window(false);
        window.show();
        window.selectPage(1);
        QApplication::processEvents();
        auto *pages = window.findChild<QStackedWidget *>("appPages");
        QVERIFY(pages);
        auto *search = pages->currentWidget();
        window.openDetailsForPreview("fixture.jar", "Fixture", "https://fixture.invalid/title");
        auto *details = window.findChild<QDialog *>("detailsDialog");
        QVERIFY(details);
        QVERIFY(!details->isWindow());
        // Loading must already obscure the page; loaded content must do so too.
        const auto loadingA = renderWithUnderlay(pages, search, Qt::magenta);
        const auto loadingB = renderWithUnderlay(pages, search, Qt::green);
        QTRY_VERIFY(details->findChild<QListWidget *>("episodeList"));
        const auto loadedA = renderWithUnderlay(pages, search, Qt::magenta);
        const auto loadedB = renderWithUnderlay(pages, search, Qt::green);
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        if (!evidence.isEmpty()) {
            QDir().mkpath(evidence);
            loadingA.save(evidence + "/details-loading.png");
            loadedA.save(evidence + "/details-loaded.png");
            window.grab().save(evidence + "/details-window.png");
        }
        QCOMPARE(loadingA, loadingB);
        QCOMPARE(loadedA, loadedB);
        details->close();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(pages->currentWidget(), search);
        QVERIFY(search->isVisible());
    }
};
QTEST_MAIN(SingleWindowSurfacesTest)
#include "test_single_window_surfaces.moc"
