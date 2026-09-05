#include "../providers/ProviderSelectionModel.h"

#include <QtTest>

class ProviderSelectionModelTest : public QObject {
    Q_OBJECT

private:
    static QJsonObject provider(const QString &jar, const QString &name, bool hasHome) {
        return {{"jarPath", jar}, {"name", name}, {"hasMainPage", hasHome}};
    }

private slots:
    void formatsProviderNamesWithLanguageFlags() {
        auto german = provider("de.jar", "AniWorld", true);
        german.insert("language", "de");
        auto unknown = provider("unknown.jar", "Mystery", true);
        unknown.insert("language", "xx");

        QCOMPARE(CloudStream::ProviderSelectionModel::languageFlag("de"), QString("🇩🇪"));
        QCOMPARE(CloudStream::ProviderSelectionModel::displayName(german), QString("🇩🇪 AniWorld"));
        QCOMPARE(CloudStream::ProviderSelectionModel::displayName(unknown), QString("XX Mystery"));
    }

    void separatesHomeCandidatesFromSearchCandidates() {
        const QList<QJsonObject> providers{
            provider("one.jar", "Home A", true),
            provider("two.jar", "Search only", false),
            provider("three.jar", "Home B", true),
        };

        const auto home = CloudStream::ProviderSelectionModel::homeCandidates(providers);
        QCOMPARE(home.size(), 2);
        QCOMPARE(home[0].value("name").toString(), QString("Home A"));
        QCOMPARE(home[1].value("name").toString(), QString("Home B"));
        QCOMPARE(CloudStream::ProviderSelectionModel::searchCandidates(providers, {}).size(), 3);
    }

    void restoresAnIndependentMultiSelectionByStableKey() {
        const QList<QJsonObject> providers{
            provider("one.jar", "First", true),
            provider("two.jar", "Second", true),
            provider("three.jar", "Third", false),
        };
        const QStringList selected{
            CloudStream::ProviderSelectionModel::key(providers[0]),
            CloudStream::ProviderSelectionModel::key(providers[2]),
        };

        const auto restored = CloudStream::ProviderSelectionModel::searchCandidates(providers, selected);
        QCOMPARE(restored.size(), 2);
        QCOMPARE(restored[0].value("name").toString(), QString("First"));
        QCOMPARE(restored[1].value("name").toString(), QString("Third"));
    }

    void doesNotReloadHomeForTheSameActiveProvider() {
        const auto selected = provider("one.jar", "Home A", true);
        const auto key = CloudStream::ProviderSelectionModel::key(selected);

        QVERIFY(!CloudStream::ProviderSelectionModel::shouldReloadHome(key, selected));
        QVERIFY(CloudStream::ProviderSelectionModel::shouldReloadHome({}, selected));
        QVERIFY(CloudStream::ProviderSelectionModel::shouldReloadHome(
            key, provider("one-v2.jar", "Home A", true)));
    }

    void nsfwPreferenceControlsHomeAndSearchChoices() {
        auto safe = provider("safe.jar", "Safe", true);
        safe.insert("language", "en");
        auto adult = provider("adult.jar", "Adult", true);
        adult.insert("language", "en");
        adult.insert("supportedTypes", QJsonArray{"NSFW"});
        const QList<QJsonObject> discovered{safe, adult};

        const auto hiddenHome = CloudStream::ProviderSelectionModel::selectableHomeCandidates(
            discovered, "All languages", false);
        QCOMPARE(hiddenHome.size(), 1);
        const auto visibleHome = CloudStream::ProviderSelectionModel::selectableHomeCandidates(
            discovered, "All languages", true);
        QCOMPARE(visibleHome.size(), 2);

        const auto automatic = CloudStream::ProviderSelectionModel::automaticHomeCandidates(
            discovered, "All languages", false);
        QCOMPARE(automatic.size(), 1);
        QCOMPARE(automatic.first().value("name").toString(), QString("Safe"));

        const auto defaultSearch = CloudStream::ProviderSelectionModel::effectiveSearchCandidates(
            discovered, {}, "All languages", false);
        QCOMPARE(defaultSearch.size(), 1);
        const auto explicitSearch = CloudStream::ProviderSelectionModel::effectiveSearchCandidates(
            discovered, {CloudStream::ProviderSelectionModel::key(adult)}, "All languages", false);
        QVERIFY(explicitSearch.isEmpty());
        const auto enabledSearch = CloudStream::ProviderSelectionModel::effectiveSearchCandidates(
            discovered, {CloudStream::ProviderSelectionModel::key(adult)}, "All languages", true);
        QCOMPARE(enabledSearch.size(), 1);
        QCOMPARE(enabledSearch.first().value("name").toString(), QString("Adult"));
    }

    void mergesNewlyValidatedProvidersWithoutAFullRescan() {
        QJsonObject stale = provider("old.jar", "Old", true);
        stale.insert("mode", "provider");
        stale.insert("extensionName", "UpdatedExtension");
        QJsonObject other = provider("other.jar", "Other", true);
        other.insert("mode", "provider");
        other.insert("extensionName", "OtherExtension");

        const auto merged = CloudStream::ProviderSelectionModel::mergeValidatedProviders(
            {stale, other}, "UpdatedExtension", "new.jar",
            QJsonArray{QJsonObject{{"name", "New"}, {"hasMainPage", true}}});

        QCOMPARE(merged.size(), 2);
        QCOMPARE(merged[0].value("name").toString(), QString("Other"));
        QCOMPARE(merged[1].value("name").toString(), QString("New"));
        QCOMPARE(merged[1].value("jarPath").toString(), QString("new.jar"));
        QCOMPARE(merged[1].value("mode").toString(), QString("provider"));
        QCOMPARE(merged[1].value("extensionName").toString(), QString("UpdatedExtension"));
    }
};

QTEST_MAIN(ProviderSelectionModelTest)
#include "test_provider_selection_model.moc"
