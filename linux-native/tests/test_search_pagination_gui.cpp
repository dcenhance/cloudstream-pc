#include <QtTest>
#include "SearchPaginationGuiTest.h"
#define main cloudstreamApplicationMain
#include "../main.cpp"
#undef main

bool SearchPaginationGuiTest::providerReady(QWidget &window) {
    for (auto *button : window.findChildren<QPushButton *>())
        if (button->text().contains("Automatic (1)")) return true;
    return false;
}

void SearchPaginationGuiTest::initTestCase() {
        QVERIFY(profile.isValid());
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
        qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
        qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
        qputenv("XDG_CACHE_HOME", (profile.path() + "/cache").toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, profile.path());
        QFile jar(profile.filePath("fixture.jar"));
        QVERIFY(jar.open(QIODevice::WriteOnly));
        jar.close();
        const auto nativeFixture = qEnvironmentVariable("CLOUDSTREAM_TEST_SEARCH_FIXTURE");
        if (!nativeFixture.isEmpty()) {
            QVERIFY(QFileInfo::exists(nativeFixture));
            qputenv("CLOUDSTREAM_PROVIDER_HOST", nativeFixture.toUtf8());
        } else {
        QFile script(profile.filePath("provider-host"));
        QVERIFY(script.open(QIODevice::WriteOnly));
        script.write(R"SH(#!/bin/sh
case "$1" in
  list) printf '%s\n' '[{"name":"Fixture","language":"en","hasMainPage":false,"supportedTypes":["Movie","TvSeries"]}]' ;;
  search)
    if [ "$7" = slow ]; then sleep 1; fi
    if [ "$6" = 1 ]; then
      printf '%s\n' '{"items":[{"name":"First","url":"https://fixture.invalid/one","apiName":"Fixture","type":"Movie"}],"hasNext":true}'
    elif [ "$6" = 2 ]; then
      printf '%s\n' '{"items":[{"name":"First again","url":"https://fixture.invalid/one","apiName":"Fixture","type":"Movie"},{"name":"Second","url":"https://fixture.invalid/two","apiName":"Fixture","type":"TvSeries"}],"hasNext":false}'
    else
      exit 3
    fi ;;
  *) exit 2 ;;
esac
)SH");
        script.close();
        QVERIFY(script.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("CLOUDSTREAM_PROVIDER_HOST", script.fileName().toUtf8());
        }
        if (!nativeFixture.isEmpty())
            QCOMPARE(qEnvironmentVariable("CLOUDSTREAM_PROVIDER_HOST"), nativeFixture);
        CloudStream::ExtensionRegistry registry(CloudStream::XdgPaths::dataDir() + "/extension-registry.json");
        CloudStream::ExtensionRecord extension;
        extension.internalName = "fixture";
        extension.displayName = "Fixture";
        extension.artifactPath = jar.fileName();
        extension.platform = "jvm";
        QVERIFY(registry.upsertExtension(extension));
    }

void SearchPaginationGuiTest::loadsSecondPageOnlyWhenRequestedAndPreservesFilters() {
        CloudStreamWindow window;
        window.show();
        window.selectPage(1);
        auto *query = window.findChild<QLineEdit *>("searchField");
        auto *results = window.findChild<QStackedWidget *>("appPages")->widget(1)->findChild<QListWidget *>("mediaList");
        auto *loadMore = window.findChild<QPushButton *>("searchLoadMore");
        QVERIFY(query && results && loadMore);
        QTRY_VERIFY_WITH_TIMEOUT(providerReady(window), 10000);
        query->setFocus();
        QTest::keyClicks(query, "fixture");
        QTest::keyClick(query, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(results->count(), 1, 10000);
        QCOMPARE(results->item(0)->text(), QString("First"));
        QTRY_VERIFY_WITH_TIMEOUT(loadMore->isVisible() && loadMore->isEnabled(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!window.findChild<QWidget *>("startupOverlay"), 3000);
        const auto evidence = qEnvironmentVariable("CLOUDSTREAM_TEST_EVIDENCE");
        if (!evidence.isEmpty()) {
            QVERIFY(QDir().mkpath(evidence));
            QVERIFY(window.grab().save(evidence + "/search-page-1.png"));
        }
        loadMore->click();
        QTRY_COMPARE_WITH_TIMEOUT(results->count(), 2, 10000);
        if (!evidence.isEmpty()) QVERIFY(window.grab().save(evidence + "/search-page-2.png"));
        QCOMPARE(results->item(1)->text(), QString("Second"));
        QVERIFY(!loadMore->isVisible());
        auto *providerFilter = window.findChild<QComboBox *>("searchResultProviderFilter");
        QVERIFY(providerFilter);
        QCOMPARE(providerFilter->count(), 2);
        providerFilter->setCurrentIndex(1);
        QCOMPARE(results->item(0)->isHidden(), false);
        QCOMPARE(results->item(1)->isHidden(), false);
        for (auto *chip : window.findChildren<QPushButton *>()) {
            if (chip->text() == "Movies") { chip->click(); break; }
        }
        QVERIFY(results->item(1)->isHidden());
        QVERIFY(!results->item(0)->isHidden());
    }

void SearchPaginationGuiTest::staleSearchCannotAppendAfterNewQuery() {
        CloudStreamWindow window;
        window.show();
        window.selectPage(1);
        auto *query = window.findChild<QLineEdit *>("searchField");
        auto *results = window.findChild<QStackedWidget *>("appPages")->widget(1)->findChild<QListWidget *>("mediaList");
        QVERIFY(query && results);
        QTRY_VERIFY_WITH_TIMEOUT(providerReady(window), 10000);
        query->setText("slow");
        QTest::keyClick(query, Qt::Key_Return);
        query->setText("fast");
        QTest::keyClick(query, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(results->count(), 1, 10000);
        QTest::qWait(1300);
        QCOMPARE(results->count(), 1);
        QCOMPARE(results->item(0)->text(), QString("First"));
    }
void SearchPaginationGuiTest::providerSelectionChangeInvalidatesPendingPage() {
    QSettings settings("CloudStream", "CloudStream Linux");
    settings.remove("searchProviderKeys");
    CloudStreamWindow window;
    window.show();
    window.selectPage(1);
    auto *query = window.findChild<QLineEdit *>("searchField");
    auto *results = window.findChild<QStackedWidget *>("appPages")->widget(1)->findChild<QListWidget *>("mediaList");
    auto *loadMore = window.findChild<QPushButton *>("searchLoadMore");
    QVERIFY(query && results && loadMore);
    QTRY_VERIFY_WITH_TIMEOUT(providerReady(window), 10000);
    query->setText("slow");
    QTest::keyClick(query, Qt::Key_Return);
    settings.setValue("searchProviderKeys", QStringList{"missing-provider"});
    QTest::qWait(1300);
    QCOMPARE(results->count(), 0);
    QVERIFY(!loadMore->isVisible());
    settings.remove("searchProviderKeys");
}

QTEST_MAIN(SearchPaginationGuiTest)
