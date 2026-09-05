#include <QtTest>
#include "../providers/HomeProcessResult.h"
using CloudStream::HomeProcessResult;
class HomeProcessResultTest : public QObject {
    Q_OBJECT
private slots:
    void processFailuresAreNotEmptyHome_data() {
        QTest::addColumn<int>("exitCode");
        QTest::addColumn<int>("exitStatus");
        QTest::addColumn<bool>("startFailure");
        QTest::addColumn<bool>("timedOut");
        QTest::addColumn<QString>("expected");
        QTest::newRow("exit") << 1 << int(QProcess::NormalExit) << false << false << "exit 1";
        QTest::newRow("crash") << 0 << int(QProcess::CrashExit) << false << false << "crashed";
        QTest::newRow("timeout") << 9 << int(QProcess::CrashExit) << false << true << "timed out";
        QTest::newRow("start") << -1 << int(QProcess::NormalExit) << true << false << "missing executable";
    }
    void processFailuresAreNotEmptyHome() {
        QFETCH(int, exitCode); QFETCH(int, exitStatus);
        QFETCH(bool, startFailure); QFETCH(bool, timedOut); QFETCH(QString, expected);
        auto result = HomeProcessResult::parse("[]", "TLS handshake failed", exitCode,
            QProcess::ExitStatus(exitStatus), startFailure, timedOut, "missing executable");
        QVERIFY2(result.error.contains(expected), qPrintable(result.error));
        QVERIFY(result.error.contains("TLS handshake failed"));
        QVERIFY(result.sections.isEmpty());
    }
    void genuineEmptyHomeIsNotAnError() {
        const auto result = HomeProcessResult::parse("[]", "log", 0, QProcess::NormalExit, false, false, {});
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.sections.isEmpty());
    }
    void preservesUtf8Home() {
        const auto result = HomeProcessResult::parse(QString::fromUtf8("[{\"name\":\"Zufälliger Anime — 日本語\"}]").toUtf8(), {}, 0, QProcess::NormalExit, false, false, {});
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.sections.at(0).toObject().value("name").toString(), QString::fromUtf8("Zufälliger Anime — 日本語"));
    }
    void rejectsWrongJsonShape() {
        QVERIFY(!HomeProcessResult::parse("{}", {}, 0, QProcess::NormalExit, false, false, {}).error.isEmpty());
    }
    void invalidWindowsEncodingIsNotEmptyHome() {
        auto result = HomeProcessResult::parse(QByteArray("[{\"name\":\"Zuf") + char(0xe4) + "lliger Anime\"}]", {}, 0, QProcess::NormalExit, false, false, {});
        QVERIFY(result.error.contains("JSON"));
        QVERIFY(result.sections.isEmpty());
    }
};
QTEST_GUILESS_MAIN(HomeProcessResultTest)
#include "test_home_process_result.moc"
