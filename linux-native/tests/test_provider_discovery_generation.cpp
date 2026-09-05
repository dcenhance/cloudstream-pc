#include "../providers/ProviderDiscoveryGeneration.h"

#include <QtTest>

class ProviderDiscoveryGenerationTest : public QObject {
    Q_OBJECT

private slots:
    void rejectsResultsFromOlderRefreshes() {
        CloudStream::ProviderDiscoveryGeneration discovery;

        const auto first = discovery.begin();
        const auto second = discovery.begin();

        QVERIFY(!discovery.isCurrent(first));
        QVERIFY(discovery.isCurrent(second));
    }
};

QTEST_MAIN(ProviderDiscoveryGenerationTest)
#include "test_provider_discovery_generation.moc"
