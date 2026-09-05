#include "ProviderConfiguration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>

namespace CloudStream {
namespace {
bool validHttpUrl(const QString &value) {
    const QUrl url(value.trimmed());
    return url.isValid() && (url.scheme() == "http" || url.scheme() == "https") && !url.host().isEmpty();
}

QString compactArray(const QJsonObject &entry) {
    return QString::fromUtf8(QJsonDocument(QJsonArray{entry}).toJson(QJsonDocument::Compact));
}
} // namespace

std::optional<ProviderConfigurationSpec> ProviderConfiguration::specFor(const QString &internalName) {
    if (internalName == "IPTV" || internalName == "IPTVProvider") {
        return ProviderConfigurationSpec{
            ProviderConfigurationKind::Playlist, "rebuild_preference", "iptv_links", "Playlist URL", {}, {}};
    }
    if (internalName == "MonPlayerProvider") {
        return ProviderConfigurationSpec{
            ProviderConfigurationKind::MonPlayer, "rebuild_preference", "monplayer_links", "Base URL",
            "Search URL", {"Live", "Movie", "NSFW"}};
    }
    if (internalName == "XtreamIPTVProvider") {
        return ProviderConfigurationSpec{
            ProviderConfigurationKind::Xtream, "rebuild_preference", "xtream_iptv_links", "Server URL", {}, {}};
    }
    if (internalName == "StremioProvider") {
        return ProviderConfigurationSpec{
            ProviderConfigurationKind::Stremio, "rebuild_preference", "stremio_links", "Manifest URL", {},
            {"StremioC", "StremioX"}};
    }
    if (internalName == "Stremio") {
        return ProviderConfigurationSpec{
            ProviderConfigurationKind::StremioSection, "StremioX", "stremio_sections", "Catalog manifest URL",
            "Stream add-on manifest URL", {}};
    }
    return std::nullopt;
}

QJsonObject ProviderConfiguration::sidecar(const QString &internalName,
                                            const QString &repositoryUrl,
                                            int version,
                                            const ProviderConfigurationInput &input,
                                            QString *error) {
    if (error) error->clear();
    const auto spec = specFor(internalName);
    if (!spec) {
        if (error) *error = "This extension does not expose a supported native configuration schema";
        return {};
    }
    if (input.name.trimmed().isEmpty()) {
        if (error) *error = "A provider name is required";
        return {};
    }
    if (input.primaryUrl.trimmed().isEmpty()) {
        if (error) *error = spec->primaryUrlLabel + " is required. Enter a valid HTTP or HTTPS URL";
        return {};
    }
    if (!validHttpUrl(input.primaryUrl)) {
        if (error) *error = spec->primaryUrlLabel + " must be a valid HTTP or HTTPS URL";
        return {};
    }
    if (!input.secondaryUrl.trimmed().isEmpty() && !validHttpUrl(input.secondaryUrl)) {
        if (error) *error = spec->secondaryUrlLabel + " must be a valid HTTP or HTTPS URL";
        return {};
    }
    if (spec->kind == ProviderConfigurationKind::Xtream &&
        (input.username.isEmpty() || input.password.isEmpty())) {
        if (error) *error = "Xtream username and password are required";
        return {};
    }

    QJsonObject entry{{"name", input.name.trimmed()}};
    switch (spec->kind) {
    case ProviderConfigurationKind::Playlist:
        entry.insert("link", input.primaryUrl.trimmed());
        break;
    case ProviderConfigurationKind::MonPlayer:
        entry.insert("mainUrl", input.primaryUrl.trimmed());
        entry.insert("searchUrl", input.secondaryUrl.trimmed());
        entry.insert("type", spec->typeOptions.contains(input.type) ? input.type : spec->typeOptions.first());
        break;
    case ProviderConfigurationKind::Xtream:
        entry.insert("mainUrl", input.primaryUrl.trimmed());
        entry.insert("username", input.username);
        entry.insert("password", input.password);
        break;
    case ProviderConfigurationKind::Stremio:
        entry.insert("mainUrl", input.primaryUrl.trimmed());
        entry.insert("type", spec->typeOptions.contains(input.type) ? input.type : spec->typeOptions.first());
        break;
    case ProviderConfigurationKind::StremioSection: {
        entry.insert("id", 1);
        entry.insert("catalogUrl", input.primaryUrl.trimmed());
        QJsonArray streamAddons;
        if (!input.secondaryUrl.trimmed().isEmpty()) {
            streamAddons.append(QJsonObject{
                {"id", 1}, {"name", input.name.trimmed() + " streams"},
                {"url", input.secondaryUrl.trimmed()}, {"type", "https"},
            });
        }
        entry.insert("streamAddons", streamAddons);
        break;
    }
    }
    const QJsonObject plugin{
        {"internalName", internalName}, {"url", repositoryUrl}, {"version", version},
    };
    const QJsonObject store{{spec->preferenceKey, compactArray(entry)}};
    return QJsonObject{{"_plugin", plugin}, {spec->storeName, store}};
}

std::optional<ProviderConfigurationInput> ProviderConfiguration::inputFromSidecar(
        const QString &internalName, const QJsonObject &sidecar, QString *error) {
    if (error) error->clear();
    const auto spec = specFor(internalName);
    if (!spec) {
        if (error) *error = "This extension does not expose a supported native configuration schema";
        return std::nullopt;
    }
    const auto encoded = sidecar.value(spec->storeName).toObject()
        .value(spec->preferenceKey).toString().toUtf8();
    const auto document = QJsonDocument::fromJson(encoded);
    if (!document.isArray() || document.array().isEmpty() || !document.array().first().isObject()) {
        if (error) *error = "The existing provider settings are malformed";
        return std::nullopt;
    }
    const auto entry = document.array().first().toObject();
    ProviderConfigurationInput input;
    input.name = entry.value("name").toString();
    switch (spec->kind) {
    case ProviderConfigurationKind::Playlist:
        input.primaryUrl = entry.value("link").toString();
        break;
    case ProviderConfigurationKind::MonPlayer:
        input.primaryUrl = entry.value("mainUrl").toString();
        input.secondaryUrl = entry.value("searchUrl").toString();
        input.type = entry.value("type").toString();
        break;
    case ProviderConfigurationKind::Xtream:
        input.primaryUrl = entry.value("mainUrl").toString();
        input.username = entry.value("username").toString();
        input.password = entry.value("password").toString();
        break;
    case ProviderConfigurationKind::Stremio:
        input.primaryUrl = entry.value("mainUrl").toString();
        input.type = entry.value("type").toString();
        break;
    case ProviderConfigurationKind::StremioSection: {
        input.primaryUrl = entry.value("catalogUrl").toString();
        const auto addons = entry.value("streamAddons").toArray();
        if (!addons.isEmpty()) input.secondaryUrl = addons.first().toObject().value("url").toString();
        break;
    }
    }
    return input;
}

bool ProviderConfiguration::writeSidecar(const QString &artifactPath,
                                         const QJsonObject &sidecar, QString *error) {
    if (error) error->clear();
    if (artifactPath.trimmed().isEmpty() || sidecar.isEmpty()) {
        if (error) *error = "Provider settings cannot be empty";
        return false;
    }
    const auto path = settingsPath(artifactPath);
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        if (error) *error = "Could not create the provider settings directory";
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    const auto bytes = QJsonDocument(sidecar).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        if (error) *error = file.errorString();
        file.cancelWriting();
        return false;
    }
    const auto ownerOnly = QFileDevice::ReadOwner | QFileDevice::WriteOwner;
    file.setPermissions(ownerOnly);
    if (!file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    if (!QFile::setPermissions(path, ownerOnly)) {
        if (error) *error = "Could not restrict provider settings permissions";
        return false;
    }
    return true;
}

QString ProviderConfiguration::settingsPath(const QString &artifactPath) {
    return artifactPath + ".settings.json";
}

} // namespace CloudStream
