#pragma once

#include <QByteArray>

namespace CloudStream {

struct MpvProperty {
    enum Kind {
        Unknown,
        Position,
        Duration,
    };

    Kind kind = Unknown;
    double value = 0.0;
    bool valid = false;
};

class MpvIpcProtocol final {
public:
    static QByteArray progressQuery();
    static MpvProperty parseResponse(const QByteArray &line);
};

} // namespace CloudStream
