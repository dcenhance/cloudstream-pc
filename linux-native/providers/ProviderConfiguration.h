#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace CloudStream {

enum class ProviderConfigurationKind {
    Playlist,
    MonPlayer,
    Xtream,
    Stremio,
    StremioSection,
};

struct ProviderConfigurationSpec {
    ProviderConfigurationKind kind;
    QString storeName;
    QString preferenceKey;
    QString primaryUrlLabel;
    QString secondaryUrlLabel;
    QStringList typeOptions;
};

struct ProviderConfigurationInput {
    QString name;
    QString primaryUrl;
    QString secondaryUrl;
    QString username;
    QString password;
    QString type;
};

class ProviderConfiguration final {
public:
    static std::optional<ProviderConfigurationSpec> specFor(const QString &internalName);
    static QJsonObject sidecar(const QString &internalName,
                               const QString &repositoryUrl,
                               int version,
                               const ProviderConfigurationInput &input,
                               QString *error = nullptr);
    static std::optional<ProviderConfigurationInput> inputFromSidecar(
        const QString &internalName, const QJsonObject &sidecar,
        QString *error = nullptr);
    static bool writeSidecar(const QString &artifactPath, const QJsonObject &sidecar,
                             QString *error = nullptr);
    static QString settingsPath(const QString &artifactPath);
};

} // namespace CloudStream
