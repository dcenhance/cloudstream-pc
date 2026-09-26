#include <QtTest>
#include "../search/SearchPaginationModel.h"

class SearchPaginationModelTest : public QObject {
    Q_OBJECT
private slots:
    void tracksEachProviderAndDeduplicatesAcrossPages() {
        CloudStream::SearchPaginationModel model;
        model.reset({"jar-a\nA", "jar-b\nB"});
        QCOMPARE(model.pageFor("jar-a\nA"), 1);
        QCOMPARE(model.pageFor("jar-b\nB"), 1);
        const QJsonObject first{{"url", "https://fixture.invalid/one"}, {"apiName", "A"}, {"_jarPath", "jar-a"}};
        const QJsonObject second{{"url", "https://fixture.invalid/two"}, {"apiName", "A"}, {"_jarPath", "jar-a"}};
        QCOMPARE(model.complete("jar-a\nA", 1, true, {first}).size(), 1);
        QCOMPARE(model.pageFor("jar-a\nA"), 2);
        QCOMPARE(model.pageFor("jar-b\nB"), 1);
        QCOMPARE(model.complete("jar-a\nA", 2, false, {first, second}).size(), 1);
        QCOMPARE(model.pageFor("jar-a\nA"), 0);
        QCOMPARE(model.pageFor("jar-b\nB"), 1);
        QCOMPARE(model.count(), 2);
        model.reset({"jar-a\nA"});
        QCOMPARE(model.pageFor("jar-a\nA"), 1);
        QCOMPARE(model.count(), 0);
    }
};
QTEST_GUILESS_MAIN(SearchPaginationModelTest)
#include "test_search_pagination_model.moc"
