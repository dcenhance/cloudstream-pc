#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace CloudStream {

struct RepositoryRecord {
    QString name;
    QString url;
};

struct ExtensionRecord {
    QString internalName;
    QString displayName;
    QString iconUrl;
    QString repositoryUrl;
    QString artifactPath;
    QString sourceArtifactPath;
    QString platform;
    QString converterId;
    int version = 0;
    QString language;
    QStringList tvTypes;
    QString sha256;
    bool enabled = true;
};

class ExtensionRegistry final {
public:
    explicit ExtensionRegistry(QString filePath);

    QList<RepositoryRecord> repositories() const;
    QList<ExtensionRecord> extensions() const;
    bool addRepository(const RepositoryRecord &repository);
    bool removeRepository(const QString &repositoryUrl);
    bool upsertExtension(const ExtensionRecord &extension);
    bool synchronizeArtifacts(const QString &extensionsDirectory);
    bool setExtensionEnabled(const QString &internalName,
                             const QString &repositoryUrl, bool enabled);
    bool removeExtension(const QString &internalName,
                         const QString &repositoryUrl);

private:
    QString filePath;
    bool write(const QList<RepositoryRecord> &repositories,
               const QList<ExtensionRecord> &extensions) const;
};

} // namespace CloudStream
