#include "ExtensionRegistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <utility>

namespace CloudStream {
namespace {
QJsonObject repositoryJson(const RepositoryRecord &repository) {
    return {{"name", repository.name}, {"url", repository.url}};
}

QJsonObject extensionJson(const ExtensionRecord &extension) {
    QJsonArray tvTypes;
    for (const auto &type : extension.tvTypes) tvTypes.append(type);
    return {
        {"internalName", extension.internalName},
        {"displayName", extension.displayName},
        {"iconUrl", extension.iconUrl},
        {"repositoryUrl", extension.repositoryUrl},
        {"artifactPath", extension.artifactPath},
        {"sourceArtifactPath", extension.sourceArtifactPath},
        {"platform", extension.platform},
        {"converterId", extension.converterId},
        {"version", extension.version},
        {"language", extension.language},
        {"tvTypes", tvTypes},
        {"sha256", extension.sha256},
        {"enabled", extension.enabled},
    };
}

QJsonObject registryObject(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto document = QJsonDocument::fromJson(file.readAll());
    return document.isObject() ? document.object() : QJsonObject();
}
} // namespace

ExtensionRegistry::ExtensionRegistry(QString filePath) : filePath(std::move(filePath)) {}

QList<RepositoryRecord> ExtensionRegistry::repositories() const {
    QList<RepositoryRecord> result;
    for (const auto &value : registryObject(filePath).value("repositories").toArray()) {
        const auto object = value.toObject();
        RepositoryRecord repository{object.value("name").toString().trimmed(),
                                    object.value("url").toString().trimmed()};
        if (!repository.url.isEmpty()) result << repository;
    }
    return result;
}

QList<ExtensionRecord> ExtensionRegistry::extensions() const {
    QList<ExtensionRecord> result;
    for (const auto &value : registryObject(filePath).value("extensions").toArray()) {
        const auto object = value.toObject();
        ExtensionRecord extension;
        extension.internalName = object.value("internalName").toString();
        extension.displayName = object.value("displayName").toString();
        extension.iconUrl = object.value("iconUrl").toString();
        extension.repositoryUrl = object.value("repositoryUrl").toString();
        extension.artifactPath = object.value("artifactPath").toString();
        extension.sourceArtifactPath = object.value("sourceArtifactPath").toString();
        extension.platform = object.value("platform").toString();
        extension.converterId = object.value("converterId").toString();
        extension.version = object.value("version").toInt();
        extension.language = object.value("language").toString();
        for (const auto &type : object.value("tvTypes").toArray()) {
            if (type.isString()) extension.tvTypes << type.toString();
        }
        extension.sha256 = object.value("sha256").toString();
        extension.enabled = object.value("enabled").toBool(true);
        if (!extension.internalName.isEmpty() && !extension.artifactPath.isEmpty()) result << extension;
    }
    return result;
}

bool ExtensionRegistry::write(const QList<RepositoryRecord> &repositoryValues,
                              const QList<ExtensionRecord> &extensionValues) const {
    if (!QDir().mkpath(QFileInfo(filePath).absolutePath())) return false;
    QJsonArray repositoriesJson;
    for (const auto &repository : repositoryValues) repositoriesJson.append(repositoryJson(repository));
    QJsonArray extensionsJson;
    for (const auto &extension : extensionValues) extensionsJson.append(extensionJson(extension));
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    const QJsonObject object{{"repositories", repositoriesJson}, {"extensions", extensionsJson}};
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0) return false;
    return file.commit();
}

bool ExtensionRegistry::addRepository(const RepositoryRecord &repository) {
    if (repository.url.trimmed().isEmpty()) return false;
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto existing = std::find_if(repositoryValues.begin(), repositoryValues.end(), [&repository](const auto &value) {
        return value.url == repository.url;
    });
    if (existing == repositoryValues.end()) repositoryValues << repository;
    else if (!repository.name.trimmed().isEmpty()) existing->name = repository.name;
    return write(repositoryValues, extensionValues);
}

