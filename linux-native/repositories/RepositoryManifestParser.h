#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace CloudStream {

struct RepositoryManifest {
    QString name;
    QString description;
    int manifestVersion = 0;
    QStringList pluginLists;
};

struct RepositoryIndexEntry {
    QString name;
    QString url;
    bool verified = false;
};

struct PluginInfo {
    QString name;
    QString internalName;
    QString description;
    QString iconUrl;
    QString url;
    QString fileHash;
    QString jarUrl;
    QString jarHash;
    QString language;
    QStringList authors;
    QStringList tvTypes;
    int status = 0;
    int version = 0;
    int apiVersion = 0;
};

class RepositoryManifestParser final {
public:
    static bool parseManifest(const QByteArray &json,
                              RepositoryManifest *result,
                              QString *error);
    static bool parsePluginList(const QByteArray &json,
                                QList<PluginInfo> *result,
                                QString *error);
    static bool parseRepositoryIndex(const QByteArray &json,
                                     QList<RepositoryIndexEntry> *result,
                                     QString *error);
};

} // namespace CloudStream
