#include "../providers/HomeHeroSelection.h"

#include <QJsonObject>
#include <QtTest>

class HomeHeroSelectionTest : public QObject {
    Q_OBJECT

private slots:
    void prefersFirstPlayableItemWithArtwork() {
        const QJsonArray sections{
            QJsonObject{{"name", "Empty"}, {"items", QJsonArray{QJsonObject{{"name", "No URL"}, {"posterUrl", "https://img/ignored.jpg"}}}}},
            QJsonObject{{"name", "Popular"}, {"items", QJsonArray{
                QJsonObject{{"name", "No art"}, {"url", "https://media/no-art"}},
                QJsonObject{{"name", "Featured"}, {"url", "https://media/featured"},
                            {"posterUrl", "https://img/featured.jpg"}, {"apiName", "AniWorld"}},
            }}},
        };
        const auto hero = CloudStream::HomeHeroSelection::select(sections, "Fallback");
        QVERIFY(hero.valid);
        QCOMPARE(hero.name, QString("Featured"));
        QCOMPARE(hero.url, QString("https://media/featured"));
        QCOMPARE(hero.posterUrl, QString("https://img/featured.jpg"));
        QCOMPARE(hero.providerName, QString("AniWorld"));
    }

    void fallsBackToPlayableItemAndProviderName() {
        const QJsonArray sections{QJsonObject{{"items", QJsonArray{
            QJsonObject{{"name", "Playable"}, {"url", "https://media/playable"}},
        }}}};
        const auto hero = CloudStream::HomeHeroSelection::select(sections, "Fallback");
        QVERIFY(hero.valid);
        QCOMPARE(hero.name, QString("Playable"));
        QCOMPARE(hero.providerName, QString("Fallback"));
    }

    void returnsInvalidForEmptySections() {
        QVERIFY(!CloudStream::HomeHeroSelection::select({}, "Provider").valid);
    }
};

QTEST_MAIN(HomeHeroSelectionTest)
#include "test_home_hero_selection.moc"
