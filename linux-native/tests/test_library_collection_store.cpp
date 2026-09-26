#include "../history/LibraryCollectionStore.h"
#include "../history/WatchHistoryStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace CloudStream;

class LibraryCollectionStoreTest : public QObject {
    Q_OBJECT
private slots:
    void createsAndSelectsNamedCollectionsAcrossRestart() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("collections.json");
        LibraryCollectionStore store(path);
        QVERIFY(store.collections().isEmpty());
        QVERIFY(store.selectedId().isEmpty());
        const auto id = store.create("  Favorites  ");
        QVERIFY(!id.isEmpty());
        QCOMPARE(store.collections().size(), 1);
        QCOMPARE(store.collections().first().name, QString("Favorites"));
        QVERIFY(store.select(id));
        LibraryCollectionStore reopened(path);
        QCOMPARE(reopened.selectedId(), id);
        QCOMPARE(reopened.collections().first().id, id);
        QVERIFY(!reopened.create(" favorites ").size());
        QVERIFY(!reopened.create("   ").size());
        QVERIFY(!reopened.select("missing"));
        QVERIFY(reopened.select({}));
        QVERIFY(reopened.selectedId().isEmpty());
    }

    void membershipAndRenameSurviveRestartWithoutChangingWatchHistory() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto historyPath = directory.filePath("watch-history.json");
        WatchHistoryStore history(historyPath);
        WatchEntry entry;
        entry.id = WatchHistoryStore::idFor("Provider", "https://example.org/title");
        entry.name = "Saved title";
        QVERIFY(history.upsert(entry));
        LibraryCollectionStore store(directory.filePath("collections.json"));
        const auto a = store.create("Favorites");
        const auto b = store.create("Weekend");
        QVERIFY(store.add(a, entry.id));
        QVERIFY(!store.add(a, entry.id));
        QVERIFY(store.add(b, entry.id));
        QVERIFY(store.rename(a, "Top picks"));
        QVERIFY(!store.rename(b, "TOP PICKS"));
        LibraryCollectionStore reopened(directory.filePath("collections.json"));
        QCOMPARE(reopened.collections().first().name, QString("Top picks"));
        QVERIFY(reopened.contains(a, entry.id));
        QVERIFY(reopened.contains(b, entry.id));
        QVERIFY(reopened.removeItem(a, entry.id));
        QVERIFY(!reopened.contains(a, entry.id));
        QVERIFY(reopened.contains(b, entry.id));
        QVERIFY(reopened.select(b));
        QVERIFY(reopened.removeCollection(b));
        QVERIFY(reopened.selectedId().isEmpty());
        QVERIFY(history.entries().size() == 1);
        QCOMPARE(history.entries().first().name, QString("Saved title"));
        QVERIFY(QFile::exists(historyPath));
    }

    void rejectsInvalidMembershipWithoutRewritingExistingData() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        LibraryCollectionStore store(directory.filePath("collections.json"));
        const auto id = store.create("Watch later");
        QVERIFY(!id.isEmpty());
        QVERIFY(!store.add("missing", "title"));
        QVERIFY(!store.add(id, " "));
        QVERIFY(!store.removeItem(id, "missing"));
        QVERIFY(!store.removeCollection("missing"));
        QVERIFY(!store.rename(id, " "));
        LibraryCollectionStore reopened(directory.filePath("collections.json"));
        QCOMPARE(reopened.collections().size(), 1);
        QVERIFY(reopened.collections().first().itemIds.isEmpty());
    }
};

QTEST_APPLESS_MAIN(LibraryCollectionStoreTest)
#include "test_library_collection_store.moc"
