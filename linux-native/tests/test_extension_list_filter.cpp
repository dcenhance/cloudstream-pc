#include "../extensions/ExtensionListFilter.h"

#include <QtTest>

class ExtensionListFilterTest : public QObject {
    Q_OBJECT

private slots:
    void blankQueryMatchesEverything() {
        QVERIFY(CloudStream::ExtensionListFilter::matches("AniWorld German Anime", "   "));
    }

    void matchesAllTokensCaseInsensitively() {
        QVERIFY(CloudStream::ExtensionListFilter::matches("AniWorld • German • Anime", "anime ANI"));
        QVERIFY(!CloudStream::ExtensionListFilter::matches("AniWorld • German • Anime", "anime english"));
    }
};

QTEST_MAIN(ExtensionListFilterTest)
#include "test_extension_list_filter.moc"
