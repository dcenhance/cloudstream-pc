#include "../app/ProcessCompletion.h"
#include "../app/ProviderHostCommand.h"

#include <QtTest>

#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

class ProcessCompletionTest final : public QObject {
    Q_OBJECT

private slots:
    void windowsProviderUsesJavaAndLiteralArguments() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto app = temporary.filePath("App with spaces & unicode é");
        QVERIFY(QDir().mkpath(app + "/runtime/bin"));
        QVERIFY(QDir().mkpath(app + "/provider-host/lib"));
        for (const auto &path : {app + "/runtime/bin/java.exe", app + "/provider-host/lib/host.jar"}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        const auto command = CloudStream::ProviderHostCommand::discover(app, {}, {}, {}, true);
        QCOMPARE(command.program, app + "/runtime/bin/java.exe");
        QProcess process;
        const QStringList arguments = {"sources", "C:/Extensions with spaces/a.jar", "auto", "Provider é",
            "{\"url\":\"https://example.test/?a=1&b=%PATH%\",\"q\":\"quoted\\\"value\"}"};
        command.configure(&process, arguments);
        QCOMPARE(process.program(), command.program);
        QCOMPARE(process.arguments(), QStringList({"-cp", app + "/provider-host/lib/*",
            "com.lagradost.cloudstream3.linux.host.MainKt"}) + arguments);
    }

    void providerDiscovery_data() {
        QTest::addColumn<bool>("windows");
        QTest::addColumn<QStringList>("files");
        QTest::addColumn<QString>("overridePath");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<QString>("lib");
        QTest::newRow("java-home") << true << QStringList{"jdk/bin/java.exe", "app/provider-host/lib/host.jar"} << "" << "jdk/bin/java.exe" << "app/provider-host/lib";
        QTest::newRow("path-java") << true << QStringList{"path/java.exe", "app/provider-host/lib/host.jar"} << "" << "path/java.exe" << "app/provider-host/lib";
        QTest::newRow("bundled-first") << true << QStringList{"app/runtime/bin/java.exe", "jdk/bin/java.exe", "path/java.exe", "app/provider-host/lib/host.jar"} << "" << "app/runtime/bin/java.exe" << "app/provider-host/lib";
        QTest::newRow("explicit-exe") << true << QStringList{"custom/host.exe", "app/runtime/bin/java.exe", "app/provider-host/lib/host.jar"} << "custom/host.exe" << "custom/host.exe" << "";
        QTest::newRow("explicit-path-exe") << true << QStringList{"path/host.exe"} << "@host.exe" << "path/host.exe" << "";
        QTest::newRow("explicit-batch-distribution") << true << QStringList{"custom/bin/host.bat", "custom/lib/host.jar", "jdk/bin/java.exe"} << "custom/bin/host.bat" << "jdk/bin/java.exe" << "custom/lib";
        QTest::newRow("explicit-cmd-no-shell") << true << QStringList{"custom/host.cmd"} << "custom/host.cmd" << "" << "";
        QTest::newRow("invalid-override-no-fallback") << true << QStringList{"app/runtime/bin/java.exe", "app/provider-host/lib/host.jar"} << "missing.exe" << "" << "";
        QTest::newRow("missing-java") << true << QStringList{"app/provider-host/lib/host.jar"} << "" << "" << "";
        QTest::newRow("missing-jars") << true << QStringList{"app/runtime/bin/java.exe"} << "" << "" << "";
        QTest::newRow("parent-distribution") << true << QStringList{"app/runtime/bin/java.exe", "provider-host/lib/host.jar"} << "" << "app/runtime/bin/java.exe" << "provider-host/lib";
        QTest::newRow("linux-packaged") << false << QStringList{"libexec/cloudstream/provider-host/bin/cloudstream-provider-host"} << "" << "libexec/cloudstream/provider-host/bin/cloudstream-provider-host" << "";
#ifdef Q_OS_UNIX
        QTest::newRow("linux-path") << false << QStringList{"path/cloudstream-provider-host"} << "" << "path/cloudstream-provider-host" << "";
#endif
        QTest::newRow("linux-override") << false << QStringList{"custom/host"} << "custom/host" << "custom/host" << "";
    }

    void providerDiscovery() {
        QFETCH(bool, windows);
        QFETCH(QStringList, files);
        QFETCH(QString, overridePath);
        QFETCH(QString, expected);
        QFETCH(QString, lib);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        for (const auto &relative : files) {
            const auto path = temporary.filePath(relative);
            QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.close();
            QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        }
        const auto overrideValue = overridePath.startsWith('@') ? overridePath.mid(1)
            : (overridePath.isEmpty() ? QString() : temporary.filePath(overridePath));
        QVERIFY(QDir().mkpath(temporary.filePath("app")));
        const auto command = CloudStream::ProviderHostCommand::discover(temporary.filePath("app"),
            overrideValue, temporary.filePath("jdk"), {temporary.filePath("path")}, windows);
        QCOMPARE(command.program, expected.isEmpty() ? QString() : temporary.filePath(expected));
        QCOMPARE(command.prefixArguments, lib.isEmpty() ? QStringList() : QStringList({"-cp",
            temporary.filePath(lib) + "/*", "com.lagradost.cloudstream3.linux.host.MainKt"}));
    }

    void completesExactlyOnceWhenProgramCannotStart() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath("not-executable");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not executable\n");
        file.close();
        QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));

        QProcess process;
        int callbacks = 0;
        bool failedToStart = false;
        CloudStream::ProcessCompletion::watch(
            &process, this,
            [&](int, QProcess::ExitStatus, bool startFailure) {
                ++callbacks;
                failedToStart = startFailure;
            });
        process.setProgram(path);
        process.start();

        QTRY_COMPARE_WITH_TIMEOUT(callbacks, 1, 1000);
        QVERIFY(failedToStart);
        QTest::qWait(50);
        QCOMPARE(callbacks, 1);
    }

    void completesExactlyOnceAfterNormalExit() {
        QProcess process;
        int callbacks = 0;
        bool failedToStart = true;
        CloudStream::ProcessCompletion::watch(
            &process, this,
            [&](int, QProcess::ExitStatus, bool startFailure) {
                ++callbacks;
                failedToStart = startFailure;
            });
#ifdef Q_OS_WIN
        process.setProgram(qEnvironmentVariable("COMSPEC", "cmd.exe"));
        process.setArguments({"/d", "/c", "exit", "0"});
#else
        process.setProgram("/usr/bin/true");
#endif
        process.start();

        QTRY_COMPARE_WITH_TIMEOUT(callbacks, 1, 1000);
        QVERIFY(!failedToStart);
        QTest::qWait(50);
        QCOMPARE(callbacks, 1);
    }
};

QTEST_MAIN(ProcessCompletionTest)
#include "test_process_completion.moc"
