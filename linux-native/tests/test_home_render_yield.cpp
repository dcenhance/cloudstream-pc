#include <QtTest>
#include <QTemporaryDir>
#include <QWheelEvent>
#define private public
#define main cloudstreamApplicationMain
#include "../main.cpp"
#undef main
#undef private

class HomeRenderYieldTest final : public QObject {
    Q_OBJECT
    QTemporaryDir profile;
private slots:
    void initTestCase() {
        QVERIFY(profile.isValid());
        qputenv("XDG_DATA_HOME", (profile.path() + "/data").toUtf8());
        qputenv("XDG_CONFIG_HOME", (profile.path() + "/config").toUtf8());
        qputenv("XDG_CACHE_HOME", (profile.path() + "/cache").toUtf8());
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, profile.path());
    }
    void homeSectionsYieldBetweenCardRows() {
        CloudStreamWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QJsonArray sections;
        for (int row = 0; row < 12; ++row) {
            QJsonObject section;
            section.insert("name", QString("Row %1").arg(row));
            QJsonArray items;
            for (int item = 0; item < 24; ++item) {
                QJsonObject card;
                card.insert("name", QString("Item %1").arg(item));
                items.append(card);
            }
            section.insert("items", items);
            sections.append(section);
        }
        window.renderHomeSections(sections, "Fixture");
        // Rendering six 24-card rows synchronously blocks paint and input until all 144 cards exist.
        QCOMPARE(window.renderedHomeSectionCount, 1);
        // Continue scrolling; every section must eventually be reachable, without a Show more button.
        bool completed = false;
        window.loadAllHomeSectionsForAutomation([&](int rendered, int total, int manual, qint64) {
            QCOMPARE(rendered, 12);
            QCOMPARE(total, 12);
            QCOMPARE(manual, 0);
            completed = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(completed, 15000);
    }
    void homeRowCanRevealAllProviderTitlesInBoundedPages() {
        CloudStreamWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QJsonArray items;
        for (int index = 0; index < 55; ++index) {
            items.append(QJsonObject{{"name", QString("Title %1").arg(index)},
                                     {"url", QString("https://example.test/%1").arg(index)},
                                     {"apiName", "Fixture"}});
        }
        window.renderHomeSections(QJsonArray{QJsonObject{{"name", "Many titles"}, {"items", items}}},
                                  "Fixture");
        auto *row = window.homeSectionsContainer->findChild<QListWidget *>("posterRow");
        QVERIFY(row);
        QCOMPARE(row->count(), 24);
        auto *more = window.homeSectionsContainer->findChild<QPushButton *>("homeRowLoadMore");
        QVERIFY(more);
        QVERIFY(!more->isHidden());
        more->click();
        QCOMPARE(row->count(), 48);
        QCOMPARE(row->item(47)->data(Qt::UserRole).toString(), QString("https://example.test/47"));
        more->click();
        QCOMPARE(row->count(), 55);
        QCOMPARE(row->item(54)->data(Qt::UserRole).toString(), QString("https://example.test/54"));
        QVERIFY(more->isHidden());
    }
    void firstWheelDuringHomeResetIsNotRewound() {
        CloudStreamWindow window(false);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QJsonArray sections;
        for (int row = 0; row < 8; ++row) {
            QJsonObject section;
            section.insert("name", QString("Row %1").arg(row));
            QJsonArray items;
            for (int item = 0; item < 4; ++item) {
                QJsonObject card;
                card.insert("name", QString("Item %1").arg(item));
                card.insert("url", QString("https://example.test/title/%1/%2").arg(row).arg(item));
                items.append(card);
            }
            section.insert("items", items);
            sections.append(section);
        }
        window.renderHomeSections(sections, "Fixture");
        bool loaded = false;
        window.loadAllHomeSectionsForAutomation([&](int rendered, int total, int, qint64) {
            loaded = rendered == total;
        });
        QTRY_VERIFY_WITH_TIMEOUT(loaded, 15000);
        auto *area = window.homeScrollArea;
        QVERIFY(area);
        auto *bar = area->verticalScrollBar();
        QVERIFY(bar->maximum() > 120);
        window.resetHomeViewport();
        QTest::qWait(25); // Let the layout reset; the old 150-ms reset is still pending.
        const auto center = area->viewport()->rect().center();
        QWheelEvent wheel(center, area->viewport()->mapToGlobal(center),
                          QPoint(0, -120), {}, Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(area->viewport(), &wheel);
        QVERIFY(wheel.isAccepted());
        QVERIFY(bar->value() > 0);
        QTest::qWait(200);
        QVERIFY2(bar->value() > 0,
                 "Home reset discarded the first downward wheel while at the top");
    }
};
QTEST_MAIN(HomeRenderYieldTest)
#include "test_home_render_yield.moc"
