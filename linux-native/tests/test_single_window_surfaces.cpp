#include <QtTest>
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
