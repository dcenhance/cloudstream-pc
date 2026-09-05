#include "../history/WatchHistoryStore.h"

#include <QTemporaryDir>
#include <QtTest>

class WatchHistoryStoreTest : public QObject {
    Q_OBJECT

private slots:
    void persistsCompleteMetadataAndSortsNewestFirst() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("watch-history.json");
        CloudStream::WatchHistoryStore store(path);

        CloudStream::WatchEntry older;
        older.id = CloudStream::WatchHistoryStore::idFor("Provider A", "https://example.org/older");
        older.name = "Older title";
        older.sourceUrl = "https://example.org/older";
        older.provider = "Provider A";
        older.jarPath = "/extensions/provider-a.jar";
        older.posterUrl = "https://example.org/older.jpg";
        older.playbackData = "older-data";
        older.episodeName = "Episode 2";
        older.state = "Watching";
        older.positionSeconds = 120.0;
        older.durationSeconds = 1200.0;
        older.updatedAt = 100;
        QVERIFY(store.upsert(older));

        CloudStream::WatchEntry newer = older;
        newer.id = CloudStream::WatchHistoryStore::idFor("Provider B", "https://example.org/newer");
        newer.name = "Newer title";
        newer.sourceUrl = "https://example.org/newer";
        newer.provider = "Provider B";
        newer.jarPath = "/extensions/provider-b.jar";
        newer.posterUrl = "https://example.org/newer.jpg";
        newer.playbackData = "newer-data";
        newer.episodeName = "Episode 7";
        newer.updatedAt = 200;
        QVERIFY(store.upsert(newer));

        CloudStream::WatchHistoryStore reopened(path);
        const auto entries = reopened.entries();
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].name, QString("Newer title"));
        QCOMPARE(entries[0].provider, QString("Provider B"));
        QCOMPARE(entries[0].jarPath, QString("/extensions/provider-b.jar"));
        QCOMPARE(entries[0].posterUrl, QString("https://example.org/newer.jpg"));
        QCOMPARE(entries[0].playbackData, QString("newer-data"));
        QCOMPARE(entries[0].episodeName, QString("Episode 7"));
        QCOMPARE(entries[1].name, QString("Older title"));
    }

    void updatesProgressAndCompletesNearTheEnd() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CloudStream::WatchHistoryStore store(directory.filePath("watch-history.json"));
        CloudStream::WatchEntry entry;
        entry.id = CloudStream::WatchHistoryStore::idFor("Provider", "https://example.org/title");
        entry.name = "Progress title";
        entry.sourceUrl = "https://example.org/title";
        entry.provider = "Provider";
        entry.state = "Watching";
        QVERIFY(store.upsert(entry));

        QVERIFY(store.updateProgress(entry.id, 450.0, 500.0, 300));
        const auto completed = store.entries("Completed");
        QCOMPARE(completed.size(), 1);
        QCOMPARE(completed[0].positionSeconds, 450.0);
        QCOMPARE(completed[0].durationSeconds, 500.0);
        QCOMPARE(completed[0].updatedAt, qint64(300));
        QCOMPARE(completed[0].name, QString("Progress title"));

        QVERIFY(store.updateProgress(entry.id, -20.0, 500.0, 400));
        const auto watching = store.entries("Watching");
        QCOMPARE(watching.size(), 1);
        QCOMPARE(watching[0].positionSeconds, 0.0);
        QCOMPARE(watching[0].durationSeconds, 500.0);
        QVERIFY(!store.updateProgress("missing", 1.0, 2.0, 500));
    }

    void changesOnlyToSupportedWatchStates() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CloudStream::WatchHistoryStore store(directory.filePath("watch-history.json"));
        CloudStream::WatchEntry entry;
        entry.id = CloudStream::WatchHistoryStore::idFor("Provider", "https://example.org/title");
        entry.name = "State title";
        entry.sourceUrl = "https://example.org/title";
        entry.provider = "Provider";
        QVERIFY(store.upsert(entry));

        QVERIFY(store.setState(entry.id, "Paused"));
        QCOMPARE(store.entries("Paused").size(), 1);
        QVERIFY(store.setState(entry.id, "Cancelled"));
        QCOMPARE(store.entries("Cancelled").size(), 1);
        QVERIFY(!store.setState(entry.id, "Invalid"));
        QCOMPARE(store.entries("Cancelled").size(), 1);
        QVERIFY(!store.setState("missing", "Watching"));
    }

    void removesLibraryEntryWithoutTouchingExternalMedia() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        CloudStream::WatchHistoryStore store(directory.filePath("watch-history.json"));
        CloudStream::WatchEntry entry;
        entry.id = CloudStream::WatchHistoryStore::idFor("Provider", "https://example.org/title");
        entry.name = "Removable title";
        entry.sourceUrl = "https://example.org/title";
        entry.provider = "Provider";
        QVERIFY(store.upsert(entry));

        QVERIFY(store.remove(entry.id));
        QVERIFY(store.entries().isEmpty());
        QVERIFY(!store.remove(entry.id));
    }
};

QTEST_APPLESS_MAIN(WatchHistoryStoreTest)
#include "test_watch_history_store.moc"
