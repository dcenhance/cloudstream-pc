#pragma once

#include "ExtensionRegistry.h"
#include "../repositories/RepositoryManifestParser.h"

namespace CloudStream {

class ExtensionInstallBatch final {
public:
    static QList<PluginInfo> pending(const QList<PluginInfo> &catalog,
                                     const QList<ExtensionRecord> &installed,
                                     const QString &repositoryUrl);
};

} // namespace CloudStream
