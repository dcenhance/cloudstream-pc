#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace CloudStream {

// Keep program and arguments separate: provider data is not shell input.
struct ProviderHostCommand {
    QString program;
    QStringList prefixArguments;

    bool isEmpty() const { return program.isEmpty(); }

    void configure(QProcess *process, const QStringList &arguments) const {
        process->setProgram(program);
        process->setArguments(prefixArguments + arguments);
    }

    static ProviderHostCommand discover(const QString &appDir, const QString &overridePath,
                                        const QString &javaHome, const QStringList &pathDirs,
                                        bool windows) {
        const auto filePath = [](const QString &path) {
            return !path.isEmpty() && QFileInfo(path).isFile()
                ? QFileInfo(path).canonicalFilePath() : QString();
        };
        const auto onPath = [&](const QString &name) {
            for (const auto &directory : pathDirs) {
                if (directory.isEmpty()) continue;
                const auto path = filePath(QDir(directory).filePath(name));
                if (!path.isEmpty() && (windows || QFileInfo(path).isExecutable())) return path;
            }
            return QString();
        };
        QString explicitHost;
        if (!overridePath.isEmpty()) {
            explicitHost = filePath(overridePath);
            if (explicitHost.isEmpty() && !overridePath.contains('/') && !overridePath.contains('\\'))
                explicitHost = onPath(overridePath);
            // An explicit override is authoritative, never silently use another host.
            if (explicitHost.isEmpty()) return {};
            const auto suffix = QFileInfo(explicitHost).suffix().toLower();
            if (!windows || (suffix != "bat" && suffix != "cmd")) return {explicitHost, {}};
        }
        if (!windows) {
            const QStringList candidates = {
                appDir + "/../libexec/cloudstream/provider-host/bin/cloudstream-provider-host",
                appDir + "/../../provider-host/build/install/cloudstream-provider-host/bin/cloudstream-provider-host",
                onPath("cloudstream-provider-host")};
            for (const auto &candidate : candidates) {
                const auto path = filePath(candidate);
                if (!path.isEmpty()) return {path, {}};
            }
            return {};
        }

        QString java = filePath(appDir + "/runtime/bin/java.exe");
        if (java.isEmpty() && !javaHome.isEmpty()) java = filePath(QDir(javaHome).filePath("bin/java.exe"));
        if (java.isEmpty()) java = onPath("java.exe");
        if (java.isEmpty()) return {};

        // Gradle's batch launcher only supplies this classpath and main class.
        // Never send JSON, URLs, or user search text through cmd.exe.
        QStringList roots;
        if (!explicitHost.isEmpty()) {
            roots << QFileInfo(explicitHost).absolutePath() + "/..";
        } else {
            roots << appDir + "/provider-host" << appDir + "/../provider-host"
                  << appDir + "/../libexec/cloudstream/provider-host"
                  << appDir + "/../../provider-host/build/install/cloudstream-provider-host";
            const auto batch = onPath("cloudstream-provider-host.bat");
            if (!batch.isEmpty()) roots << QFileInfo(batch).absolutePath() + "/..";
        }
        for (const auto &root : roots) {
            const auto lib = QDir::cleanPath(root + "/lib");
            if (!QDir(lib).entryList({"*.jar"}, QDir::Files).isEmpty())
                return {java, {"-cp", lib + "/*", "com.lagradost.cloudstream3.linux.host.MainKt"}};
        }
        return {};
    }

    static ProviderHostCommand discover() {
        return discover(QCoreApplication::applicationDirPath(),
                        qEnvironmentVariable("CLOUDSTREAM_PROVIDER_HOST"),
                        qEnvironmentVariable("JAVA_HOME"),
                        qEnvironmentVariable("PATH").split(QDir::listSeparator(), Qt::SkipEmptyParts),
#ifdef Q_OS_WIN
                        true
#else
                        false
#endif
        );
    }
};

} // namespace CloudStream
