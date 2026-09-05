#include <QtTest>

#include "../providers/ProviderPreferenceFilter.h"

class ProviderPreferenceFilterTest final : public QObject {
    Q_OBJECT
private slots:
    void filtersLanguageAndAdultTypesWithoutMutatingInput() {
        const QList<QJsonObject> providers{
            QJsonObject{{"name", "Deutsch"}, {"language", "de"},
                        {"supportedTypes", QJsonArray{"Movie"}}},
            QJsonObject{{"name", "English"}, {"language", "en"},
                        {"supportedTypes", QJsonArray{"TVSeries"}}},
            QJsonObject{{"name", "Adult German"}, {"language", "de"},
                        {"supportedTypes", QJsonArray{"NSFW"}}},
        };
        const auto filtered = CloudStream::ProviderPreferenceFilter::apply(
            providers, "German", false);
        QCOMPARE(filtered.size(), 1);
        QCOMPARE(filtered.first().value("name").toString(), QString("Deutsch"));
        QCOMPARE(providers.size(), 3);
    }

    void allLanguagesRetainsUnknownAndAdultWhenEnabled() {
        const QList<QJsonObject> providers{
            QJsonObject{{"name", "Unknown"}, {"language", "un"}},
            QJsonObject{{"name", "Adult"}, {"language", "en"},
                        {"supportedTypes", QJsonArray{"Movie", "NSFW"}}},
        };
        QCOMPARE(CloudStream::ProviderPreferenceFilter::apply(
            providers, "All languages", true).size(), 2);
    }
};

QTEST_GUILESS_MAIN(ProviderPreferenceFilterTest)
#include "test_provider_preference_filter.moc"
