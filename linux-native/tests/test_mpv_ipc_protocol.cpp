#include "../player/MpvIpcProtocol.h"

#include <QtTest>

class MpvIpcProtocolTest : public QObject {
    Q_OBJECT

private slots:
    void buildsProgressQueries() {
        const auto query = CloudStream::MpvIpcProtocol::progressQuery();
        const auto lines = query.split('\n');
        QCOMPARE(lines.size(), 3);
        QVERIFY(lines[0].contains("get_property"));
        QVERIFY(lines[0].contains("time-pos"));
        QVERIFY(lines[0].contains("position"));
        QVERIFY(lines[1].contains("duration"));
    }

    void parsesProgressResponsesAndIgnoresOtherMessages() {
        const auto position = CloudStream::MpvIpcProtocol::parseResponse(
            R"({"data":42.5,"request_id":"position","error":"success"})");
        QVERIFY(position.valid);
        QCOMPARE(position.kind, CloudStream::MpvProperty::Position);
        QCOMPARE(position.value, 42.5);

        const auto duration = CloudStream::MpvIpcProtocol::parseResponse(
            R"({"data":120.0,"request_id":"duration","error":"success"})");
        QVERIFY(duration.valid);
        QCOMPARE(duration.kind, CloudStream::MpvProperty::Duration);
        QCOMPARE(duration.value, 120.0);

        QVERIFY(!CloudStream::MpvIpcProtocol::parseResponse(
            R"({"event":"property-change","name":"pause","data":true})").valid);
        QVERIFY(!CloudStream::MpvIpcProtocol::parseResponse(
            R"({"data":null,"request_id":"position","error":"property unavailable"})").valid);
    }
};

QTEST_APPLESS_MAIN(MpvIpcProtocolTest)
#include "test_mpv_ipc_protocol.moc"
