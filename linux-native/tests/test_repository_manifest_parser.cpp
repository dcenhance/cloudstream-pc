#include "../repositories/RepositoryManifestParser.h"

#include <QtTest>

class RepositoryManifestParserTest : public QObject {
    Q_OBJECT

private slots:
    void parsesRepositoryManifest() {
        const QByteArray json =
            "{\"name\":\"Cloudstream providers repository\","
            "\"description\":\"Cloudstream extension Repository\","
            "\"manifestVersion\":1,"
            "\"pluginLists\":[\"https://example.org/plugins.json\"]}";
        CloudStream::RepositoryManifest result;
        QString error;
        QVERIFY2(CloudStream::RepositoryManifestParser::parseManifest(json, &result, &error), qPrintable(error));
        QCOMPARE(result.name, QString("Cloudstream providers repository"));
        QCOMPARE(result.manifestVersion, 1);
        QCOMPARE(result.pluginLists, QStringList{"https://example.org/plugins.json"});
    }

    void rejectsMalformedManifest() {
        CloudStream::RepositoryManifest result;
        QString error;
        QVERIFY(!CloudStream::RepositoryManifestParser::parseManifest("{", &result, &error));
        QVERIFY(error.startsWith("Invalid JSON:"));
        QVERIFY(!CloudStream::RepositoryManifestParser::parseManifest("{\"name\":\"Empty\",\"manifestVersion\":1}", &result, &error));
        QCOMPARE(error, QString("Repository contains no plugin lists"));
    }

    void parsesPluginsAndSkipsUnusableEntries() {
        const QByteArray json =
            "[{\"name\":\"DailymotionProvider\","
            "\"internalName\":\"DailymotionProvider\","
            "\"url\":\"https://example.org/Dailymotion.cs3\","
            "\"iconUrl\":\"https://example.org/icon-%size%.png\","
            "\"fileHash\":\"sha256-abcd\",\"status\":1,"
            "\"version\":4,\"apiVersion\":1,"
            "\"authors\":[\"Luna\"],\"tvTypes\":[\"Others\"]},"
            "{\"name\":\"Missing URL\",\"internalName\":\"Broken\"},"
            "\"not-an-object\"]";
        QList<CloudStream::PluginInfo> plugins;
        QString error;
        QVERIFY2(CloudStream::RepositoryManifestParser::parsePluginList(json, &plugins, &error), qPrintable(error));
        QCOMPARE(plugins.size(), 1);
        QCOMPARE(plugins.first().internalName, QString("DailymotionProvider"));
        QCOMPARE(plugins.first().iconUrl, QString("https://example.org/icon-%size%.png"));
        QCOMPARE(plugins.first().authors, QStringList{"Luna"});
        QCOMPARE(plugins.first().status, 1);
    }

    void rejectsEmptyPluginList() {
        QList<CloudStream::PluginInfo> plugins;
        QString error;
        QVERIFY(!CloudStream::RepositoryManifestParser::parsePluginList("[]", &plugins, &error));
        QCOMPARE(error, QString("Plugin list contains no usable entries"));
    }

    void parsesRepositoryIndexAndDeduplicatesUrls() {
        const QByteArray json =
            "[{\"url\":\"https://example.org/official/repo.json\",\"verified\":true,\"name\":\"Official\"},"
            "\"https://example.org/community/repo.json\","
            "{\"url\":\"https://example.org/official/repo.json\",\"verified\":false}]";
        QList<CloudStream::RepositoryIndexEntry> repositories;
        QString error;
        QVERIFY2(CloudStream::RepositoryManifestParser::parseRepositoryIndex(json, &repositories, &error), qPrintable(error));
        QCOMPARE(repositories.size(), 2);
        QCOMPARE(repositories.first().name, QString("Official"));
        QCOMPARE(repositories.first().url, QString("https://example.org/official/repo.json"));
        QVERIFY(repositories.first().verified);
        QCOMPARE(repositories.last().url, QString("https://example.org/community/repo.json"));
        QVERIFY(!repositories.last().verified);
    }

    void rejectsPluginListAsRepositoryIndex() {
        const QByteArray json =
            "[{\"internalName\":\"ExampleProvider\",\"url\":\"https://example.org/Example.cs3\"}]";
        QList<CloudStream::RepositoryIndexEntry> repositories;
        QString error;
        QVERIFY(!CloudStream::RepositoryManifestParser::parseRepositoryIndex(json, &repositories, &error));
        QCOMPARE(error, QString("JSON array is a plugin list, not a repository index"));
    }
};

QTEST_APPLESS_MAIN(RepositoryManifestParserTest)
#include "test_repository_manifest_parser.moc"
