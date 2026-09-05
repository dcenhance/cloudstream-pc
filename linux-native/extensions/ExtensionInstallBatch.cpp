#include "ExtensionInstallBatch.h"

#include <QFileInfo>
#include <QSet>

namespace CloudStream {

QList<PluginInfo> ExtensionInstallBatch::pending(
        const QList<PluginInfo> &catalog,
        const QList<ExtensionRecord> &installed,
        const QString &repositoryUrl) {
    QList<PluginInfo> result;
    QSet<QString> seen;
    for (const auto &plugin : catalog) {
        if (plugin.internalName.trimmed().isEmpty() ||
            (plugin.url.trimmed().isEmpty() && plugin.jarUrl.trimmed().isEmpty())) continue;
        const auto key = plugin.internalName + "\n" +
            (plugin.jarUrl.isEmpty() ? plugin.url : plugin.jarUrl);
        if (seen.contains(key)) continue;
        seen.insert(key);
        bool current = false;
        for (const auto &record : installed) {
            if (record.internalName != plugin.internalName ||
                record.repositoryUrl != repositoryUrl) continue;
            if (QFileInfo::exists(record.artifactPath) && record.version >= plugin.version) {
                current = true;
                break;
            }
        }
        if (!current) result << plugin;
    }
    return result;
}

} // namespace CloudStream
