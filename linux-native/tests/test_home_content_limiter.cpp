#include "../providers/HomeContentLimiter.h"

#include <QtTest>

class HomeContentLimiterTest : public QObject {
    Q_OBJECT

private slots:
    void boundsSectionsAndCardsWhileKeepingOriginalCounts() {
        QJsonArray sections;
        for (int sectionIndex = 0; sectionIndex < 12; ++sectionIndex) {
            QJsonArray items;
            for (int itemIndex = 0; itemIndex < 40; ++itemIndex) {
                items.append(QJsonObject{{"name", QString::number(itemIndex)}});
            }
            sections.append(QJsonObject{{"name", QString::number(sectionIndex)}, {"items", items}});
        }

        const auto limited = CloudStream::HomeContentLimiter::limit(sections, 10, 30);

        QCOMPARE(limited.size(), 10);
        QCOMPARE(limited[0].toObject().value("items").toArray().size(), 30);
        QCOMPARE(limited[0].toObject().value("totalItems").toInt(), 40);
        QCOMPARE(limited[9].toObject().value("name").toString(), QString("9"));
    }

    void detectsWhenAHomeRefreshWouldBeAVisualNoOp() {
        const QJsonArray original{
            QJsonObject{{"name", "Popular"}, {"items", QJsonArray{
                QJsonObject{{"name", "One"}, {"url", "https://media/one"}},
            }}},
        };
        auto changed = original;
        auto section = changed[0].toObject();
        section.insert("name", "Recently updated");
        changed[0] = section;

        QVERIFY(CloudStream::HomeContentLimiter::equivalent(original, original));
        QVERIFY(!CloudStream::HomeContentLimiter::equivalent(original, changed));
    }

    void advancesSectionsInBoundedProgressiveBatches() {
        QCOMPARE(CloudStream::HomeContentLimiter::nextSectionCount(0, 62, 6), 6);
        QCOMPARE(CloudStream::HomeContentLimiter::nextSectionCount(6, 62, 4), 10);
        QCOMPARE(CloudStream::HomeContentLimiter::nextSectionCount(60, 62, 4), 62);
        QCOMPARE(CloudStream::HomeContentLimiter::nextSectionCount(62, 62, 4), 62);
    }

    void limitsOnlyTheNewProgressiveRange() {
        QJsonArray sections;
        for (int sectionIndex = 0; sectionIndex < 12; ++sectionIndex) {
            QJsonArray items;
            for (int itemIndex = 0; itemIndex < 30; ++itemIndex) {
                items.append(QJsonObject{{"name", QString::number(itemIndex)}});
            }
            sections.append(QJsonObject{{"name", QString::number(sectionIndex)},
                                        {"items", items}});
        }

        const auto next = CloudStream::HomeContentLimiter::limitRange(sections, 6, 4, 24);
        QCOMPARE(next.size(), 4);
        QCOMPARE(next[0].toObject().value("name").toString(), QString("6"));
        QCOMPARE(next[3].toObject().value("name").toString(), QString("9"));
        QCOMPARE(next[0].toObject().value("items").toArray().size(), 24);
        QCOMPARE(next[0].toObject().value("totalItems").toInt(), 30);
    }
};

QTEST_MAIN(HomeContentLimiterTest)
#include "test_home_content_limiter.moc"
