#include "../extensions/ExtensionRegistry.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class ExtensionRegistryTest : public QObject {
    Q_OBJECT

private slots:
    void persistsMultipleRepositoriesAndInstalledMetadata() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("extension-registry.json");
        CloudStream::ExtensionRegistry registry(path);

        CloudStream::RepositoryRecord official{"Official", "https://example.org/official.json"};
        CloudStream::RepositoryRecord custom{"Custom", "https://example.org/custom.json"};
        QVERIFY(registry.addRepository(official));
        QVERIFY(registry.addRepository(custom));
        QVERIFY(registry.addRepository(custom));

        CloudStream::ExtensionRecord extension;
        extension.internalName = "AniworldProvider";
        extension.displayName = "Aniworld";
        extension.iconUrl = "https://example.org/aniworld.png";
        extension.repositoryUrl = custom.url;
        extension.artifactPath = directory.filePath("AniworldProvider.cs3");
        extension.sourceArtifactPath = extension.artifactPath;
        extension.platform = "android";
        extension.converterId = "none";
        extension.version = 25;
        extension.language = "de";
        extension.tvTypes = {"Anime", "OVA"};
        extension.sha256 = "sha256-test";
        extension.enabled = false;
        QVERIFY(registry.upsertExtension(extension));

        CloudStream::ExtensionRegistry reopened(path);
        const auto repositories = reopened.repositories();
        QCOMPARE(repositories.size(), 2);
        QCOMPARE(repositories[0].name, QString("Official"));
        QCOMPARE(repositories[1].url, custom.url);
        const auto extensions = reopened.extensions();
        QCOMPARE(extensions.size(), 1);
        QCOMPARE(extensions[0].internalName, QString("AniworldProvider"));
        QCOMPARE(extensions[0].displayName, QString("Aniworld"));
        QCOMPARE(extensions[0].iconUrl, extension.iconUrl);
        QCOMPARE(extensions[0].repositoryUrl, custom.url);
        QCOMPARE(extensions[0].sourceArtifactPath, extension.sourceArtifactPath);
        QCOMPARE(extensions[0].platform, QString("android"));
        QCOMPARE(extensions[0].converterId, QString("none"));
        QCOMPARE(extensions[0].version, 25);
        QCOMPARE(extensions[0].language, QString("de"));
        QCOMPARE(extensions[0].tvTypes, QStringList({"Anime", "OVA"}));
        QVERIFY(!extensions[0].enabled);

        QVERIFY(reopened.removeRepository(custom.url));
        QCOMPARE(reopened.repositories().size(), 1);
        QCOMPARE(reopened.repositories().first().url, official.url);
        QVERIFY(reopened.extensions().isEmpty());
    }

    void migratesLegacyArtifactsAndControlsInstalledState() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto extensionsDir = directory.filePath("extensions");
        QVERIFY(QDir().mkpath(extensionsDir));
        QFile jar(extensionsDir + "/DailymotionProvider.jar");
        QVERIFY(jar.open(QIODevice::WriteOnly));
        QVERIFY(jar.write("jar") > 0);
        jar.close();
        QFile cs3(extensionsDir + "/AniworldProvider.cs3");
        QVERIFY(cs3.open(QIODevice::WriteOnly));
        QVERIFY(cs3.write("dex") > 0);
        cs3.close();

        CloudStream::ExtensionRegistry registry(directory.filePath("extension-registry.json"));
        QVERIFY(registry.synchronizeArtifacts(extensionsDir));
        QVERIFY(registry.synchronizeArtifacts(extensionsDir));
        auto extensions = registry.extensions();
        QCOMPARE(extensions.size(), 2);
        const auto jvm = std::find_if(extensions.begin(), extensions.end(), [](const auto &entry) {
            return entry.internalName == "DailymotionProvider";
        });
        const auto android = std::find_if(extensions.begin(), extensions.end(), [](const auto &entry) {
            return entry.internalName == "AniworldProvider";
        });
        QVERIFY(jvm != extensions.end());
        QVERIFY(android != extensions.end());
        QCOMPARE(jvm->platform, QString("jvm"));
        QVERIFY(jvm->enabled);
        QCOMPARE(android->platform, QString("android"));
        QVERIFY(!android->enabled);

        QVERIFY(registry.setExtensionEnabled("DailymotionProvider", {}, false));
        extensions = registry.extensions();
        const auto disabledJvm = std::find_if(extensions.begin(), extensions.end(), [](const auto &entry) {
            return entry.internalName == "DailymotionProvider";
        });
        QVERIFY(disabledJvm != extensions.end());
        QVERIFY(!disabledJvm->enabled);
        QVERIFY(!registry.setExtensionEnabled("AniworldProvider", {}, true));
        auto currentEntries = registry.extensions();
        const auto currentAndroid = std::find_if(currentEntries.begin(), currentEntries.end(), [](const auto &entry) {
            return entry.internalName == "AniworldProvider";
        });
        QVERIFY(currentAndroid != currentEntries.end());
        auto converted = *currentAndroid;
        converted.artifactPath = extensionsDir + "/AniworldProvider-converted.jar";
        converted.sourceArtifactPath = cs3.fileName();
        converted.platform = "jvm-converted";
        converted.converterId = "dex2jar-2.4.38-preserve-names";
        converted.enabled = false;
        QVERIFY(registry.upsertExtension(converted));
        QVERIFY(registry.setExtensionEnabled("AniworldProvider", {}, true));
        QVERIFY(registry.synchronizeArtifacts(extensionsDir));
        QCOMPARE(registry.extensions().size(), 2);
        const auto convertedEntries = registry.extensions();
        const auto convertedAndroid = std::find_if(convertedEntries.begin(), convertedEntries.end(), [](const auto &entry) {
            return entry.internalName == "AniworldProvider";
        });
        QVERIFY(convertedAndroid != convertedEntries.end());
        QCOMPARE(convertedAndroid->converterId, QString("dex2jar-2.4.38-preserve-names"));
        QVERIFY(registry.removeExtension("AniworldProvider", {}));
        QCOMPARE(registry.extensions().size(), 1);
    }
};

QTEST_APPLESS_MAIN(ExtensionRegistryTest)
#include "test_extension_registry.moc"
