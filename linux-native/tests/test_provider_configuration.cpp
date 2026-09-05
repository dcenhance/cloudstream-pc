#include "../providers/ProviderConfiguration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class ProviderConfigurationTest : public QObject {
    Q_OBJECT

private:
    static QJsonObject storedEntry(const QJsonObject &sidecar, const QString &store, const QString &key) {
        const auto encoded = sidecar.value(store).toObject().value(key).toString().toUtf8();
        return QJsonDocument::fromJson(encoded).array().first().toObject();
    }

private slots:
    void buildsPlaylistSidecar() {
        CloudStream::ProviderConfigurationInput input{"Living room", "https://example.org/live.m3u", {}, {}, {}, {}};
        QString error;
        const auto sidecar = CloudStream::ProviderConfiguration::sidecar(
            "IPTVProvider", "https://example.org/repo.json", 9, input, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(sidecar.value("_plugin").toObject().value("internalName").toString(), QString("IPTVProvider"));
        const auto entry = storedEntry(sidecar, "rebuild_preference", "iptv_links");
        QCOMPARE(entry.value("name").toString(), input.name);
        QCOMPARE(entry.value("link").toString(), input.primaryUrl);
    }

    void buildsMonPlayerSidecar() {
        CloudStream::ProviderConfigurationInput input{
            "Movies", "https://example.org", "https://example.org/search?q=", {}, {}, "Movie"};
        QString error;
        const auto sidecar = CloudStream::ProviderConfiguration::sidecar(
            "MonPlayerProvider", "https://example.org/repo.json", 9, input, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto entry = storedEntry(sidecar, "rebuild_preference", "monplayer_links");
        QCOMPARE(entry.value("mainUrl").toString(), input.primaryUrl);
        QCOMPARE(entry.value("searchUrl").toString(), input.secondaryUrl);
        QCOMPARE(entry.value("type").toString(), QString("Movie"));
    }

    void buildsXtreamSidecar() {
        CloudStream::ProviderConfigurationInput input{
            "TV", "https://example.org", {}, "alice", "secret", {}};
        QString error;
        const auto sidecar = CloudStream::ProviderConfiguration::sidecar(
            "XtreamIPTVProvider", "https://example.org/repo.json", 1, input, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto entry = storedEntry(sidecar, "rebuild_preference", "xtream_iptv_links");
        QCOMPARE(entry.value("username").toString(), QString("alice"));
        QCOMPARE(entry.value("password").toString(), QString("secret"));
    }

    void buildsBothStremioFormats() {
        CloudStream::ProviderConfigurationInput direct{
            "Cinemeta", "https://v3-cinemeta.strem.io/manifest.json", {}, {}, {}, "StremioC"};
        QString error;
        auto sidecar = CloudStream::ProviderConfiguration::sidecar(
            "StremioProvider", "https://example.org/repo.json", 7, direct, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(storedEntry(sidecar, "rebuild_preference", "stremio_links").value("type").toString(),
                 QString("StremioC"));

        CloudStream::ProviderConfigurationInput section{
            "Italian", "https://catalog.example.org/manifest.json",
            "https://streams.example.org/manifest.json", {}, {}, {}};
        sidecar = CloudStream::ProviderConfiguration::sidecar(
            "Stremio", "https://example.org/repo.json", 14, section, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const auto entry = storedEntry(sidecar, "StremioX", "stremio_sections");
        QCOMPARE(entry.value("catalogUrl").toString(), section.primaryUrl);
        QCOMPARE(entry.value("streamAddons").toArray().first().toObject().value("url").toString(), section.secondaryUrl);
    }

    void rejectsUnsupportedAndInvalidConfiguration() {
        CloudStream::ProviderConfigurationInput input{"Broken", "not a url", {}, {}, {}, {}};
        QString error;
        QVERIFY(CloudStream::ProviderConfiguration::sidecar("Unknown", {}, 0, input, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(CloudStream::ProviderConfiguration::sidecar("IPTV", {}, 0, input, &error).isEmpty());
        QVERIFY(error.contains("HTTP", Qt::CaseInsensitive));
        input.primaryUrl.clear();
        error.clear();
        QVERIFY(CloudStream::ProviderConfiguration::sidecar("IPTV", {}, 0, input, &error).isEmpty());
        QVERIFY(error.contains("required", Qt::CaseInsensitive));
    }

    void decodesExistingConfigurationForEditing() {
        const CloudStream::ProviderConfigurationInput expected{
            "Living room", "https://tv.example.org", {}, "alice", "secret", {}};
        QString error;
        const auto sidecar = CloudStream::ProviderConfiguration::sidecar(
            "XtreamIPTVProvider", "https://example.org/repo.json", 4, expected, &error);
        const auto decoded = CloudStream::ProviderConfiguration::inputFromSidecar(
            "XtreamIPTVProvider", sidecar, &error);
        QVERIFY2(decoded.has_value(), qPrintable(error));
        QCOMPARE(decoded->name, expected.name);
        QCOMPARE(decoded->primaryUrl, expected.primaryUrl);
        QCOMPARE(decoded->username, expected.username);
        QCOMPARE(decoded->password, expected.password);
    }

    void writesOwnerOnlySidecarAtomically() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto artifact = directory.filePath("Provider.jar");
        const QJsonObject sidecar{{"test", QJsonObject{{"value", 1}}}};
        QString error;
        QVERIFY2(CloudStream::ProviderConfiguration::writeSidecar(artifact, sidecar, &error), qPrintable(error));
        const auto path = CloudStream::ProviderConfiguration::settingsPath(artifact);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(), sidecar);
        const auto permissions = QFileInfo(path).permissions();
        QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
        QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                                 QFileDevice::ReadOther | QFileDevice::WriteOther)));
    }
};

QTEST_MAIN(ProviderConfigurationTest)
#include "test_provider_configuration.moc"
