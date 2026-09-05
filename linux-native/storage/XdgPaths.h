#pragma once

#include <QString>

namespace CloudStream {

class XdgPaths final {
public:
    static QString configDir();
    static QString dataDir();
    static QString cacheDir();
    static QString extensionsDir();
    static QString logFile();
    static bool ensureDirectories();
};

} // namespace CloudStream
