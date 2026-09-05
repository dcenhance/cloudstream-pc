#include "../search/SearchHistoryModel.h"

#include <QtTest>

class SearchHistoryModelTest : public QObject {
    Q_OBJECT

private slots:
    void normalizesDeduplicatesAndMovesNewestFirst() {
        const auto values = CloudStream::SearchHistoryModel::add(
            {"Naruto", "Perfect Blue", "One Piece"}, "  perfect   blue  ");
        QCOMPARE(values, QStringList({"perfect blue", "Naruto", "One Piece"}));
    }

    void boundsHistoryAndRemovesCaseInsensitively() {
        QStringList values{"a", "b", "c"};
        QCOMPARE(CloudStream::SearchHistoryModel::add(values, "d", 3), QStringList({"d", "a", "b"}));
        QCOMPARE(CloudStream::SearchHistoryModel::remove({"Naruto", "One Piece"}, "naruto"),
                 QStringList({"One Piece"}));
    }

    void ignoresBlankQueries() {
        QCOMPARE(CloudStream::SearchHistoryModel::add({"Naruto"}, "   "), QStringList({"Naruto"}));
    }
};

QTEST_MAIN(SearchHistoryModelTest)
#include "test_search_history_model.moc"
