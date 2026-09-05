#include <QtTest>

#include "../extensions/ExtensionInstallBatch.h"

#include <QFile>
#include <QTemporaryDir>

class ExtensionInstallBatchTest final : public QObject {
    Q_OBJECT
private slots:
    void selectsMissingAndUpdatesButSkipsCurrentAndUnusableEntries() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto currentPath = directory.filePath("Current.jar");
        const auto oldPath = directory.filePath("Old.jar");
        QFile current(currentPath);
        QVERIFY(current.open(QIODevice::WriteOnly));
        current.write("current");
        current.close();
        QFile old(oldPath);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write("old");
        old.close();

        const QString repository = "https://example.org/repo.json";
        CloudStream::ExtensionRecord installedCurrent;
        installedCurrent.internalName = "Current";
        installedCurrent.repositoryUrl = repository;
        installedCurrent.artifactPath = currentPath;
        installedCurrent.version = 3;
        CloudStream::ExtensionRecord installedOld;
        installedOld.internalName = "Update";
        installedOld.repositoryUrl = repository;
        installedOld.artifactPath = oldPath;
        installedOld.version = 1;

        CloudStream::PluginInfo currentPlugin;
        currentPlugin.internalName = "Current";
        currentPlugin.url = "https://example.org/Current.cs3";
        currentPlugin.version = 3;
        CloudStream::PluginInfo newPlugin;
        newPlugin.internalName = "New";
        newPlugin.jarUrl = "https://example.org/New.jar";
        newPlugin.version = 1;
        CloudStream::PluginInfo updatePlugin;
        updatePlugin.internalName = "Update";
        updatePlugin.url = "https://example.org/Update.cs3";
        updatePlugin.version = 2;
        CloudStream::PluginInfo unusable;
        unusable.internalName = "NoArtifact";
        unusable.version = 1;

        const auto pending = CloudStream::ExtensionInstallBatch::pending(
            {currentPlugin, newPlugin, updatePlugin, newPlugin, unusable},
            {installedCurrent, installedOld}, repository);
        QCOMPARE(pending.size(), 2);
        QCOMPARE(pending[0].internalName, QString("New"));
        QCOMPARE(pending[1].internalName, QString("Update"));
    }
};

QTEST_APPLESS_MAIN(ExtensionInstallBatchTest)
#include "test_extension_install_batch.moc"