bool ExtensionRegistry::removeRepository(const QString &repositoryUrl) {
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto previousSize = repositoryValues.size();
    repositoryValues.erase(std::remove_if(repositoryValues.begin(), repositoryValues.end(), [&repositoryUrl](const auto &repository) {
        return repository.url == repositoryUrl;
    }), repositoryValues.end());
    if (repositoryValues.size() == previousSize) return false;
    extensionValues.erase(std::remove_if(extensionValues.begin(), extensionValues.end(), [&repositoryUrl](const auto &extension) {
        return extension.repositoryUrl == repositoryUrl;
    }), extensionValues.end());
    return write(repositoryValues, extensionValues);
}

bool ExtensionRegistry::upsertExtension(const ExtensionRecord &extension) {
    if (extension.internalName.trimmed().isEmpty() || extension.artifactPath.trimmed().isEmpty()) return false;
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto existing = std::find_if(extensionValues.begin(), extensionValues.end(), [&extension](const auto &value) {
        return value.internalName == extension.internalName && value.repositoryUrl == extension.repositoryUrl;
    });
    if (existing == extensionValues.end()) extensionValues << extension;
    else *existing = extension;
    return write(repositoryValues, extensionValues);
}

bool ExtensionRegistry::synchronizeArtifacts(const QString &extensionsDirectory) {
    QDir directory(extensionsDirectory);
    if (!directory.exists() && !QDir().mkpath(extensionsDirectory)) return false;
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto absoluteDirectory = QFileInfo(extensionsDirectory).absoluteFilePath();
    extensionValues.erase(std::remove_if(extensionValues.begin(), extensionValues.end(), [&absoluteDirectory](const auto &extension) {
        const QFileInfo artifact(extension.artifactPath);
        return artifact.absolutePath() == absoluteDirectory && !artifact.exists() &&
            (extension.sourceArtifactPath.isEmpty() || !QFileInfo::exists(extension.sourceArtifactPath));
    }), extensionValues.end());
    const auto artifacts = directory.entryInfoList({"*.jar", "*.cs3"}, QDir::Files, QDir::Name);
    for (const auto &artifact : artifacts) {
        const auto path = artifact.absoluteFilePath();
        const auto existing = std::find_if(extensionValues.begin(), extensionValues.end(), [&path](const auto &extension) {
            return QFileInfo(extension.artifactPath).absoluteFilePath() == path ||
                (!extension.sourceArtifactPath.isEmpty() && QFileInfo(extension.sourceArtifactPath).absoluteFilePath() == path);
        });
        if (existing != extensionValues.end()) continue;
        ExtensionRecord extension;
        extension.internalName = artifact.completeBaseName();
        extension.displayName = extension.internalName;
        if (extension.displayName.endsWith("Provider")) extension.displayName.chop(8);
        extension.artifactPath = path;
        extension.platform = artifact.suffix().compare("jar", Qt::CaseInsensitive) == 0 ? "jvm" : "android";
        extension.sourceArtifactPath = extension.platform == "android" ? path : QString();
        extension.enabled = extension.platform == "jvm";
        extensionValues << extension;
    }
    return write(repositoryValues, extensionValues);
}

bool ExtensionRegistry::setExtensionEnabled(const QString &internalName,
                                            const QString &repositoryUrl, bool enabled) {
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto existing = std::find_if(extensionValues.begin(), extensionValues.end(), [&](const auto &extension) {
        return extension.internalName == internalName && extension.repositoryUrl == repositoryUrl;
    });
    if (existing == extensionValues.end()) return false;
    if (enabled && !existing->platform.startsWith("jvm")) return false;
    existing->enabled = enabled;
    return write(repositoryValues, extensionValues);
}

bool ExtensionRegistry::removeExtension(const QString &internalName,
                                        const QString &repositoryUrl) {
    auto repositoryValues = repositories();
    auto extensionValues = extensions();
    const auto previousSize = extensionValues.size();
    extensionValues.erase(std::remove_if(extensionValues.begin(), extensionValues.end(), [&](const auto &extension) {
        return extension.internalName == internalName && extension.repositoryUrl == repositoryUrl;
    }), extensionValues.end());
    if (extensionValues.size() == previousSize) return false;
    return write(repositoryValues, extensionValues);
}

} // namespace CloudStream
