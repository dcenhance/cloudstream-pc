#include "MpvIpcProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace CloudStream {

QByteArray MpvIpcProtocol::progressQuery() {
    return QByteArrayLiteral("{\"command\":[\"get_property\",\"time-pos\"],\"request_id\":\"position\"}\n"
                             "{\"command\":[\"get_property\",\"duration\"],\"request_id\":\"duration\"}\n");
}

MpvProperty MpvIpcProtocol::parseResponse(const QByteArray &line) {
    const auto document = QJsonDocument::fromJson(line);
    if (!document.isObject()) return {};
    const auto object = document.object();
    if (object.value("error").toString() != "success" || !object.value("data").isDouble()) return {};
    MpvProperty result;
    const auto request = object.value("request_id").toString();
    if (request == "position") result.kind = MpvProperty::Position;
    else if (request == "duration") result.kind = MpvProperty::Duration;
    else return {};
    result.value = object.value("data").toDouble();
    result.valid = true;
    return result;
}

} // namespace CloudStream
