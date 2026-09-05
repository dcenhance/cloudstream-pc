#include "RepositoryManifestParser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>
#include <algorithm>

namespace CloudStream {
namespace {
QStringList strings(const QJsonValue &value) {
    QStringList output;
    for (const auto &item : value.toArray()) {
        if (item.isString() && !item.toString().trimmed().isEmpty())
            output << item.toString().trimmed();
    }
    return output;
}

bool decode(const QByteArray &json, QJsonDocument *document, QString *error) {
    QJsonParseError parseError;
    *document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = "Invalid JSON: " + parseError.errorString();
        return false;
    }
    return true;
}
}

bool RepositoryManifestParser::parseManifest(const QByteArray &json,
                                             RepositoryManifest *result,
                                             QString *error) {
    if (!result) return false;
    QJsonDocument document;
    if (!decode(json, &document, error) || !document.isObject()) {
        if (error && error->isEmpty()) *error = "Repository manifest must be an object";
        return false;
    }
    const auto object = document.object();
    RepositoryManifest parsed;
    parsed.name = object.value("name").toString().trimmed();
    parsed.description = object.value("description").toString().trimmed();
    parsed.manifestVersion = object.value("manifestVersion").toInt();
    parsed.pluginLists = strings(object.value("pluginLists"));
    if (parsed.name.isEmpty()) {
        if (error) *error = "Repository name is missing";
        return false;
    }
    if (parsed.manifestVersion <= 0) {
        if (error) *error = "Unsupported or missing manifest version";
        return false;
    }
    if (parsed.pluginLists.isEmpty()) {
        if (error) *error = "Repository contains no plugin lists";
        return false;
    }
    *result = parsed;
    if (error) error->clear();
    return true;
}

bool RepositoryManifestParser::parsePluginList(const QByteArray &json,
                                               QList<PluginInfo> *result,
                                               QString *error) {
    if (!result) return false;
    QJsonDocument document;
    if (!decode(json, &document, error) || !document.isArray()) {
        if (error && error->isEmpty()) *error = "Plugin list must be an array";
        return false;
    }
    QList<PluginInfo> parsed;
    for (const auto &value : document.array()) {
        if (!value.isObject()) continue;
        const auto object = value.toObject();
        PluginInfo plugin;
        plugin.name = object.value("name").toString().trimmed();
        plugin.internalName = object.value("internalName").toString().trimmed();
        plugin.description = object.value("description").toString().trimmed();
        plugin.iconUrl = object.value("iconUrl").toString().trimmed();
        plugin.url = object.value("url").toString().trimmed();
        plugin.fileHash = object.value("fileHash").toString().trimmed();
        plugin.jarUrl = object.value("jarUrl").toString().trimmed();
        plugin.jarHash = object.value("jarHash").toString().trimmed();
        plugin.language = object.value("language").toString().trimmed();
        plugin.authors = strings(object.value("authors"));
        plugin.tvTypes = strings(object.value("tvTypes"));
        plugin.status = object.value("status").toInt();
        plugin.version = object.value("version").toInt();
        plugin.apiVersion = object.value("apiVersion").toInt();
        if (plugin.name.isEmpty()) plugin.name = plugin.internalName;
        if (plugin.internalName.isEmpty() || plugin.url.isEmpty()) continue;
        parsed << plugin;
    }
    if (parsed.isEmpty()) {
        if (error) *error = "Plugin list contains no usable entries";
        return false;
    }
    *result = parsed;
    if (error) error->clear();
    return true;
}

bool RepositoryManifestParser::parseRepositoryIndex(const QByteArray &json,
                                                    QList<RepositoryIndexEntry> *result,
                                                    QString *error) {
    if (!result) return false;
    QJsonDocument document;
    if (!decode(json, &document, error) || !document.isArray()) {
        if (error && error->isEmpty()) *error = "Repository index must be an array";
        return false;
    }
    QList<RepositoryIndexEntry> parsed;
    bool lookedLikePluginList = false;
    for (const auto &value : document.array()) {
        RepositoryIndexEntry entry;
        if (value.isString()) {
            entry.url = value.toString().trimmed();
        } else if (value.isObject()) {
            const auto object = value.toObject();
            lookedLikePluginList = lookedLikePluginList || object.contains("internalName");
            entry.name = object.value("name").toString().trimmed();
            entry.url = object.value("url").toString().trimmed();
            entry.verified = object.value("verified").toBool(false);
        }
        const QUrl url(entry.url);
        if (!url.isValid() || url.host().isEmpty() ||
            (url.scheme().compare("https", Qt::CaseInsensitive) != 0 &&
             url.scheme().compare("http", Qt::CaseInsensitive) != 0)) continue;
        auto existing = std::find_if(parsed.begin(), parsed.end(), [&entry](const auto &candidate) {
            return candidate.url == entry.url;
        });
        if (existing == parsed.end()) {
            parsed << entry;
        } else {
            if (existing->name.isEmpty() && !entry.name.isEmpty()) existing->name = entry.name;
            existing->verified = existing->verified || entry.verified;
        }
    }
    if (lookedLikePluginList) {
        if (error) *error = "JSON array is a plugin list, not a repository index";
        return false;
    }
    if (parsed.isEmpty()) {
        if (error) *error = "Repository index contains no usable repository URLs";
        return false;
    }
    *result = parsed;
    if (error) error->clear();
    return true;
}

} // namespace CloudStream
