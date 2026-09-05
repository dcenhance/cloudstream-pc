#pragma once

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QString>
#ifdef Q_OS_WIN
#include <cstdlib>
#endif

namespace CloudStream {

inline QProcessEnvironment packagedRuntimeEnvironment(const QString &applicationDirectory,
    QProcessEnvironment environment, bool windows) {
    if (!windows) return environment;
    const QDir root(applicationDirectory);
    const auto useDefault = [&](const QString &name, const QString &path) {
        if (environment.value(name).isEmpty())
            environment.insert(name, QDir::toNativeSeparators(path));
    };
    const auto certificates = root.filePath("ca-bundle.crt");
    if (QFileInfo(certificates).isFile()) {
        useDefault("SSL_CERT_FILE", certificates);
        useDefault("CURL_CA_BUNDLE", certificates);
    }
    const auto fonts = root.filePath("etc/fonts/fonts.conf");
    if (QFileInfo(fonts).isFile()) {
        useDefault("FONTCONFIG_FILE", fonts);
        useDefault("FONTCONFIG_PATH", root.filePath("etc/fonts"));
    }
    return environment;
}

#ifdef Q_OS_WIN
inline bool setRuntimeEnvironmentVariable(const QString &name, const QString &value) {
    return _wputenv_s(reinterpret_cast<const wchar_t *>(name.utf16()),
        reinterpret_cast<const wchar_t *>(value.utf16())) == 0;
}
#endif

} // namespace CloudStream
