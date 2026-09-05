#include "../episodes/EpisodeCatalog.h"

#include <QtTest>

class EpisodeCatalogTest : public QObject {
    Q_OBJECT

private slots:
    void mergesDuplicateProviderRowsAndRetainsDubStates() {
        const QJsonArray raw{
            QJsonObject{{"data", "episode-1"}, {"season", 1}, {"episode", 1}, {"name", "Pilot"}, {"dubStatus", "Dubbed"}},
            QJsonObject{{"data", "episode-1"}, {"season", 1}, {"episode", 1}, {"name", "Pilot"}, {"dubStatus", "Subbed"}},
            QJsonObject{{"data", "episode-2"}, {"season", 1}, {"episode", 2}, {"name", "Second"}, {"dubStatus", "Subbed"}},
            QJsonObject{{"data", "episode-2"}, {"season", 1}, {"episode", 2}, {"name", "Second"}, {"dubStatus", "Subbed"}},
            QJsonObject{{"data", "episode-3"}, {"season", 2}, {"episode", 1}, {"name", "Return"}},
        };

        const auto episodes = CloudStream::EpisodeCatalog::fromJson(raw);

        QCOMPARE(episodes.size(), 3);
        QCOMPARE(episodes[0].dubStatuses, QStringList({"Dubbed", "Subbed"}));
        QCOMPARE(CloudStream::EpisodeCatalog::seasons(episodes), QList<int>({1, 2}));
        QCOMPARE(CloudStream::EpisodeCatalog::dubStatuses(episodes), QStringList({"Dubbed", "Subbed"}));
    }

    void filtersBySeasonDubStateAndText() {
        const auto episodes = CloudStream::EpisodeCatalog::fromJson(QJsonArray{
            QJsonObject{{"data", "a"}, {"season", 1}, {"episode", 1}, {"name", "Pilot"}, {"dubStatus", "Dubbed"}},
            QJsonObject{{"data", "b"}, {"season", 1}, {"episode", 2}, {"name", "Lost Technology"}, {"dubStatus", "Subbed"}},
            QJsonObject{{"data", "c"}, {"season", 2}, {"episode", 1}, {"name", "Return"}, {"dubStatus", "Subbed"}},
        });

        const auto filtered = CloudStream::EpisodeCatalog::filter(episodes, 1, "Subbed", "technology");
        QCOMPARE(filtered.size(), 1);
        QCOMPARE(filtered[0].data, QString("b"));
    }

    void returnsBoundedEpisodePages() {
        QList<CloudStream::EpisodeEntry> episodes;
        for (int index = 0; index < 123; ++index) {
            CloudStream::EpisodeEntry episode;
            episode.data = QString::number(index);
            episodes << episode;
        }
        const auto middle = CloudStream::EpisodeCatalog::page(episodes, 50, 50);
        QCOMPARE(middle.size(), 50);
        QCOMPARE(middle.first().data, QString("50"));
        QCOMPARE(middle.last().data, QString("99"));
        QCOMPARE(CloudStream::EpisodeCatalog::page(episodes, 100, 50).size(), 23);
        QVERIFY(CloudStream::EpisodeCatalog::page(episodes, 200, 50).isEmpty());
        QVERIFY(CloudStream::EpisodeCatalog::page(episodes, 0, 0).isEmpty());
    }
};

QTEST_APPLESS_MAIN(EpisodeCatalogTest)
#include "test_episode_catalog.moc"
